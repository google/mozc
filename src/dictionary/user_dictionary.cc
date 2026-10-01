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
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ios>
#include <istream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "base/bits.h"
#include "base/container/serialized_string_array.h"
#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/hash.h"
#include "base/protobuf/coded_stream.h"
#include "base/protobuf/wire_format_lite.h"
#include "base/protobuf/zero_copy_stream_impl.h"
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
#include "protocol/user_dictionary_storage.pb.h"

namespace mozc {
namespace dictionary {
namespace {

// 512MByte
// We expand the limit of serialized message from 64MB(default) to 512MB.
constexpr size_t kDefaultTotalBytesLimit = 512 << 20;

}  // namespace

// Stores user dictionary tokens in a flat `SerializedStringArray` image
// (`tokens_`), sorted by `(key, id)`, where each element has the binary layout
// below. Suppression words (`SUPPRESSION_WORD`) are stored as 64-bit
// fingerprints in `suppression_set_`.
//
// Binary layout of each element in `tokens_`:
// +---------------------------------------+
// | id (uint16_t, 2 bytes)                |
// +---------------------------------------+
// | attributes (uint8_t, 1 byte)          |
// +---------------------------------------+
// | raw_pos_type (uint8_t, 1 byte)        |
// +---------------------------------------+
// | key_len (uint16_t, 2 bytes)           |
// +---------------------------------------+
// | value_len (uint16_t, 2 bytes)         |
// +---------------------------------------+
// | key (key_len bytes)                   |
// +---------------------------------------+
// | value (value_len bytes)               |
// +---------------------------------------+
// | comment (remaining bytes)             |
// +---------------------------------------+
class UserDictionary::TokensIndex {
 public:
  static constexpr size_t kTokenHeaderSize = 8;

  // Lightweight zero-copy view over a serialized token entry.
  class Token {
   public:
    explicit constexpr Token(absl::string_view data) : data_(data) {
      DCHECK_GE(data.size(), kTokenHeaderSize);
    }

    uint16_t id() const { return LoadUnaligned<uint16_t>(data_.data()); }
    uint8_t attributes() const { return static_cast<uint8_t>(data_[2]); }
    uint8_t raw_pos_type() const { return static_cast<uint8_t>(data_[3]); }
    uint16_t key_len() const {
      return LoadUnaligned<uint16_t>(data_.data() + 4);
    }
    uint16_t value_len() const {
      return LoadUnaligned<uint16_t>(data_.data() + 6);
    }

    absl::string_view key() const {
      return absl::string_view(data_.data() + kTokenHeaderSize, key_len());
    }
    absl::string_view value() const {
      return absl::string_view(data_.data() + kTokenHeaderSize + key_len(),
                               value_len());
    }
    absl::string_view comment() const {
      const size_t offset = kTokenHeaderSize + key_len() + value_len();
      return absl::string_view(data_.data() + offset, data_.size() - offset);
    }

    bool has_attribute(UserPos::Token::Attribute attr) const {
      return attributes() & attr;
    }
    user_dictionary::UserDictionary::PosType pos_type() const {
      return static_cast<::mozc::user_dictionary::UserDictionary::PosType>(
          raw_pos_type());
    }

   private:
    absl::string_view data_;
  };

  using const_iterator = SerializedStringArray::const_iterator;

  explicit TokensIndex(const UserPos& user_pos) : user_pos_(user_pos) {}

  ~TokensIndex() = default;

  bool empty() const { return tokens_.empty(); }
  size_t size() const { return tokens_.size(); }

  const_iterator begin() const { return tokens_.begin(); }
  const_iterator end() const { return tokens_.end(); }

  std::pair<const_iterator, const_iterator> EqualRange(
      absl::string_view key) const {
    return std::equal_range(begin(), end(), LookupKey{key}, OrderByKey());
  }

  std::pair<const_iterator, const_iterator> PrefixEqualRange(
      absl::string_view prefix) const {
    return std::equal_range(begin(), end(), LookupKey{prefix},
                            OrderByKeyPrefix());
  }

  const_iterator LowerBound(absl::string_view key) const {
    return std::lower_bound(begin(), end(), LookupKey{key}, OrderByKey());
  }

  void PopulateToken(const Token& user_pos_token,
                     UserDictionary::RequestType request_type,
                     PosMatcher pos_matcher,
                     mozc::dictionary::Token* token) const {
    strings::Assign(token->key, user_pos_token.key());
    strings::Assign(token->value, user_pos_token.value());
    token->lid = token->rid = user_pos_token.id();
    token->attributes = mozc::dictionary::Token::USER_DICTIONARY;

    // * Overwrites POS ids.
    // Actual pos id of suggestion-only candidates are 名詞-サ変.
    // TODO(taku): We would like to change the POS to 名詞-サ変 in user-pos.def,
    // because SUGGESTION_ONLY is not POS.
    if (user_pos_token.pos_type() ==
        user_dictionary::UserDictionary::SUGGESTION_ONLY) {
      token->lid = token->rid = pos_matcher.GetUnknownId();
    }

    // * Overwrites costs.
    // Locale is not Japanese.
    if (user_pos_token.has_attribute(UserPos::Token::NON_JA_LOCALE)) {
      token->cost = 10000;
    } else {
      token->cost =
          UserPos::GetCostFromPosType(user_pos_token.pos_type(), cost_penalty_);
      DCHECK_GT(token->cost, 0);
    }

    // The treatment for the words with default POS (NO_POS).
    // Shorter keys have more penalty so that they are not shown in the context.
    // TODO(taku): Better to apply this cost for all user defined words?
    if (user_pos_token.pos_type() == user_dictionary::UserDictionary::NO_POS &&
        (request_type == UserDictionary::PREFIX ||
         request_type == UserDictionary::EXACT)) {
      const int key_length = strings::AtLeastCharsLen(token->key, 4);
      token->cost += (4 - key_length) * 2000;
    }
  }

  bool Load(std::istream& ifs, std::atomic<bool>* canceled_signal) {
    DCHECK(canceled_signal);
    DCHECK(tokens_.empty());
    DCHECK(suppression_set_.empty());

    mozc::protobuf::io::IstreamInputStream zero_copy_input(&ifs);
    mozc::protobuf::io::CodedInputStream decoder(&zero_copy_input);
    decoder.SetTotalBytesLimit(kDefaultTotalBytesLimit);

    using WireFormatLite = ::mozc::protobuf::internal::WireFormatLite;

    constexpr uint32_t kDictionariesTag = WireFormatLite::MakeTag(
        user_dictionary::UserDictionaryStorage::kDictionariesFieldNumber,
        WireFormatLite::WIRETYPE_LENGTH_DELIMITED);
    constexpr uint32_t kEntriesTag = WireFormatLite::MakeTag(
        user_dictionary::UserDictionary::kEntriesFieldNumber,
        WireFormatLite::WIRETYPE_LENGTH_DELIMITED);

    std::vector<std::string> serialized_tokens;
    absl::flat_hash_set<uint64_t> seen;
    user_dictionary::UserDictionary::Entry entry;
    uint32_t len = 0;

    while (const uint32_t tag = decoder.ReadTag()) {
      if (canceled_signal->load()) {
        LOG(INFO) << "User dictionary loading is canceled";
        return true;
      }
      if (tag == kDictionariesTag) {
        // Read only the byte-length prefix of the UserDictionary submessage
        // (without skipping its payload) so subsequent ReadTag() calls in this
        // loop read the fields inside UserDictionary directly.
        if (!decoder.ReadVarint32(&len)) {
          return false;
        }
      } else if (tag == kEntriesTag) {
        if (!decoder.ReadVarint32(&len)) {
          return false;
        }
        const auto limit = decoder.PushLimit(len);
        entry.Clear();
        if (!entry.MergeFromCodedStream(&decoder) ||
            !decoder.ConsumedEntireMessage()) {
          return false;
        }
        decoder.PopLimit(limit);
        AddEntry(entry, &seen, &serialized_tokens);
      } else if (!WireFormatLite::SkipField(&decoder, tag)) {
        return false;
      }
    }
    if (!decoder.ConsumedEntireMessage() || !ifs.eof()) {
      return false;
    }

    if (!serialized_tokens.empty()) {
      absl::c_sort(serialized_tokens,
                   [](absl::string_view lhs, absl::string_view rhs) {
                     const Token lhs_token(lhs);
                     const Token rhs_token(rhs);
                     return std::make_pair(lhs_token.key(), lhs_token.id()) <
                            std::make_pair(rhs_token.key(), rhs_token.id());
                   });
      tokens_.Set(SerializedStringArray::SerializeToBuffer(serialized_tokens,
                                                           &tokens_buffer_));
    }

    // Adds cost penalty based on dictionary entry count N: 500 * log(N + 1).
    //
    // Background:
    // User dictionary entries default to static minimum costs defined in
    // user_pos.def. When users carelessly or unconsciously import large
    // 3rd-party dictionaries, low-quality entries over-trigger and overpower
    // standard system candidates.
    //
    // Theoretical Justification:
    // Assuming a uniform prior probability P = 1/N across N entries, the
    // self-information penalty in Mozc cost scaling (approx. 500 * (-ln P)) is
    // 500 * ln(N + 1). Using N + 1 generalizes to 0 penalty when N = 0.
    //
    // Cost penalty examples per N:
    // - N = 0: +0
    // - N = 1: +346
    // - N = 10: +1,198
    // - N = 100: +2,307
    // - N = 1,000: +3,454
    // - N = 10,000: +4,605
    // - N = 100,000: +5,756
    cost_penalty_ = static_cast<int>(500.0 * std::log(tokens_.size() + 1));

    MOZC_VLOG(1) << tokens_.size() << " user dic entries loaded";
    return true;
  }

  bool IsSuppressedEntry(absl::string_view key, absl::string_view value) const {
    if (!HasSuppressedEntries()) {
      return false;
    }
    return suppression_set_.contains(SuppressedEntryFingerprint(key, value));
  }

  bool HasSuppressedEntries() const { return !suppression_set_.empty(); }

  static std::string SerializeToken(const UserPos::Token& token,
                                    absl::string_view comment) {
    DCHECK_LE(token.key.size(), std::numeric_limits<uint16_t>::max());
    DCHECK_LE(token.value.size(), std::numeric_limits<uint16_t>::max());
    std::string buf(kTokenHeaderSize + token.key.size() + token.value.size() +
                        comment.size(),
                    '\0');
    char* ptr = buf.data();
    StoreUnaligned<uint16_t>(token.id, ptr);
    ptr[2] = static_cast<char>(token.attributes);
    ptr[3] = static_cast<char>(token.raw_pos_type);
    StoreUnaligned<uint16_t>(static_cast<uint16_t>(token.key.size()), ptr + 4);
    StoreUnaligned<uint16_t>(static_cast<uint16_t>(token.value.size()),
                             ptr + 6);
    ptr += kTokenHeaderSize;
    std::memcpy(ptr, token.key.data(), token.key.size());
    ptr += token.key.size();
    std::memcpy(ptr, token.value.data(), token.value.size());
    ptr += token.value.size();
    if (!comment.empty()) {
      std::memcpy(ptr, comment.data(), comment.size());
    }
    return buf;
  }

 private:
  void AddEntry(const user_dictionary::UserDictionary::Entry& entry,
                absl::flat_hash_set<uint64_t>* seen,
                std::vector<std::string>* serialized_tokens) {
    if (!user_dictionary::ValidateEntry(entry).ok()) {
      return;
    }

    // We cannot call NormalizeVoiceSoundMark inside NormalizeReading,
    // because the normalization is user-visible.
    // http://b/2480844
    const std::string reading = japanese::NormalizeVoicedSoundMark(
        user_dictionary::NormalizeReading(entry.key()));

    DCHECK(user_dictionary::UserDictionary_PosType_IsValid(entry.pos()));
    static_assert(user_dictionary::UserDictionary_PosType_PosType_MAX <=
                  std::numeric_limits<char>::max());
    const char pos_type_as_char[] = {static_cast<char>(entry.pos())};
    const uint64_t fp =
        CityFingerprint(absl::StrCat(reading, "\t", entry.value(), "\t",
                                     absl::string_view(pos_type_as_char, 1)));
    if (!seen->insert(fp).second) {
      MOZC_VLOG(1) << "Found dup item";
      return;
    }

    if (entry.pos() == user_dictionary::UserDictionary::SUPPRESSION_WORD) {
      // "抑制単語"
      suppression_set_.insert(
          SuppressedEntryFingerprint(reading, entry.value()));
    } else {
      const absl::string_view comment =
          absl::StripAsciiWhitespace(entry.comment());
      for (const auto& token :
           user_pos_.GetTokens(reading, entry.value(), entry.pos())) {
        serialized_tokens->push_back(SerializeToken(token, comment));
      }
    }
  }

  static uint64_t SuppressedEntryFingerprint(absl::string_view key,
                                             absl::string_view value) {
    return CityFingerprintWithSeed(value, CityFingerprint(key));
  }

  struct LookupKey {
    absl::string_view key;
  };

  struct OrderByKey {
    bool operator()(absl::string_view raw_token, LookupKey key) const {
      return Token(raw_token).key() < key.key;
    }

    bool operator()(LookupKey key, absl::string_view raw_token) const {
      return key.key < Token(raw_token).key();
    }
  };

  struct OrderByKeyPrefix {
    bool operator()(absl::string_view raw_token, LookupKey prefix) const {
      return Token(raw_token).key().substr(0, prefix.key.size()) < prefix.key;
    }

    bool operator()(LookupKey prefix, absl::string_view raw_token) const {
      return prefix.key < Token(raw_token).key().substr(0, prefix.key.size());
    }
  };

  const UserPos& user_pos_;
  SerializedStringArray tokens_;
  std::unique_ptr<uint32_t[]> tokens_buffer_;
  absl::flat_hash_set<uint64_t> suppression_set_;
  int cost_penalty_ = 0;
};

UserDictionary::UserDictionary(std::unique_ptr<const UserPos> user_pos,
                               PosMatcher pos_matcher)
    : UserDictionary::UserDictionary(
          std::move(user_pos), std::move(pos_matcher),
          UserDictionaryStorage::GetDefaultUserDictionaryFileName()) {}

UserDictionary::UserDictionary(std::unique_ptr<const UserPos> user_pos,
                               PosMatcher pos_matcher, std::string filename)
    : user_pos_(std::move(user_pos)),
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

void UserDictionary::LookupPredictive(absl::string_view key,
                                      Callback* callback) const {
  if (key.empty()) {
    MOZC_VLOG(2) << "string of length zero is passed.";
    return;
  }

  std::shared_ptr<const TokensIndex> tokens = GetTokens();

  if (tokens->empty()) {
    return;
  }

  // Find the starting point of iteration over dictionary contents.
  for (auto [begin, end] = tokens->PrefixEqualRange(key); begin != end;
       ++begin) {
    const TokensIndex::Token user_pos_token(*begin);
    const absl::string_view token_key = user_pos_token.key();
    switch (callback->OnKey(token_key)) {
      case Callback::TRAVERSE_DONE:
        return;
      case Callback::TRAVERSE_NEXT_KEY:
      case Callback::TRAVERSE_CULL:
        continue;
      default:
        break;
    }
    // b/333613472: Make sure not to set the additional penalties.
    if (callback->OnActualKey(token_key, token_key,
                              /* num_expanded= */ 0) ==
        Callback::TRAVERSE_DONE) {
      return;
    }
    Token token;
    tokens->PopulateToken(user_pos_token, PREDICTIVE, pos_matcher_, &token);
    if (callback->OnToken(token_key, token_key, std::move(token)) ==
        Callback::TRAVERSE_DONE) {
      return;
    }
  }
}

// UserDictionary doesn't support kana modifier insensitive lookup.
void UserDictionary::LookupPrefix(absl::string_view key,
                                  Callback* callback) const {
  if (key.empty()) {
    LOG(WARNING) << "string of length zero is passed.";
    return;
  }

  std::shared_ptr<const TokensIndex> tokens = GetTokens();

  if (tokens->empty()) {
    return;
  }

  // Find the starting point for iteration over dictionary contents.
  const absl::string_view first_char = Utf8AsChars(key).front();
  for (auto it = tokens->LowerBound(first_char); it != tokens->end(); ++it) {
    const TokensIndex::Token user_pos_token(*it);
    const absl::string_view token_key = user_pos_token.key();
    if (token_key > key) {
      break;
    }
    if (user_pos_token.pos_type() ==
        user_dictionary::UserDictionary::SUGGESTION_ONLY) {
      continue;
    }
    if (!key.starts_with(token_key)) {
      continue;
    }
    switch (callback->OnKey(token_key)) {
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
    if (callback->OnActualKey(token_key, token_key,
                              /* num_expanded= */ 0) ==
        Callback::TRAVERSE_DONE) {
      return;
    }
    Token token;
    tokens->PopulateToken(user_pos_token, PREFIX, pos_matcher_, &token);
    switch (callback->OnToken(token_key, token_key, std::move(token))) {
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
                                 Callback* callback) const {
  std::shared_ptr<const TokensIndex> tokens = GetTokens();

  if (key.empty() || tokens->empty()) {
    return;
  }
  auto [begin, end] = tokens->EqualRange(key);
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

  for (; begin != end; ++begin) {
    const TokensIndex::Token user_pos_token(*begin);
    if (user_pos_token.pos_type() ==
        user_dictionary::UserDictionary::SUGGESTION_ONLY) {
      continue;
    }
    Token token;
    tokens->PopulateToken(user_pos_token, EXACT, pos_matcher_, &token);
    if (callback->OnToken(key, key, std::move(token)) !=
        Callback::TRAVERSE_CONTINUE) {
      return;
    }
  }
}

void UserDictionary::LookupReverse(absl::string_view key,
                                   Callback* callback) const {}

bool UserDictionary::LookupComment(absl::string_view key,
                                   absl::string_view value,
                                   std::string* comment) const {
  if (key.empty()) {
    return false;
  }

  std::shared_ptr<const TokensIndex> tokens = GetTokens();

  if (tokens->empty()) {
    return false;
  }

  // Set the comment that was found first.
  for (auto [begin, end] = tokens->EqualRange(key); begin != end; ++begin) {
    const TokensIndex::Token token(*begin);
    if (token.value() == value && !token.comment().empty()) {
      strings::Assign(*comment, token.comment());
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
  absl::StatusOr<FileTimeStamp> modification_time =
      FileUtil::GetModificationTime(filename_);
  if (!modification_time.ok()) {
    // If the file doesn't exist, return doing nothing.
    // Therefore if the file is deleted after first reload,
    // second reload does nothing so the content loaded by first reload
    // is kept as is.
    LOG(WARNING) << "Cannot get modification time of the user dictionary: "
                 << modification_time.status();
    LOG(INFO) << "MaybeStartReload() didn't start reloading";
    return true;
  }

  if (modified_at_.exchange(*modification_time) == *modification_time) {
    LOG(INFO) << "MaybeStartReload() didn't start reloading";
    return true;
  }
  if (reload_state_.exchange(ReloadState::kRunningWithPending) ==
      ReloadState::kIdle) {
    // Runs `ReloadThreadMain()` in a background thread.
    reload_task_.Schedule([this] { ReloadThreadMain(); });
  }
  return true;
}

void UserDictionary::WaitForReloader() { reload_task_.Wait(); }

void UserDictionary::ReloadThreadMain() {
  while (!canceled_signal_.load()) {
    reload_state_.store(ReloadState::kRunning);

    InputFileStream ifs(filename_, std::ios::binary);
    if (!ifs) {
      LOG(ERROR) << "Failed to open the user dictionary: " << filename_;
    } else if (!Load(ifs)) {
      LOG(ERROR) << "Failed to load the user dictionary: " << filename_;
    }

    ReloadState expected = ReloadState::kRunning;
    if (reload_state_.compare_exchange_strong(expected, ReloadState::kIdle)) {
      return;
    }
  }
  reload_state_.store(ReloadState::kIdle);
}

bool UserDictionary::Load(
    const user_dictionary::UserDictionaryStorage& storage) {
  // proto-based loading is mainly for unittesting, so not optimized.
  std::stringstream ifs;
  storage.SerializeToOstream(&ifs);
  return Load(ifs);
}

bool UserDictionary::Load(std::istream& ifs) {
  const size_t size = GetTokens()->size();

  constexpr size_t kVeryBigUserDictionarySize = 100000;

  if (size >= kVeryBigUserDictionarySize) {
    auto placeholder_empty_tokens = std::make_shared<TokensIndex>(*user_pos_);
    SetTokens(std::move(placeholder_empty_tokens));
  }

  auto tokens = std::make_shared<TokensIndex>(*user_pos_);
  if (!tokens->Load(ifs, &canceled_signal_)) {
    return false;
  }

  SetTokens(std::move(tokens));
  return true;
}

std::vector<std::string> UserDictionary::GetPosList() const {
  return user_pos_->GetPosList();
}

void UserDictionary::PopulateTokenFromUserPosTokenForTesting(
    const UserPos::Token& user_pos_token, RequestType request_type,
    Token* token) const {
  const std::string serialized =
      TokensIndex::SerializeToken(user_pos_token, "");
  GetTokens()->PopulateToken(TokensIndex::Token(serialized), request_type,
                             pos_matcher_, token);
}

}  // namespace dictionary
}  // namespace mozc
