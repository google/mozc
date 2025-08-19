// Copyright 2010-2021, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "dictionary/user_dictionary.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/hash/hash.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "base/file_util.h"
#include "base/hash.h"
#include "base/strings/assign.h"
#include "base/strings/japanese.h"
#include "base/strings/unicode.h"
#include "base/thread.h"
#include "base/vlog.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/user_dictionary_storage.h"
#include "dictionary/user_dictionary_util.h"
#include "dictionary/user_pos.h"
#include "protocol/config.pb.h"
#include "protocol/user_dictionary_storage.pb.h"
#include "request/conversion_request.h"

namespace mozc {
namespace dictionary {
namespace {

struct OrderByKey {
  bool operator()(const UserPos::Token& token, absl::string_view key) const {
    return token.key < key;
  }

  bool operator()(absl::string_view key, const UserPos::Token& token) const {
    return key < token.key;
  }
};

struct OrderByKeyPrefix {
  bool operator()(const UserPos::Token& token, absl::string_view prefix) const {
    return absl::string_view(token.key).substr(0, prefix.size()) < prefix;
  }

  bool operator()(absl::string_view prefix, const UserPos::Token& token) const {
    return prefix < absl::string_view(token.key).substr(0, prefix.size());
  }
};

struct OrderByKeyThenById {
  bool operator()(const UserPos::Token& lhs, const UserPos::Token& rhs) const {
    const int comp = lhs.key.compare(rhs.key);
    return comp == 0 ? (lhs.id < rhs.id) : (comp < 0);
  }
};

class SuppressionDictionary {
 public:
  bool AddEntry(std::string key, std::string value) {
    if (key.empty() && value.empty()) {
      LOG(WARNING) << "Both key and value are empty";
      return false;
    }

    if (key.empty()) {
      values_only_.emplace(std::move(value));
    } else if (value.empty()) {
      keys_only_.emplace(std::move(key));
    } else {
      keys_values_.emplace(std::move(key), std::move(value));
    }

    return true;
  }

  bool IsEmpty() const {
    return keys_only_.empty() && values_only_.empty() && keys_values_.empty();
  }

  bool IsSuppressedEntry(const absl::string_view key,
                         const absl::string_view value) const {
    return !IsEmpty() &&
           (keys_values_.contains(std::make_pair(key, value)) ||
            keys_only_.contains(key) || values_only_.contains(value));
  }

 private:
  using KeyValue = std::pair<std::string, std::string>;
  using KeyValueView = std::pair<absl::string_view, absl::string_view>;
  struct KeyValueHash : public absl::Hash<KeyValueView> {
    using is_transparent = void;
  };
  struct KeyValueEq : public std::equal_to<KeyValueView> {
    using is_transparent = void;
  };

  absl::flat_hash_set<KeyValue, KeyValueHash, KeyValueEq> keys_values_;
  absl::flat_hash_set<std::string> keys_only_;
  absl::flat_hash_set<std::string> values_only_;
};
}  // namespace

class UserDictionary::TokensIndex {
 public:
  explicit TokensIndex(const UserPos& user_pos) : user_pos_(user_pos) {}

  ~TokensIndex() = default;

  bool empty() const { return user_pos_tokens_.empty(); }
  size_t size() const { return user_pos_tokens_.size(); }

  std::vector<UserPos::Token>::const_iterator begin() const {
    return user_pos_tokens_.begin();
  }
  std::vector<UserPos::Token>::const_iterator end() const {
    return user_pos_tokens_.end();
  }

  void Load(const user_dictionary::UserDictionaryStorage& storage,
            std::atomic<bool>* canceled_signal) {
    DCHECK(canceled_signal);
    user_pos_tokens_.clear();
    absl::flat_hash_set<uint64_t> seen;
    std::vector<UserPos::Token> tokens;

    for (const UserDictionaryStorage::UserDictionary& dic :
         storage.dictionaries()) {
      if (dic.entries_size() == 0) {
        continue;
      }
      const bool is_android_shortcuts =
          (dic.name() == "__auto_imported_android_shortcuts_dictionary");

      for (const UserDictionaryStorage::UserDictionaryEntry& entry :
           dic.entries()) {
        if (!UserDictionaryUtil::IsValidEntry(user_pos_, entry)) {
          continue;
        }
        if (canceled_signal->load()) {
          LOG(INFO) << "User dictionary loading is canceled";
          return;
        }

        // We cannot call NormalizeVoiceSoundMark inside NormalizeReading,
        // because the normalization is user-visible.
        // http://b/2480844
        std::string reading = japanese::NormalizeVoicedSoundMark(
            UserDictionaryUtil::NormalizeReading(entry.key()));

        DCHECK(user_dictionary::UserDictionary_PosType_IsValid(entry.pos()));
        static_assert(user_dictionary::UserDictionary_PosType_PosType_MAX <=
                      std::numeric_limits<char>::max());
        const char pos_type_as_char[] = {static_cast<char>(entry.pos())};
        const uint64_t fp =
            Fingerprint(absl::StrCat(reading, "\t", entry.value(), "\t",
                                     absl::string_view(pos_type_as_char, 1)));
        if (!seen.insert(fp).second) {
          MOZC_VLOG(1) << "Found dup item";
          continue;
        }

        if (entry.pos() == user_dictionary::UserDictionary::SUPPRESSION_WORD) {
          // "抑制単語"
          suppression_dictionary_.AddEntry(std::move(reading), entry.value());
        } else if (entry.pos() == user_dictionary::UserDictionary::NO_POS) {
          // In theory NO_POS works without this implementation, as it is
          // covered in the UserPos::GetTokens function. However, that function
          // is depending on the user_pos_*.data in the dictionary and there
          // will not be corresponding POS tag. To avoid invalid behavior, this
          // special treatment is added here.
          // "品詞なし"
          const absl::string_view comment =
              absl::StripAsciiWhitespace(entry.comment());
          UserPos::Token token{.key = reading,
                               .value = entry.value(),
                               .id = 0,
                               .attributes = UserPos::Token::SHORTCUT,
                               .comment = std::string(comment)};
          // NO_POS has '名詞サ変' id as in user_pos.def
          user_pos_.GetPosIds("名詞サ変", &token.id);
          user_pos_tokens_.push_back(std::move(token));
        } else {
          tokens.clear();
          user_pos_.GetTokens(reading, entry.value(),
                              UserDictionaryUtil::GetStringPosType(entry.pos()),
                              &tokens);
          const absl::string_view comment =
              absl::StripAsciiWhitespace(entry.comment());
          for (auto& token : tokens) {
            strings::Assign(token.comment, comment);
            if (is_android_shortcuts &&
                token.has_attribute(UserPos::Token::SUGGESTION_ONLY)) {
              // TODO(b/295964970): This special implementation is planned to be
              // removed after validating the safety of NO_POS implementation.
              token.remove_attribute(UserPos::Token::SUGGESTION_ONLY);
              token.add_attribute(UserPos::Token::SHORTCUT);
            }
            user_pos_tokens_.push_back(std::move(token));
          }
        }
      }
    }
    user_pos_tokens_.shrink_to_fit();

    // Sort first by key and then by POS ID.
    std::sort(user_pos_tokens_.begin(), user_pos_tokens_.end(),
              OrderByKeyThenById());

    MOZC_VLOG(1) << user_pos_tokens_.size() << " user dic entries loaded";
  }

  bool IsSuppressedEntry(absl::string_view key, absl::string_view value) const {
    return suppression_dictionary_.IsSuppressedEntry(key, value);
  }

  bool HasSuppressedEntries() const {
    return !suppression_dictionary_.IsEmpty();
  }

 private:
  const UserPos& user_pos_;
  SuppressionDictionary suppression_dictionary_;
  std::vector<UserPos::Token> user_pos_tokens_;
};

class UserDictionary::UserDictionaryReloader {
 public:
  explicit UserDictionaryReloader(UserDictionary* dic)
      : modified_at_(0), dic_(dic) {
    DCHECK(dic_);
  }

  UserDictionaryReloader(const UserDictionaryReloader&) = delete;
  UserDictionaryReloader& operator=(const UserDictionaryReloader&) = delete;

  ~UserDictionaryReloader() { Wait(); }

  // When the user dictionary exists AND the modification time has been updated,
  // reloads the dictionary.  Returns true when reloader thread is started.
  bool MaybeStartReload() {
    if (reload_.has_value() && !reload_->Ready()) {
      // Previously started reload is still running.
      // TODO(tomokinat): test this path.
      return false;
    }

    absl::StatusOr<FileTimeStamp> modification_time =
        FileUtil::GetModificationTime(dic_->GetFileName());
    if (!modification_time.ok()) {
      // If the file doesn't exist, return doing nothing.
      // Therefore if the file is deleted after first reload,
      // second reload does nothing so the content loaded by first reload
      // is kept as is.
      LOG(WARNING) << "Cannot get modification time of the user dictionary: "
                   << modification_time.status();
      return false;
    }
    if (modified_at_ == *modification_time) {
      return false;
    }
    modified_at_ = *modification_time;
    // Runs `ThreadMain()` in a background thread.
    reload_.emplace([this] { ThreadMain(); });
    return true;
  }

  void Wait() {
    if (reload_.has_value()) {
      reload_->Wait();
    }
  }

 private:
  void ThreadMain() {
    UserDictionaryStorage storage(dic_->GetFileName());

    // Load from file
    if (absl::Status s = storage.Load(); !s.ok()) {
      LOG(ERROR) << "Failed to load the user dictionary: " << s;
      return;
    }

    dic_->Load(storage.GetProto());
  }

  std::optional<BackgroundFuture<void>> reload_;
  FileTimeStamp modified_at_;
  UserDictionary* dic_ = nullptr;
};

UserDictionary::UserDictionary(std::unique_ptr<const UserPos> user_pos,
                               PosMatcher pos_matcher)
    : UserDictionary::UserDictionary(
          std::move(user_pos), std::move(pos_matcher),
          UserDictionaryUtil::GetUserDictionaryFileName()) {}

UserDictionary::UserDictionary(std::unique_ptr<const UserPos> user_pos,
                               PosMatcher pos_matcher, std::string filename)
    : reloader_(std::make_unique<UserDictionaryReloader>(this)),
      user_pos_(std::move(user_pos)),
      pos_matcher_(pos_matcher),
      tokens_(std::make_shared<TokensIndex>(*user_pos_)),
      filename_(std::move(filename)) {
  DCHECK(user_pos_);
  DCHECK(!canceled_signal_);
  DCHECK(!filename_.empty());
  Reload();
}

UserDictionary::~UserDictionary() {
  canceled_signal_.store(true);  // force to finish the thread.
  WaitForReloader();
}

bool UserDictionary::HasKey(absl::string_view key) const {
  // TODO(noriyukit): Currently, we don't support HasKey() for user dictionary
  // because we need to search tokens linearly, which might be slow in extreme
  // cases where 100K entries exist.
  return false;
}

bool UserDictionary::HasValue(absl::string_view value) const {
  // TODO(noriyukit): Currently, we don't support HasValue() for user dictionary
  // because we need to search tokens linearly, which might be slow in extreme
  // cases where 100K entries exist.  Note: HasValue() method is used only in
  // UserHistoryPredictor for privacy sensitivity check.
  return false;
}

void UserDictionary::LookupPredictive(
    absl::string_view key, const ConversionRequest& conversion_request,
    Callback* callback) const {
  if (key.empty()) {
    MOZC_VLOG(2) << "string of length zero is passed.";
    return;
  }

  std::shared_ptr<const TokensIndex> tokens = GetTokens();

  if (tokens->empty()) {
    return;
  }
  if (conversion_request.incognito_mode()) {
    return;
  }

  // Find the starting point of iteration over dictionary contents.
  Token token;
  for (auto [begin, end] = std::equal_range(tokens->begin(), tokens->end(), key,
                                            OrderByKeyPrefix());
       begin != end; ++begin) {
    const UserPos::Token& user_pos_token = *begin;
    switch (callback->OnKey(user_pos_token.key)) {
      case Callback::TRAVERSE_DONE:
        return;
      case Callback::TRAVERSE_NEXT_KEY:
      case Callback::TRAVERSE_CULL:
        continue;
      default:
        break;
    }
    // b/333613472: Make sure not to set the additional penalties.
    if (callback->OnActualKey(user_pos_token.key, user_pos_token.key,
                              /* num_expanded= */ 0) ==
        Callback::TRAVERSE_DONE) {
      return;
    }
    PopulateTokenFromUserPosToken(user_pos_token, PREDICTIVE, &token);
    if (callback->OnToken(user_pos_token.key, user_pos_token.key, token) ==
        Callback::TRAVERSE_DONE) {
      return;
    }
  }
}

// UserDictionary doesn't support kana modifier insensitive lookup.
void UserDictionary::LookupPrefix(absl::string_view key,
                                  const ConversionRequest& conversion_request,
                                  Callback* callback) const {
  if (key.empty()) {
    LOG(WARNING) << "string of length zero is passed.";
    return;
  }

  if (conversion_request.incognito_mode()) {
    return;
  }

  std::shared_ptr<const TokensIndex> tokens = GetTokens();

  if (tokens->empty()) {
    return;
  }

  // Find the starting point for iteration over dictionary contents.
  const absl::string_view first_char = Utf8AsChars(key).front();
  Token token;
  for (auto it = std::lower_bound(tokens->begin(), tokens->end(), first_char,
                                  OrderByKey());
       it != tokens->end(); ++it) {
    const UserPos::Token& user_pos_token = *it;
    if (user_pos_token.key > key) {
      break;
    }
    if (user_pos_token.has_attribute(UserPos::Token::SUGGESTION_ONLY)) {
      continue;
    }
    if (!key.starts_with(user_pos_token.key)) {
      continue;
    }
    switch (callback->OnKey(user_pos_token.key)) {
      case Callback::TRAVERSE_DONE:
        return;
      case Callback::TRAVERSE_NEXT_KEY:
        continue;
      case Callback::TRAVERSE_CULL:
        LOG(FATAL) << "UserDictionary doesn't support culling.";
        break;
      default:
        break;
    }
    if (callback->OnActualKey(user_pos_token.key, user_pos_token.key,
                              /* num_expanded= */ 0) ==
        Callback::TRAVERSE_DONE) {
      return;
    }
    PopulateTokenFromUserPosToken(user_pos_token, PREFIX, &token);
    switch (callback->OnToken(user_pos_token.key, user_pos_token.key, token)) {
      case Callback::TRAVERSE_DONE:
        return;
      case Callback::TRAVERSE_CULL:
        LOG(FATAL) << "UserDictionary doesn't support culling.";
        break;
      default:
        break;
    }
  }
}

void UserDictionary::LookupExact(absl::string_view key,
                                 const ConversionRequest& conversion_request,
                                 Callback* callback) const {
  std::shared_ptr<const TokensIndex> tokens = GetTokens();

  if (key.empty() || tokens->empty() || conversion_request.incognito_mode()) {
    return;
  }
  auto [begin, end] =
      std::equal_range(tokens->begin(), tokens->end(), key, OrderByKey());
  if (begin == end) {
    return;
  }
  if (callback->OnKey(key) != Callback::TRAVERSE_CONTINUE) {
    return;
  }
  if (callback->OnActualKey(key, key, /* num_expanded= */ 0) !=
      Callback::TRAVERSE_CONTINUE) {
    return;
  }

  Token token;
  for (; begin != end; ++begin) {
    const UserPos::Token& user_pos_token = *begin;
    if (user_pos_token.has_attribute(UserPos::Token::SUGGESTION_ONLY)) {
      continue;
    }
    PopulateTokenFromUserPosToken(user_pos_token, EXACT, &token);
    if (callback->OnToken(key, key, token) != Callback::TRAVERSE_CONTINUE) {
      return;
    }
  }
}

void UserDictionary::LookupReverse(absl::string_view key,
                                   const ConversionRequest& conversion_request,
                                   Callback* callback) const {}

bool UserDictionary::LookupComment(absl::string_view key,
                                   absl::string_view value,
                                   const ConversionRequest& conversion_request,
                                   std::string* comment) const {
  if (key.empty() || conversion_request.incognito_mode()) {
    return false;
  }

  std::shared_ptr<const TokensIndex> tokens = GetTokens();

  if (tokens->empty()) {
    return false;
  }

  // Set the comment that was found first.
  for (auto [begin, end] =
           std::equal_range(tokens->begin(), tokens->end(), key, OrderByKey());
       begin != end; ++begin) {
    const UserPos::Token& token = *begin;
    if (token.value == value && !token.comment.empty()) {
      comment->assign(token.comment);
      return true;
    }
  }
  return false;
}

bool UserDictionary::IsSuppressedEntry(absl::string_view key,
                                       absl::string_view value) const {
  return GetTokens()->IsSuppressedEntry(key, value);
}

bool UserDictionary::HasSuppressedEntries() const {
  return GetTokens()->HasSuppressedEntries();
}

bool UserDictionary::Reload() {
  if (!reloader_->MaybeStartReload()) {
    LOG(INFO) << "MaybeStartReload() didn't start reloading";
  }
  return true;
}

void UserDictionary::WaitForReloader() { reloader_->Wait(); }

bool UserDictionary::Load(
    const user_dictionary::UserDictionaryStorage& storage) {
  const size_t size = GetTokens()->size();

  // If UserDictionary is pretty big, we first remove the
  // current dictionary to save memory usage.
#ifdef __ANDROID__
  constexpr size_t kVeryBigUserDictionarySize = 5000;
#else   // __ANDROID__
  constexpr size_t kVeryBigUserDictionarySize = 100000;
#endif  // __ANDROID__

  if (size >= kVeryBigUserDictionarySize) {
    auto placeholder_empty_tokens = std::make_shared<TokensIndex>(*user_pos_);
    SetTokens(std::move(placeholder_empty_tokens));
  }

  auto tokens = std::make_shared<TokensIndex>(*user_pos_);
  tokens->Load(storage, &canceled_signal_);

  SetTokens(tokens);
  return true;
}

std::vector<std::string> UserDictionary::GetPosList() const {
  return user_pos_->GetPosList();
}

std::string UserDictionary::GetFileName() const { return filename_; }

void UserDictionary::PopulateTokenFromUserPosToken(
    const UserPos::Token& user_pos_token, RequestType request_type,
    Token* token) const {
  token->key = user_pos_token.key;
  token->value = user_pos_token.value;
  token->lid = token->rid = user_pos_token.id;
  token->attributes = Token::USER_DICTIONARY;

  // * Overwrites POS ids.
  // Actual pos id of suggestion-only candidates are 名詞-サ変.
  // TODO(taku): We would like to change the POS to 名詞-サ変 in user-pos.def,
  // because SUGGEST_ONLY is not POS.
  if (user_pos_token.has_attribute(UserPos::Token::SUGGESTION_ONLY) ||
      user_pos_token.has_attribute(UserPos::Token::SHORTCUT)) {
    token->lid = token->rid = pos_matcher_.GetUnknownId();
  }

  // * Overwrites costs.
  // Locale is not Japanese.
  if (user_pos_token.has_attribute(UserPos::Token::NON_JA_LOCALE)) {
    token->cost = 10000;
  } else if (user_pos_token.has_attribute(UserPos::Token::ISOLATED_WORD)) {
    // Set smaller cost for "短縮よみ" in order to make
    // the rank of the word higher than others.
    token->cost = 200;
  } else {
    // default user dictionary cost.
    token->cost = 5000;
  }

  // The words added via Android shortcut have adaptive cost based
  // on the length of the key. Shorter keys have more penalty so that
  // they are not shown in the context.
  // TODO(taku): Better to apply this cost for all user defined words?
  if (user_pos_token.has_attribute(UserPos::Token::SHORTCUT) &&
      (request_type == PREFIX || request_type == EXACT)) {
    const int key_length = strings::AtLeastCharsLen(token->key, 4);
    token->cost += (4 - key_length) * 2000;
  }
}

}  // namespace dictionary
}  // namespace mozc
