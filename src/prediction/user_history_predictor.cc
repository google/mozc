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

#include "prediction/user_history_predictor.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/hash/hash.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "base/clock.h"
#include "base/config_file_stream.h"
#include "base/container/freelist.h"
#include "base/container/trie.h"
#include "base/file_util.h"
#include "base/hash.h"
#include "base/japanese_util.h"
#include "base/util.h"
#include "base/vlog.h"
#include "composer/composer.h"
#include "composer/query.h"
#include "converter/attribute.h"
#include "dictionary/dictionary_interface.h"
#include "engine/modules.h"
#include "prediction/result.h"
#include "prediction/user_history_predictor.pb.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "storage/encrypted_string_storage.h"
#include "storage/lru_cache.h"

namespace mozc::prediction {
namespace {

using ::mozc::composer::TypeCorrectedQuery;

// Finds suggestion candidates from the most recent 3000 history in LRU.
// We don't check all history, since suggestion is called every key event
constexpr size_t kDefaultMaxSuggestionTrial = 3000;

// Finds suffix matches of history_segments from the most recent 500 histories
// in LRU.
constexpr size_t kMaxPrevValueTrial = 500;

// On-memory LRU cache size.
// Typically memory/storage footprint becomes kLruCacheSize * 70 bytes.
// Note that actual entries serialized to the disk may be smaller
// than this size.
constexpr size_t kLruCacheSize = 10000;

// Don't save key/value that are
// longer than kMaxCandidateSize to avoid memory explosion
constexpr size_t kMaxStringLength = 256;

// Maximum size of next_entries
constexpr size_t kMaxNextEntriesSize = 6;

// File name for the history
#ifdef _WIN32
constexpr char kFileName[] = "user://history.db";
#else   // _WIN32
constexpr char kFileName[] = "user://.history.db";
#endif  // _WIN32

// Uses '\t' as a key/value delimiter
constexpr absl::string_view kDelimiter = "\t";
constexpr absl::string_view kEmojiDescription = "絵文字";

constexpr size_t kRevertCacheSize = 16;

// TODO(peria, hidehiko): Unify this checker and IsEmojiCandidate in
//     EmojiRewriter.  If you make similar functions before the merging in
//     case, put a similar note to avoid twisted dependency.
bool IsEmojiEntry(const UserHistoryPredictor::Entry &entry) {
  return (entry.has_description() &&
          absl::StrContains(entry.description(), kEmojiDescription));
}

// http://unicode.org/~scherer/emoji4unicode/snapshot/full.html
constexpr absl::string_view kUtf8MinGooglePuaEmoji = "\xf3\xbe\x80\x80";
constexpr absl::string_view kUtf8MaxGooglePuaEmoji = "\xf3\xbe\xba\xa0";

bool IsAndroidPuaEmoji(absl::string_view s) {
  return (s.size() == 4 && kUtf8MinGooglePuaEmoji <= s &&
          s <= kUtf8MaxGooglePuaEmoji);
}

bool IsPunctuation(absl::string_view value) {
  return (value == "。" || value == "." || value == "、" || value == "," ||
          value == "？" || value == "?" || value == "！" || value == "!" ||
          value == "，" || value == "．");
}

// Returns romanaized string.
std::string ToRoman(const absl::string_view str) {
  return japanese_util::HiraganaToRomanji(str);
}

// Returns true if value looks like a content word.
// Currently, just checks the script type.
bool IsContentWord(const absl::string_view value) {
  return Util::CharsLen(value) > 1 ||
         Util::GetScriptType(value) != Util::UNKNOWN_SCRIPT;
}

// Returns candidate description.
// If candidate is spelling correction, typing correction
// or auto partial suggestion,
// don't use the description, since "did you mean" like description must be
// provided at an appropriate timing and context.
absl::string_view GetDescription(const Result &result) {
  if (result.candidate_attributes &
      (converter::Attribute::SPELLING_CORRECTION |
       converter::Attribute::TYPING_CORRECTION |
       converter::Attribute::AUTO_PARTIAL_SUGGESTION)) {
    return "";
  }
  return result.description;
}

}  // namespace

// Returns true if the input first candidate seems to be a privacy sensitive
// such like password.
bool UserHistoryPredictor::IsPrivacySensitive(
    absl::Span<const Result> results) const {
  constexpr bool kNonSensitive = false;
  constexpr bool kSensitive = true;

  if (results.empty()) {
    return kNonSensitive;
  }

  // Skips privacy sensitive check if |segments| consists of multiple conversion
  // segment. That is, segments like "パスワードは|x7LAGhaR" where '|'
  // represents segment boundary is not considered to be privacy sensitive.
  // TODO(team): Revisit this rule if necessary.
  if (results.front().inner_segment_boundary.size() > 1) {
    return kNonSensitive;
  }

  // Hereafter, we must have only one conversion segment.
  absl::string_view key = results.front().key;

  // The top candidate, which is about to be committed.
  absl::string_view candidate_value = results.front().value;

  // If |candidate_value| contains any non-ASCII character, do not treat
  // it as privacy sensitive information.
  // TODO(team): Improve the following rule. For example,
  //     "0000－0000－0000－0000" is not treated as privacy sensitive
  //     because of this rule. When a user commits his password in
  //     full-width form by mistake, like "ｘ７ＬＡＧｈａＲ", it is not
  //     treated as privacy sensitive too.
  if (!Util::IsAscii(candidate_value)) {
    return kNonSensitive;
  }

  // Hereafter, |candidate_value| consists of ASCII characters only.

  // Note: if the key looks like hiragana, the candidate might be Katakana to
  // English transliteration. Don't suppress transliterated candidates.
  // http://b/4394325

  // If the key consists of number characters only, treat it as privacy
  // sensitive.
  if (Util::GetScriptType(key) == Util::NUMBER) {
    return kSensitive;
  }

  return kNonSensitive;
}

bool UserHistoryStorage::Load() {
  std::string input;
  if (!storage_.Load(&input)) {
    LOG(ERROR) << "Can't load user history data.";
    return false;
  }

  if (!proto_.ParseFromString(input)) {
    LOG(ERROR) << "ParseFromString failed. message looks broken";
    return false;
  }

  MOZC_VLOG(1) << "Loaded user history, size=" << proto_.entries_size();
  return true;
}

bool UserHistoryStorage::Save() {
  std::string output;
  if (!proto_.AppendToString(&output)) {
    LOG(ERROR) << "AppendToString failed";
    return false;
  }

  // Remove the storage file when proto is empty because
  // storing empty file causes an error.
  if (output.empty()) {
    FileUtil::UnlinkIfExists(storage_.filename()).IgnoreError();
    return true;
  }

  if (!storage_.Save(output)) {
    LOG(ERROR) << "Can't save user history data.";
    return false;
  }

  return true;
}

bool UserHistoryPredictor::EntryPriorityQueue::Push(Entry *entry) {
  DCHECK(entry);
  if (!seen_.insert(absl::HashOf(entry->value())).second) {
    MOZC_VLOG(2) << "found dups";
    return false;
  }
  const uint32_t score = UserHistoryPredictor::GetScore(*entry);
  agenda_.push(std::make_pair(score, entry));
  return true;
}

UserHistoryPredictor::Entry *absl_nullable
UserHistoryPredictor::EntryPriorityQueue::Pop() {
  if (agenda_.empty()) {
    return nullptr;
  }
  const QueueElement &element = agenda_.top();
  Entry *result = element.second;
  DCHECK(result);
  agenda_.pop();
  return result;
}

UserHistoryPredictor::Entry *
UserHistoryPredictor::EntryPriorityQueue::NewEntry() {
  return pool_.Alloc();
}

UserHistoryPredictor::UserHistoryPredictor(const engine::Modules &modules)
    : dictionary_(modules.GetDictionary()),
      user_dictionary_(modules.GetUserDictionary()),
      updated_(false),
      dic_(new DicCache(UserHistoryPredictor::cache_size())),
      modules_(modules),
      revert_cache_(kRevertCacheSize) {
  AsyncLoad();  // non-blocking
  // Load()  blocking version can be used if any
}

UserHistoryPredictor::~UserHistoryPredictor() {
  // In destructor, must call blocking version
  WaitForSyncer();
  Save();  // blocking
}

std::string UserHistoryPredictor::GetUserHistoryFileName() {
  return ConfigFileStream::GetFileName(kFileName);
}

void UserHistoryPredictor::WaitForSyncer() {
  if (sync_.has_value()) {
    sync_->Wait();
    sync_.reset();
  }
}

bool UserHistoryPredictor::Wait() {
  WaitForSyncer();
  return true;
}

bool UserHistoryPredictor::CheckSyncerAndDelete() const {
  if (sync_.has_value()) {
    if (!sync_->Ready()) {
      return false;
    }
    sync_.reset();
  }

  return true;
}

bool UserHistoryPredictor::Sync() { return AsyncSave(); }

bool UserHistoryPredictor::Reload() {
  WaitForSyncer();
  return AsyncLoad();
}

bool UserHistoryPredictor::AsyncLoad() {
  if (!CheckSyncerAndDelete()) {  // now loading/saving
    return true;
  }

  sync_.emplace([this] {
    MOZC_VLOG(1) << "Executing Reload method";
    Load();
  });

  return true;
}

bool UserHistoryPredictor::AsyncSave() {
  if (!updated_) {
    return true;
  }

  if (!CheckSyncerAndDelete()) {  // now loading/saving
    return true;
  }

  sync_.emplace([this] {
    MOZC_VLOG(1) << "Executing Sync method";
    Save();
  });

  return true;
}

bool UserHistoryPredictor::Load() {
  const std::string filename = GetUserHistoryFileName();

  UserHistoryStorage history(filename);
  if (!history.Load()) {
    LOG(ERROR) << "UserHistoryStorage::Load() failed";
    return false;
  }
  return Load(std::move(history));
}

bool UserHistoryPredictor::Load(UserHistoryStorage &&history) {
  dic_->Clear();
  for (Entry &entry : *history.GetProto().mutable_entries()) {
    // Workaround for b/116826494: Some garbled characters are suggested
    // from user history. This filters such entries.
    if (entry.value().empty() || entry.key().empty() ||
        !Util::IsValidUtf8(entry.value())) {
      LOG(ERROR) << "Invalid UTF8 found in user history: " << entry;
      continue;
    }
    // conversion_freq is migrated to suggestion_freq.
    entry.set_suggestion_freq(
        std::max(entry.suggestion_freq(), entry.conversion_freq_deprecated()));
    entry.clear_conversion_freq_deprecated();
    // Avoid std::move() is called before EntryFingerprint.
    const uint32_t dic_key = EntryFingerprint(entry);
    dic_->Insert(dic_key, std::move(entry));
  }

  return true;
}

bool UserHistoryPredictor::Save() {
  if (!updated_) {
    return true;
  }

  const std::string filename = GetUserHistoryFileName();

  UserHistoryStorage history(filename);

  // Copy the current values to avoid these values from being updated in
  // the actual copy operations.
  const int store_size = cache_store_size_.load();
  const absl::Duration lifetime_days = entry_lifetime_days_as_duration();
  auto &proto = history.GetProto();

  const absl::Time now = Clock::GetAbslTime();
  for (const DicElement &elm : *dic_) {
    if (store_size > 0 && proto.entries_size() >= store_size) {
      break;
    }
    if (absl::FromUnixSeconds(elm.value.last_access_time()) + lifetime_days <
        now) {
      continue;
    }
    *proto.add_entries() = elm.value;
  }

  // Reverse the contents to keep the LRU order when loading.
  absl::c_reverse(*proto.mutable_entries());

  if (!history.Save()) {
    LOG(ERROR) << "UserHistoryStorage::Save() failed";
    return false;
  }

  Load(std::move(history));

  updated_ = false;

  return true;
}

bool UserHistoryPredictor::ClearAllHistory() {
  // Waits until syncer finishes
  WaitForSyncer();

  MOZC_VLOG(1) << "Clearing user prediction";
  // Renews DicCache as LruCache tries to reuse the internal value by
  // using FreeList
  dic_ = std::make_unique<DicCache>(UserHistoryPredictor::cache_size());

  updated_ = true;

  Sync();

  return true;
}

bool UserHistoryPredictor::ClearUnusedHistory() {
  // Waits until syncer finishes
  WaitForSyncer();

  MOZC_VLOG(1) << "Clearing unused prediction";
  if (dic_->empty()) {
    MOZC_VLOG(2) << "dic is empty";
    return false;
  }

  std::vector<uint32_t> keys;
  for (const DicElement &elm : *dic_) {
    MOZC_VLOG(3) << elm.key << " " << elm.value.suggestion_freq();
    if (elm.value.suggestion_freq() == 0) {
      keys.push_back(elm.key);
    }
  }

  for (const uint32_t key : keys) {
    MOZC_VLOG(2) << "Removing: " << key;
    if (!dic_->Erase(key)) {
      LOG(ERROR) << "cannot erase " << key;
    }
  }

  updated_ = true;

  Sync();

  MOZC_VLOG(1) << keys.size() << " removed";

  return true;
}

// Erases all the next_entries whose entry_fp field equals |fp|.
void UserHistoryPredictor::EraseNextEntries(uint32_t fp, Entry *entry) {
  const size_t orig_size = entry->next_entries_size();
  size_t new_size = orig_size;
  for (size_t pos = 0; pos < new_size;) {
    if (entry->next_entries(pos).entry_fp() == fp) {
      entry->mutable_next_entries()->SwapElements(pos, --new_size);
    } else {
      ++pos;
    }
  }
  for (size_t i = 0; i < orig_size - new_size; ++i) {
    entry->mutable_next_entries()->RemoveLast();
  }
}

// Recursively finds the Ngram history that produces |target_key| and
// |target_value| and removes the last link. For example, if there exists a
// chain like
//    ("aaa", "AAA") -- ("bbb", "BBB") -- ("ccc", "CCC"),
// and if target_key == "aaabbbccc" and target_value == "AAABBBCCC", the link
// from ("bbb", "BBB") to ("ccc", "CCC") is removed. If a link was removed, this
// method returns DONE. If no history entries can produce the target key
// value, then NOT_FOUND is returned. TAIL is returned only when the
// tail was found, e.g., in the above example, when the method finds the tail
// node ("ccc", "CCC").
UserHistoryPredictor::RemoveNgramChainResult
UserHistoryPredictor::RemoveNgramChain(
    const absl::string_view target_key, const absl::string_view target_value,
    Entry *entry, std::vector<absl::string_view> *key_ngrams,
    size_t key_ngrams_len, std::vector<absl::string_view> *value_ngrams,
    size_t value_ngrams_len) {
  DCHECK(entry);
  DCHECK(key_ngrams);
  DCHECK(value_ngrams);

  // Updates the lengths with the current entry node.
  key_ngrams_len += entry->key().size();
  value_ngrams_len += entry->value().size();

  // This is the case where ngram key and value are shorter than the target key
  // and value, respectively. In this case, we need to find further entries to
  // concatenate in order to make |target_key| and |target_value|.
  if (key_ngrams_len < target_key.size() &&
      value_ngrams_len < target_value.size()) {
    key_ngrams->push_back(entry->key());
    value_ngrams->push_back(entry->value());
    for (size_t i = 0; i < entry->next_entries().size(); ++i) {
      const uint32_t fp = entry->next_entries(i).entry_fp();
      Entry *e = dic_->MutableLookupWithoutInsert(fp);
      if (e == nullptr) {
        continue;
      }
      const RemoveNgramChainResult r =
          RemoveNgramChain(target_key, target_value, e, key_ngrams,
                           key_ngrams_len, value_ngrams, value_ngrams_len);
      switch (r) {
        case RemoveNgramChainResult::DONE:
          return RemoveNgramChainResult::DONE;
        case RemoveNgramChainResult::TAIL:
          // |entry| is the second-to-the-last node. So cut the link to the
          // child entry.
          EraseNextEntries(fp, entry);
          return RemoveNgramChainResult::DONE;
        default:
          break;
      }
    }
    // Recovers the state.
    key_ngrams->pop_back();
    value_ngrams->pop_back();
    return RemoveNgramChainResult::NOT_FOUND;
  }

  // This is the case where the current ngram key and value have the same
  // lengths as those of |target_key| and |target_value|, respectively.
  if (key_ngrams_len == target_key.size() &&
      value_ngrams_len == target_value.size()) {
    key_ngrams->push_back(entry->key());
    value_ngrams->push_back(entry->value());
    const std::string ngram_key = absl::StrJoin(*key_ngrams, "");
    const std::string ngram_value = absl::StrJoin(*value_ngrams, "");
    if (ngram_key == target_key && ngram_value == target_value) {
      // |entry| is the last node. So return TAIL to tell the caller so
      // that it can remove the link to this last node.
      return RemoveNgramChainResult::TAIL;
    }
    key_ngrams->pop_back();
    value_ngrams->pop_back();
    return RemoveNgramChainResult::NOT_FOUND;
  }

  return RemoveNgramChainResult::NOT_FOUND;
}

bool UserHistoryPredictor::ClearHistoryEntry(const absl::string_view key,
                                             const absl::string_view value) {
  bool deleted = false;
  {
    // Finds the history entry that has the exactly same key and value and has
    // not been removed yet. If exists, remove it.
    Entry *entry = dic_->MutableLookupWithoutInsert(Fingerprint(key, value));
    if (entry != nullptr && !entry->removed()) {
      entry->set_suggestion_freq(0);
      entry->set_shown_freq(0);
      entry->set_removed(true);
      // We don't clear entry->next_entries() so that we can generate prediction
      // by chaining.
      deleted = true;
    }
  }
  {
    // Finds a chain of history entries that produces key and value. If exists,
    // remove the link so that N-gram history prediction never generates this
    // key value pair..
    for (DicElement &elm : *dic_) {
      Entry *entry = &elm.value;
      if (!key.starts_with(entry->key()) ||
          !value.starts_with(entry->value())) {
        continue;
      }
      std::vector<absl::string_view> key_ngrams, value_ngrams;
      if (RemoveNgramChain(key, value, entry, &key_ngrams, 0, &value_ngrams,
                           0) == RemoveNgramChainResult::DONE) {
        deleted = true;
      }
    }
  }
  if (deleted) {
    updated_ = true;
  }
  return deleted;
}

// Returns true if prev_entry has a next_fp link to entry
// static
bool UserHistoryPredictor::HasBigramEntry(const Entry &entry,
                                          const Entry &prev_entry) {
  const uint32_t fp = EntryFingerprint(entry);
  for (int i = 0; i < prev_entry.next_entries_size(); ++i) {
    if (fp == prev_entry.next_entries(i).entry_fp()) {
      return true;
    }
  }
  return false;
}

// static
std::string UserHistoryPredictor::GetRomanMisspelledKey(
    const ConversionRequest &request) {
  if (request.config().preedit_method() != config::Config::ROMAN) {
    return "";
  }

  absl::string_view preedit = request.key();
  // TODO(team): Use composer if it is available.
  // segments.composer()->GetQueryForConversion(&preedit);
  // Since ConverterInterface doesn't have StartPredictionWithComposer,
  // we cannot use composer currently.
  if (!preedit.empty() && MaybeRomanMisspelledKey(preedit)) {
    return ToRoman(preedit);
  }

  return "";
}

std::vector<TypeCorrectedQuery> UserHistoryPredictor::GetTypingCorrectedQueries(
    const ConversionRequest &request) const {
  const int size = request.request()
                       .decoder_experiment_params()
                       .typing_correction_apply_user_history_size();
  if (size == 0 || !request.config().use_typing_correction()) return {};

  const std::optional<std::vector<TypeCorrectedQuery>> corrected =
      modules_.GetSupplementalModel().CorrectComposition(request);
  if (!corrected) return {};

  std::vector<TypeCorrectedQuery> result = std::move(corrected.value());
  if (size < result.size()) result.resize(size);

  return result;
}

// static
bool UserHistoryPredictor::MaybeRomanMisspelledKey(
    const absl::string_view key) {
  int num_alpha = 0;
  int num_hiragana = 0;
  int num_unknown = 0;
  for (ConstChar32Iterator iter(key); !iter.Done(); iter.Next()) {
    const char32_t codepoint = iter.Get();
    const Util::ScriptType type = Util::GetScriptType(codepoint);
    if (type == Util::HIRAGANA || codepoint == 0x30FC) {  // "ー".
      ++num_hiragana;
      continue;
    }
    if (type == Util::UNKNOWN_SCRIPT && num_unknown <= 0) {
      ++num_unknown;
      continue;
    }
    if (type == Util::ALPHABET && num_alpha <= 0) {
      ++num_alpha;
      continue;
    }
    return false;
  }

  return (num_hiragana > 0 && ((num_alpha == 1 && num_unknown == 0) ||
                               (num_alpha == 0 && num_unknown == 1)));
}

// static
bool UserHistoryPredictor::RomanFuzzyPrefixMatch(
    const absl::string_view str, const absl::string_view prefix) {
  if (prefix.empty() || prefix.size() > str.size()) {
    return false;
  }

  // 1. Allows one character delete in Romanji sequence.
  // 2. Allows one swap in Romanji sequence.
  for (size_t i = 0; i < prefix.size(); ++i) {
    if (prefix[i] == str[i]) {
      continue;
    }

    if (str[i] == '-') {
      // '-' voice sound mark can be matched to any
      // non-alphanum character.
      if (!isalnum(prefix[i])) {
        std::string replaced_prefix(prefix);
        replaced_prefix[i] = str[i];
        if (str.starts_with(replaced_prefix)) {
          return true;
        }
      }
    } else {
      // deletion.
      std::string inserted_prefix(prefix);
      inserted_prefix.insert(i, 1, str[i]);
      if (str.starts_with(inserted_prefix)) {
        return true;
      }

      // swap.
      if (i + 1 < prefix.size()) {
        using std::swap;
        std::string swapped_prefix(prefix);
        swap(swapped_prefix[i], swapped_prefix[i + 1]);
        if (str.starts_with(swapped_prefix)) {
          return true;
        }
      }
    }

    return false;
  }

  // |prefix| is an exact suffix of |str|.
  return false;
}

bool UserHistoryPredictor::ZeroQueryLookupEntry(
    const ConversionRequest &request, absl::string_view request_key,
    const Entry *entry, const Entry *prev_entry,
    EntryPriorityQueue *entry_queue) const {
  DCHECK(entry);
  DCHECK(entry_queue);

  // - request_key is empty,
  // - prev_entry(history) = 東京
  // - entry in LRU = 東京で
  //   Then we suggest で as zero query suggestion.

  // when `perv_entry` is not null, it is guaranteed that
  // the history segment is in the LRU cache.
  if (prev_entry && request.request().zero_query_suggestion() &&
      request_key.empty() && entry->key().size() > prev_entry->key().size() &&
      entry->value().size() > prev_entry->value().size() &&
      entry->key().starts_with(prev_entry->key()) &&
      entry->value().starts_with(prev_entry->value())) {
    // suffix must starts with Japanese characters.
    std::string key = entry->key().substr(prev_entry->key().size());
    std::string value = entry->value().substr(prev_entry->value().size());
    const auto type = Util::GetFirstScriptType(value);
    if (type != Util::KANJI && type != Util::HIRAGANA &&
        type != Util::KATAKANA) {
      return false;
    }
    Entry *result = entry_queue->NewEntry();
    DCHECK(result);
    // Copy timestamp from `entry`
    *result = *entry;
    result->set_key(std::move(key));
    result->set_value(std::move(value));
    result->set_bigram_boost(true);
    entry_queue->Push(result);
    return true;
  }

  return false;
}

bool UserHistoryPredictor::RomanFuzzyLookupEntry(
    const absl::string_view roman_request_key, const Entry *entry,
    EntryPriorityQueue *entry_queue) const {
  if (roman_request_key.empty()) {
    return false;
  }

  DCHECK(entry);
  DCHECK(entry_queue);

  if (!RomanFuzzyPrefixMatch(ToRoman(entry->key()), roman_request_key)) {
    return false;
  }

  Entry *result = entry_queue->NewEntry();
  DCHECK(result);
  *result = *entry;
  result->set_spelling_correction(true);
  entry_queue->Push(result);

  return true;
}

UserHistoryPredictor::Entry *UserHistoryPredictor::AddEntry(
    const Entry &entry, EntryPriorityQueue *entry_queue) const {
  // We add an entry even if it was marked as removed so that it can be used to
  // generate prediction by entry chaining. The deleted entry itself is never
  // shown in the final prediction result as it is filtered finally.
  Entry *new_entry = entry_queue->NewEntry();
  *new_entry = entry;
  return new_entry;
}

UserHistoryPredictor::Entry *UserHistoryPredictor::AddEntryWithNewKeyValue(
    std::string key, std::string value, Entry entry,
    EntryPriorityQueue *entry_queue) const {
  // We add an entry even if it was marked as removed so that it can be used to
  // generate prediction by entry chaining. The deleted entry itself is never
  // shown in the final prediction result as it is filtered finally.
  Entry *new_entry = entry_queue->NewEntry();
  *new_entry = std::move(entry);
  new_entry->set_key(std::move(key));
  new_entry->set_value(std::move(value));

  // Sets removed field true if the new key and value were removed.
  const Entry *e = dic_->LookupWithoutInsert(
      Fingerprint(new_entry->key(), new_entry->value()));
  new_entry->set_removed(e != nullptr && e->removed());

  return new_entry;
}

bool UserHistoryPredictor::GetKeyValueForExactAndRightPrefixMatch(
    const absl::string_view request_key, const Entry *entry,
    const Entry **result_last_entry, uint64_t *left_last_access_time,
    uint64_t *left_most_last_access_time, std::string *result_key,
    std::string *result_value) const {
  std::string key = entry->key();
  std::string value = entry->value();
  const Entry *current_entry = entry;
  absl::flat_hash_set<std::pair<absl::string_view, absl::string_view>> seen;
  seen.emplace(current_entry->key(), current_entry->value());
  // Until target entry gets longer than request_key.
  while (key.size() <= request_key.size()) {
    const Entry *latest_entry = nullptr;
    const Entry *left_same_timestamp_entry = nullptr;
    const Entry *left_most_same_timestamp_entry = nullptr;
    for (size_t i = 0; i < current_entry->next_entries_size(); ++i) {
      const Entry *tmp_next_entry =
          dic_->LookupWithoutInsert(current_entry->next_entries(i).entry_fp());
      if (tmp_next_entry == nullptr || tmp_next_entry->key().empty()) {
        continue;
      }
      const MatchType mtype_joined =
          GetMatchType(absl::StrCat(key, tmp_next_entry->key()), request_key);
      if (mtype_joined == MatchType::NO_MATCH ||
          mtype_joined == MatchType::LEFT_EMPTY_MATCH) {
        continue;
      }
      if (latest_entry == nullptr || latest_entry->last_access_time() <
                                         tmp_next_entry->last_access_time()) {
        latest_entry = tmp_next_entry;
      }
      if (tmp_next_entry->last_access_time() == *left_last_access_time) {
        left_same_timestamp_entry = tmp_next_entry;
      }
      if (tmp_next_entry->last_access_time() == *left_most_last_access_time) {
        left_most_same_timestamp_entry = tmp_next_entry;
      }
    }

    // Prefer bigrams which are generated at the same time.
    // When last_access_time are the same, these two bigrams were
    // input together.
    // The preferences:
    // (1). The current entry's time stamp is equal to that of
    //      left most content word
    // (2). The current entry's time stamp is equal to that of
    //      left closest content word
    // (3). The current entry is the latest
    const Entry *next_entry = left_most_same_timestamp_entry;
    if (next_entry == nullptr) {
      next_entry = left_same_timestamp_entry;
    }
    if (next_entry == nullptr) {
      next_entry = latest_entry;
    }

    if (next_entry == nullptr || next_entry->key().empty()) {
      break;
    }

    // If duplicate entry is found, don't expand more.
    // This is because an entry only has one timestamp.
    // we cannot trust the timestamp if there are duplicate values
    // in one input.
    if (!seen.emplace(next_entry->key(), next_entry->value()).second) {
      break;
    }

    key += next_entry->key();
    value += next_entry->value();
    current_entry = next_entry;
    *result_last_entry = next_entry;

    // Don't update left_access_time if the current entry is
    // not a content word.
    // The time-stamp of non-content-word will be updated frequently.
    // The time-stamp of the previous candidate is more trustful.
    // It partially fixes the bug http://b/2843371.
    const bool is_content_word = IsContentWord(current_entry->value());

    if (is_content_word) {
      *left_last_access_time = current_entry->last_access_time();
    }

    // If left_most entry is a functional word (symbols/punctuations),
    // we don't take it as a canonical candidate.
    if (*left_most_last_access_time == 0 && is_content_word) {
      *left_most_last_access_time = current_entry->last_access_time();
    }
  }

  if (key.size() < request_key.size()) {
    MOZC_VLOG(3) << "Cannot find prefix match even after chain rules";
    return false;
  }

  *result_key = std::move(key);
  *result_value = std::move(value);
  return true;
}

bool UserHistoryPredictor::LookupEntry(const ConversionRequest &request,
                                       const absl::string_view request_key,
                                       const absl::string_view key_base,
                                       const Trie<std::string> *key_expanded,
                                       const Entry *entry,
                                       const Entry *prev_entry,
                                       EntryPriorityQueue *entry_queue) const {
  DCHECK(entry);
  DCHECK(entry_queue);

  Entry *result = nullptr;

  const Entry *last_entry = nullptr;

  // last_access_time of the left-closest content word.
  uint64_t left_last_access_time = 0;

  // last_access_time of the left-most content word.
  uint64_t left_most_last_access_time = 0;

  // Example: [a|B|c|D]
  // a,c: functional word
  // B,D: content word
  // left_last_access_time:   timestamp of D
  // left_most_last_access_time:   timestamp of B

  // |request_key| is a query user is now typing.
  // |entry->key()| is a target value saved in the database.
  //  const string request_key = key_base;

  const MatchType mtype =
      GetMatchTypeFromInput(request_key, key_base, key_expanded, entry->key());
  if (mtype == MatchType::NO_MATCH) {
    return false;
  } else if (mtype == MatchType::LEFT_EMPTY_MATCH) {  // zero-query-suggestion
    // if |request_key| is empty, the |prev_entry| and |entry| must
    // have bigram relation.
    if (prev_entry != nullptr && HasBigramEntry(*entry, *prev_entry)) {
      result = AddEntry(*entry, entry_queue);
      if (result) {
        last_entry = entry;
        left_last_access_time = entry->last_access_time();
        left_most_last_access_time =
            IsContentWord(entry->value()) ? left_last_access_time : 0;
      }
    } else {
      return false;
    }
  } else if (mtype == MatchType::LEFT_PREFIX_MATCH) {
    // |request_key| is shorter than |entry->key()|
    // This scenario is a simple prefix match.
    // e.g., |request_key|="foo", |entry->key()|="foobar"
    result = AddEntry(*entry, entry_queue);
    if (result) {
      last_entry = entry;
      left_last_access_time = entry->last_access_time();
      left_most_last_access_time =
          IsContentWord(entry->value()) ? left_last_access_time : 0;
    }
  } else if (mtype == MatchType::RIGHT_PREFIX_MATCH ||
             mtype == MatchType::EXACT_MATCH) {
    // |request_key| is longer than or the same as |entry->key()|.
    // In this case, recursively traverse "next_entries" until
    // target entry gets longer than request_key.
    // e.g., |request_key|="foobar", |entry->key()|="foo"
    if (request.request().zero_query_suggestion() &&
        mtype == MatchType::EXACT_MATCH) {
      // For mobile, we don't generate joined result.
      result = AddEntry(*entry, entry_queue);
      if (result) {
        last_entry = entry;
        left_last_access_time = entry->last_access_time();
        left_most_last_access_time =
            IsContentWord(entry->value()) ? left_last_access_time : 0;
      }
    } else {
      std::string key, value;
      left_last_access_time = entry->last_access_time();
      left_most_last_access_time =
          IsContentWord(entry->value()) ? left_last_access_time : 0;
      if (!GetKeyValueForExactAndRightPrefixMatch(
              request_key, entry, &last_entry, &left_last_access_time,
              &left_most_last_access_time, &key, &value)) {
        return false;
      }
      result = AddEntryWithNewKeyValue(std::move(key), std::move(value), *entry,
                                       entry_queue);
    }
  } else {
    LOG(ERROR) << "Unknown match mode: " << static_cast<int>(mtype);
    return false;
  }

  if (result == nullptr) {
    return false;
  }

  // If prev entry is not nullptr, check whether there is a bigram
  // from |prev_entry| to |entry|.
  result->set_bigram_boost(false);

  if (prev_entry != nullptr && HasBigramEntry(*entry, *prev_entry)) {
    // Sets bigram_boost flag so that this entry is boosted
    // against LRU policy.
    result->set_bigram_boost(true);
  }

  if (!result->removed()) {
    entry_queue->Push(result);
  }

  if (request.request().zero_query_suggestion()) {
    // For mobile, we don't generate joined result.
    return true;
  }

  // Generates joined result using |last_entry|.
  if (last_entry != nullptr && Util::CharsLen(result->key()) >= 1 &&
      2 * Util::CharsLen(request_key) >= Util::CharsLen(result->key())) {
    const Entry *latest_entry = nullptr;
    const Entry *left_same_timestamp_entry = nullptr;
    const Entry *left_most_same_timestamp_entry = nullptr;
    for (int i = 0; i < last_entry->next_entries_size(); ++i) {
      const Entry *tmp_entry =
          dic_->LookupWithoutInsert(last_entry->next_entries(i).entry_fp());
      if (tmp_entry == nullptr || tmp_entry->key().empty()) {
        continue;
      }
      if (latest_entry == nullptr ||
          latest_entry->last_access_time() < tmp_entry->last_access_time()) {
        latest_entry = tmp_entry;
      }
      if (tmp_entry->last_access_time() == left_last_access_time) {
        left_same_timestamp_entry = tmp_entry;
      }
      if (tmp_entry->last_access_time() == left_most_last_access_time) {
        left_most_same_timestamp_entry = tmp_entry;
      }
    }

    const Entry *next_entry = left_most_same_timestamp_entry;
    if (next_entry == nullptr) {
      next_entry = left_same_timestamp_entry;
    }
    if (next_entry == nullptr) {
      next_entry = latest_entry;
    }

    // The new entry was input within 10 seconds.
    // TODO(taku): This is a simple heuristics.
    if (next_entry != nullptr && !next_entry->key().empty() &&
        abs(static_cast<int32_t>(next_entry->last_access_time() -
                                 last_entry->last_access_time())) <= 10 &&
        IsContentWord(next_entry->value())) {
      Entry *result2 = AddEntryWithNewKeyValue(
          absl::StrCat(result->key(), next_entry->key()),
          absl::StrCat(result->value(), next_entry->value()), *result,
          entry_queue);
      if (!result2->removed()) {
        entry_queue->Push(result2);
      }
    }
  }

  return true;
}

std::vector<Result> UserHistoryPredictor::Predict(
    const ConversionRequest &request) const {
  {
    // MaybeProcessPartialRevertEntry is marked as const so
    // as to access it Predict(), but it updates the contents of dic_, so
    // WriterLock is required.
    absl::WriterMutexLock lock(&dic_mutex_);
    MaybeProcessPartialRevertEntry(request);
  }

  // The const-method accessing the contents in `dic_` should be protected
  // with ReaderLock, as the Entry in `dic_` may be updated in different thread.
  // We wants to use ReaderLock instead of MutexLock as the
  // actual decoding process is much slower than ReaderLock.
  absl::ReaderMutexLock lock(&dic_mutex_);

  if (!ShouldPredict(request)) {
    return {};
  }

  const bool is_empty_input = request.key().empty();
  const Entry *prev_entry = LookupPrevEntry(request);
  if (is_empty_input && prev_entry == nullptr) {
    MOZC_VLOG(1) << "If request_key_len is 0, prev_entry must be set";
    return {};
  }

  const auto &params = request.request().decoder_experiment_params();

  SetEntryLifetimeDays(params.user_history_entry_lifetime_days());
  SetCacheStoreSize(params.user_history_cache_store_size());

  const bool is_zero_query =
      request.request().zero_query_suggestion() && is_empty_input;
  size_t max_prediction_size =
      request.max_user_history_prediction_candidates_size();
  size_t max_prediction_char_coverage =
      params.user_history_prediction_max_char_coverage();

  if (is_zero_query) {
    max_prediction_size =
        request.max_user_history_prediction_candidates_size_for_zero_query();
  } else if (max_prediction_char_coverage > 0) {
    // When char coverage feature is enabled,
    // set a fixed value so that we can enumerate enough candidates.
    max_prediction_size = 3;
  }

  EntryPriorityQueue entry_queue = GetEntry_QueueFromHistoryDictionary(
      request, prev_entry, max_prediction_size * 5);

  if (entry_queue.size() == 0) {
    MOZC_VLOG(2) << "no prefix match candidate is found.";
    return {};
  }

  return MakeResults(request, max_prediction_size, max_prediction_char_coverage,
                     &entry_queue);
}

bool UserHistoryPredictor::ShouldPredict(
    const ConversionRequest &request) const {
  if (!CheckSyncerAndDelete()) {
    LOG(WARNING) << "Syncer is running";
    return false;
  }

  if (request.incognito_mode()) {
    MOZC_VLOG(2) << "incognito mode";
    return false;
  }

  if (request.config().history_learning_level() == config::Config::NO_HISTORY) {
    MOZC_VLOG(2) << "history learning level is NO_HISTORY";
    return false;
  }

  if (request.request_type() == ConversionRequest::CONVERSION) {
    MOZC_VLOG(2) << "request type is CONVERSION";
    return false;
  }

  if (!request.config().use_history_suggest() &&
      request.request_type() == ConversionRequest::SUGGESTION) {
    MOZC_VLOG(2) << "no history suggest";
    return false;
  }

  if (dic_->empty()) {
    MOZC_VLOG(2) << "dic is empty";
    return false;
  }

  absl::string_view request_key = request.key();

  if (request_key.empty() && !request.request().zero_query_suggestion()) {
    MOZC_VLOG(2) << "key length is 0";
    return false;
  }

  if (IsPunctuation(Util::Utf8SubString(request_key, 0, 1))) {
    MOZC_VLOG(2) << "request_key starts with punctuations";
    return false;
  }

  return true;
}

const UserHistoryPredictor::Entry *absl_nullable
UserHistoryPredictor::LookupPrevEntry(const ConversionRequest &request) const {
  // When there are non-zero history segments, lookup an entry
  // from the LRU dictionary, which is corresponding to the last
  // history segment.
  if (request.converter_history_size() == 0) {
    return nullptr;
  }

  // Finds the prev_entry from the longest context.
  // Even when the original value is split into content_value and suffix,
  // longest context information is used.
  const Entry *prev_entry = nullptr;
  for (int size = request.converter_history_size(); size >= 1; --size) {
    absl::string_view suffix_key = request.converter_history_key(size);
    absl::string_view suffix_value = request.converter_history_value(size);
    prev_entry =
        dic_->LookupWithoutInsert(Fingerprint(suffix_key, suffix_value));
    if (prev_entry) break;
  }

  // Check the timestamp of prev_entry.
  const absl::Time now = Clock::GetAbslTime();
  const absl::Duration lifetime_days = entry_lifetime_days_as_duration();
  if (prev_entry != nullptr &&
      absl::FromUnixSeconds(prev_entry->last_access_time()) + lifetime_days <
          now) {
    updated_ = true;  // We found an entry to be deleted at next save.
    return nullptr;
  }

  // When |prev_entry| is nullptr or |prev_entry| has no valid next_entries,
  // do linear-search over the LRU.
  if (prev_entry == nullptr ||
      (prev_entry != nullptr && prev_entry->next_entries_size() == 0)) {
    absl::string_view prev_value = prev_entry == nullptr
                                       ? request.converter_history_value(1)
                                       : prev_entry->value();
    int trial = 0;
    for (const DicElement &elm : *dic_) {
      if (++trial > kMaxPrevValueTrial) {
        break;
      }
      const Entry *entry = &(elm.value);
      // entry->value() equals to the prev_value or
      // entry->value() is a SUFFIX of prev_value.
      // length of entry->value() must be >= 2, as single-length
      // match would be noisy.
      if (IsValidEntry(*entry) && entry != prev_entry &&
          entry->next_entries_size() > 0 &&
          Util::CharsLen(entry->value()) >= 2 &&
          (entry->value() == prev_value ||
           prev_value.ends_with(entry->value()))) {
        prev_entry = entry;
        break;
      }
    }
  }
  return prev_entry;
}

UserHistoryPredictor::EntryPriorityQueue
UserHistoryPredictor::GetEntry_QueueFromHistoryDictionary(
    const ConversionRequest &request, const Entry *prev_entry,
    size_t max_entry_queue_size) const {
  // Gets romanized input key if the given preedit looks misspelled.
  const std::string roman_request_key = GetRomanMisspelledKey(request);

  const std::vector<TypeCorrectedQuery> corrected =
      GetTypingCorrectedQueries(request);

  // If we have ambiguity for the input, get expanded key.
  // Example1 roman input: for "あk", we will get |base|, "あ" and |expanded|,
  // "か", "き", etc
  // Example2 kana input: for "あか", we will get |base|, "あ" and |expanded|,
  // "か", and "が".

  // |base_key| and |request_key| could be different
  // For kana-input, we will expand the ambiguity for "゛".
  // When we input "もす",
  // |base_key|: "も"
  // |expanded|: "す", "ず"
  // |request_key|: "もす"
  // In this case, we want to show candidates for "もす" as EXACT match,
  // and candidates for "もず" as LEFT_PREFIX_MATCH
  //
  // For roman-input, when we input "あｋ",
  // |request_key| is "あｋ" and |base_key| is "あ"
  std::string request_key;
  std::string base_key;
  std::unique_ptr<Trie<std::string>> expanded;
  GetInputKeyFromRequest(request, &request_key, &base_key, &expanded);

  EntryPriorityQueue entry_queue;
  const absl::Time now = Clock::GetAbslTime();
  int trial = 0;
  int max_trial = request.request()
                      .decoder_experiment_params()
                      .user_history_max_suggestion_trial();
  if (max_trial <= 0) max_trial = kDefaultMaxSuggestionTrial;
  const absl::Duration lifetime_days = entry_lifetime_days_as_duration();

  for (const DicElement &elm : *dic_) {
    // already found enough entry_queue.
    if (entry_queue.size() >= max_entry_queue_size) {
      break;
    }

    if (!IsValidEntryIgnoringRemovedField(elm.value)) {
      continue;
    }
    if (absl::FromUnixSeconds(elm.value.last_access_time()) + lifetime_days <
        now) {
      updated_ = true;  // We found an entry to be deleted at next save.
      continue;
    }
    if (request.request_type() == ConversionRequest::SUGGESTION &&
        trial++ >= max_trial) {
      MOZC_VLOG(2) << "too many trials";
      break;
    }

    // Lookup key from elm_value and prev_entry.
    // If a new entry is found, the entry is pushed to the entry_queue.
    if (LookupEntry(request, request_key, base_key, expanded.get(),
                    &(elm.value), prev_entry, &entry_queue) ||
        RomanFuzzyLookupEntry(roman_request_key, &(elm.value), &entry_queue) ||
        ZeroQueryLookupEntry(request, request_key, &(elm.value), prev_entry,
                             &entry_queue)) {
      continue;
    }

    // Lookup typing corrected keys when the original `request_key` doesn't
    // match. Since dic_ is sorted in LRU, typing corrected queries are ranked
    // lower than the original key.
    for (const auto &c : corrected) {
      // Only apply when score > 0. When score < 0, we trigger literal-on-top
      // in dictionary predictor.
      if (c.score > 0.0 &&
          LookupEntry(request, c.correction, c.correction, nullptr,
                      &(elm.value), prev_entry, &entry_queue)) {
        break;
      }
    }
  }

  return entry_queue;
}

// static
void UserHistoryPredictor::GetInputKeyFromRequest(
    const ConversionRequest &request, std::string *request_key,
    std::string *base, std::unique_ptr<Trie<std::string>> *expanded) {
  DCHECK(request_key);
  DCHECK(base);

  *request_key = request.composer().GetStringForPreedit();
  // auto = std::pair<std::string, absl::btree_set<std::string>>
  const auto [query_base, expanded_set] =
      request.composer().GetQueriesForPrediction();
  *base = std::move(query_base);
  if (!expanded_set.empty()) {
    *expanded = std::make_unique<Trie<std::string>>();
    for (absl::string_view expanded_key : expanded_set) {
      // For getting matched key, insert values
      (*expanded)->AddEntry(expanded_key, expanded_key);
    }
  }
}

UserHistoryPredictor::ResultType UserHistoryPredictor::GetResultType(
    const ConversionRequest &request, bool is_top_candidate,
    uint32_t request_key_len, const Entry &entry) {
  if (request.request().mixed_conversion()) {
    if (IsValidSuggestionForMixedConversion(request, request_key_len, entry)) {
      return ResultType::GOOD_RESULT;
    }
    return ResultType::BAD_RESULT;
  }

  if (request.request_type() == ConversionRequest::SUGGESTION) {
    // The top result of suggestion should be a VALID suggestion candidate.
    // i.e., SuggestionTriggerFunc should return true for the first
    // candidate.
    // If user types "デスノート" too many times, "デスノート" will be
    // suggested when user types "で". It is expected, but if user types
    // "です" after that,  showing "デスノート" is annoying.
    // In this situation, "です" is in the LRU, but SuggestionTriggerFunc
    // returns false for "です", since it is short.
    if (IsValidSuggestion(request, request_key_len, entry)) {
      return ResultType::GOOD_RESULT;
    }
    if (is_top_candidate) {
      MOZC_VLOG(2) << "candidates size is 0";
      return ResultType::STOP_ENUMERATION;
    }
    return ResultType::BAD_RESULT;
  }

  return ResultType::GOOD_RESULT;
}

std::vector<Result> UserHistoryPredictor::MakeResults(
    const ConversionRequest &request, size_t max_prediction_size,
    size_t max_prediction_char_coverage,
    EntryPriorityQueue *entry_queue) const {
  DCHECK(entry_queue);
  if (request.request_type() != ConversionRequest::SUGGESTION &&
      request.request_type() != ConversionRequest::PREDICTION) {
    LOG(ERROR) << "Unknown mode";
    return {};
  }
  const uint32_t request_key_len = Util::CharsLen(request.key());

  size_t inserted_num = 0;
  size_t inserted_char_coverage = 0;

  std::vector<const UserHistoryPredictor::Entry *> entries;
  entries.reserve(entry_queue->size());

  // Current LRU-based candidates = ["東京"]
  //  target="東京は" -> Remove.   (target is longer)
  //  target="東"     -> Keep.     (target is shorter)
  // Current LRU-based candidates = ["東京は"]
  //   target="東京" -> Replace "東京は" and "東京"

  auto starts_with = [](absl::string_view text, absl::string_view prefix) {
    return text.starts_with(prefix) &&
           Util::IsScriptType(text.substr(prefix.size()), Util::HIRAGANA);
  };

  auto is_redandant = [&](absl::string_view target,
                          absl::string_view inserted) {
    return starts_with(target, inserted);
  };

  auto is_redandant_entry = [&](const Entry &entry) {
    return absl::c_find_if(entries, [&](const Entry *inserted_entry) {
             return is_redandant(entry.value(), inserted_entry->value());
           }) != entries.end();
  };

  // Replace `entry` with the one entry in `entries`.
  auto maybe_replace_entry = [&](const Entry *entry) {
    auto it = absl::c_find_if(entries, [&](const Entry *inserted_entry) {
      return starts_with(inserted_entry->value(), entry->value());
    });
    if (it == entries.end()) {
      return false;
    }
    inserted_char_coverage -= Util::CharsLen((*it)->value());
    inserted_char_coverage += Util::CharsLen(entry->value());
    *it = entry;

    return true;
  };

  while (inserted_num < max_prediction_size) {
    // |entry_queue| is a priority queue where the element
    // in the queue is sorted by the score defined in GetScore().
    const Entry *result_entry = entry_queue->Pop();
    if (result_entry == nullptr) {
      // Pop() returns nullptr when no more valid entry exists.
      break;
    }

    const ResultType result =
        GetResultType(request, entries.empty(), request_key_len, *result_entry);
    if (result == ResultType::STOP_ENUMERATION) {
      break;
    } else if (result == ResultType::BAD_RESULT) {
      continue;
    }

    // Check candidate redundancy.
    if (is_redandant_entry(*result_entry) ||
        maybe_replace_entry(result_entry)) {
      continue;
    }

    // Break when the accumulated character length exceeds the
    // `max_prediction_char_coverage`. Allows to add at least one candidate.
    const size_t value_len = Util::CharsLen(result_entry->value());
    if (max_prediction_char_coverage > 0 && inserted_num > 0 &&
        inserted_char_coverage + value_len > max_prediction_char_coverage) {
      break;
    }

    entries.emplace_back(result_entry);

    ++inserted_num;
    inserted_char_coverage += value_len;
  }

  std::vector<Result> results;

  for (const auto *result_entry : entries) {
    Result result;
    result.key = result_entry->key();
    result.value = result_entry->value();
    result.candidate_attributes |=
        converter::Attribute::USER_HISTORY_PREDICTION |
        converter::Attribute::NO_VARIANTS_EXPANSION;
    if (result_entry->spelling_correction()) {
      result.candidate_attributes |= converter::Attribute::SPELLING_CORRECTION;
    }
    absl::string_view description = result_entry->description();
    // If we have stored description, set it exactly.
    if (!description.empty()) {
      result.description = description;
      result.candidate_attributes |= converter::Attribute::NO_EXTRA_DESCRIPTION;
    }
    MOZC_WORD_LOG(result, "Added by UserHistoryPredictor::InsertCandidates");
#if DEBUG
    if (!absl::StrContains(result.description, "History")) {
      result.description += " History";
    }
#endif  // DEBUG

    results.emplace_back(std::move(result));
  }

  return results;
}

void UserHistoryPredictor::InsertNextEntry(const NextEntry &next_entry,
                                           Entry *entry) const {
  if (next_entry.entry_fp() == 0 || entry == nullptr) {
    return;
  }

  NextEntry *target_next_entry = nullptr;

  // If next_entries_size is less than kMaxNextEntriesSize,
  // we simply allocate a new entry.
  if (entry->next_entries_size() < max_next_entries_size()) {
    target_next_entry = entry->add_next_entries();
  } else {
    // Otherwise, find the oldest next_entry.
    uint64_t last_access_time = std::numeric_limits<uint64_t>::max();
    for (int i = 0; i < entry->next_entries_size(); ++i) {
      // Already has the same id
      if (next_entry.entry_fp() == entry->next_entries(i).entry_fp()) {
        target_next_entry = entry->mutable_next_entries(i);
        break;
      }
      const Entry *found_entry =
          dic_->LookupWithoutInsert(entry->next_entries(i).entry_fp());
      // Reuses the entry if it is already removed from the LRU.
      if (found_entry == nullptr) {
        target_next_entry = entry->mutable_next_entries(i);
        break;
      }
      // Preserves the oldest entry
      if (target_next_entry == nullptr ||
          last_access_time > found_entry->last_access_time()) {
        target_next_entry = entry->mutable_next_entries(i);
        last_access_time = found_entry->last_access_time();
      }
    }
  }

  if (target_next_entry == nullptr) {
    LOG(ERROR) << "cannot find a room for inserting next fp";
    return;
  }

  *target_next_entry = next_entry;
}

bool UserHistoryPredictor::IsValidEntry(const Entry &entry) const {
  if (entry.removed() || !IsValidEntryIgnoringRemovedField(entry)) {
    return false;
  }
  return true;
}

bool UserHistoryPredictor::IsValidEntryIgnoringRemovedField(
    const Entry &entry) const {
  if (user_dictionary_.IsSuppressedEntry(entry.key(), entry.value())) {
    return false;
  }

  if (IsEmojiEntry(entry) && IsAndroidPuaEmoji(entry.value())) {
    return false;
  }

  if (entry.key().ends_with(" ")) {
    // Invalid user history entry from alphanumeric input.
    return false;
  }

  return true;
}

bool UserHistoryPredictor::ShouldInsert(
    const ConversionRequest &request, absl::string_view key,
    absl::string_view value, const absl::string_view description) const {
  if (key.empty() || value.empty() || key.size() > kMaxStringLength ||
      value.size() > kMaxStringLength ||
      description.size() > kMaxStringLength) {
    return false;
  }

  // For mobile, we do not learn candidates that ends with punctuation.
  if (request.request().zero_query_suggestion() && Util::CharsLen(value) > 1 &&
      IsPunctuation(Util::Utf8SubString(value, Util::CharsLen(value) - 1, 1))) {
    return false;
  }
  return true;
}

void UserHistoryPredictor::TryInsert(
    const ConversionRequest &request, int32_t key_begin, int32_t value_begin,
    absl::string_view key, absl::string_view value,
    absl::string_view description, bool is_suggestion_selected,
    absl::Span<const uint32_t> next_fps, uint64_t last_access_time,
    UserHistoryPredictor::RevertEntries *revert_entries) {
  // b/279560433: Preprocess key value
  // (key|value)_begin don't change after StripTrailingAsciiWhitespace.
  key = absl::StripTrailingAsciiWhitespace(key);
  value = absl::StripTrailingAsciiWhitespace(value);
  if (ShouldInsert(request, key, value, description)) {
    Insert(key_begin, value_begin, key, value, description,
           is_suggestion_selected, next_fps, last_access_time, revert_entries);
  }
}

void UserHistoryPredictor::Insert(
    int32_t key_begin, int32_t value_begin, absl::string_view key,
    absl::string_view value, absl::string_view description,
    bool is_suggestion_selected, absl::Span<const uint32_t> next_fps,
    uint64_t last_access_time,
    UserHistoryPredictor::RevertEntries *revert_entries) {
  const uint32_t dic_key = Fingerprint(key, value);

  const bool has_dic_key = dic_->HasKey(dic_key);

  DicElement *e = dic_->Insert(dic_key);
  if (e == nullptr) {
    MOZC_VLOG(2) << "insert failed";
    return;
  }

  Entry *entry = &(e->value);
  DCHECK(entry);

  // `entry` might be reused from the heap, so explicitly clear.
  if (!has_dic_key) {
    entry->Clear();
  }

  entry->set_key(key);
  entry->set_value(value);
  entry->set_removed(false);

  if (description.empty()) {
    entry->clear_description();
  } else {
    entry->set_description(description);
  }

  revert_entries->entries.emplace_back(key_begin, value_begin, *entry);

  entry->set_last_access_time(last_access_time);
  entry->set_suggestion_freq(entry->suggestion_freq() + 1);

  // Inserts next_fp to the entry
  for (const auto next_fp : next_fps) {
    NextEntry next_entry;
    next_entry.set_entry_fp(next_fp);
    InsertNextEntry(next_entry, entry);
  }

  MOZC_VLOG(2) << entry->key() << " " << entry->value()
               << " has been inserted: " << *entry;

  // New entry is inserted to the cache
  updated_ = true;
}

void UserHistoryPredictor::MaybeRemoveUnselectedHistory(
    absl::Span<const Result> results,
    UserHistoryPredictor::RevertEntries *revert_entries) {
  static constexpr size_t kMaxHistorySize = 5;
  static constexpr float kMinSelectedRatio = 0.05;

  for (size_t i = 0; i < std::min(results.size(), kMaxHistorySize); ++i) {
    const uint64_t dic_key = Fingerprint(results[i].key, results[i].value);
    Entry *entry = dic_->MutableLookupWithoutInsert(dic_key);
    if (entry == nullptr) {
      continue;
    }
    // Note(b/339742825): For now shown freq is only used here and it's OK to
    // increment the value here.
    entry->set_shown_freq(entry->shown_freq() + 1);

    const float selected_ratio =
        1.0 * entry->suggestion_freq() / entry->shown_freq();

    if (selected_ratio < kMinSelectedRatio) {
      Entry revert_entry = *entry;
      revert_entry.set_shown_freq(std::max<int>(0, entry->shown_freq() - 1));
      revert_entries->entries.emplace_back(0, 0, std::move(revert_entry));

      entry->set_suggestion_freq(0);
      entry->set_shown_freq(0);
      entry->set_removed(true);
      continue;
    }
  }
}

void UserHistoryPredictor::Finish(const ConversionRequest &request,
                                  absl::Span<const Result> results,
                                  uint32_t revert_id) {
  if (request.incognito_mode()) {
    MOZC_VLOG(2) << "incognito mode";
    return;
  }

  if (request.config().history_learning_level() !=
      config::Config::DEFAULT_HISTORY) {
    MOZC_VLOG(2) << "history learning level is not DEFAULT_HISTORY: "
                 << request.config().history_learning_level();
    return;
  }

  if (!request.config().use_history_suggest()) {
    MOZC_VLOG(2) << "no history suggest";
    return;
  }

  if (!CheckSyncerAndDelete()) {
    LOG(WARNING) << "Syncer is running";
    return;
  }

  last_committed_entries_.reset();

  if (results.empty() || results.front().candidate_attributes &
                             converter::Attribute::NO_SUGGEST_LEARNING) {
    MOZC_VLOG(2) << "NO_SUGGEST_LEARNING";
    return;
  }

  if (IsPrivacySensitive(results)) {
    MOZC_VLOG(2) << "do not remember privacy sensitive input";
    return;
  }

  RevertEntries revert_entries;

  const SegmentsForLearning learning_segments =
      MakeLearningSegments(request, results);

  InsertHistory(request, learning_segments, &revert_entries);

  MaybeRemoveUnselectedHistory(results, &revert_entries);

  if (!revert_entries.entries.empty()) {
    revert_entries.result = results.front();
    if (auto *element = revert_cache_.Insert(revert_id); element) {
      element->value = std::move(revert_entries);
    }
  }
}

UserHistoryPredictor::SegmentsForLearning
UserHistoryPredictor::MakeLearningSegments(
    const ConversionRequest &request, absl::Span<const Result> results) const {
  SegmentsForLearning learning_segments;

  if (results.empty()) return learning_segments;

  auto make_learning_segments = [](const Result &result) {
    std::vector<SegmentForLearning> segments;
    const bool is_single_segment = result.inner_segments().size() <= 1;
    for (const auto &iter : result.inner_segments()) {
      const int key_begin =
          std::distance(result.key.data(), iter.GetKey().data());
      const int value_begin =
          std::distance(result.value.data(), iter.GetValue().data());
      segments.push_back({key_begin, value_begin, iter.GetKey(),
                          iter.GetValue(), iter.GetContentKey(),
                          iter.GetContentValue(),
                          is_single_segment ? GetDescription(result) : ""});
    }
    return segments;
  };

  const Result &result = results.front();

  learning_segments.conversion_segments_key = result.key;
  learning_segments.conversion_segments_value = result.value;
  learning_segments.history_segments =
      make_learning_segments(request.history_result());
  learning_segments.conversion_segments = make_learning_segments(result);

  return learning_segments;
}

void UserHistoryPredictor::InsertHistory(
    const ConversionRequest &request,
    const UserHistoryPredictor::SegmentsForLearning &learning_segments,
    UserHistoryPredictor::RevertEntries *revert_entries) {
  const bool is_suggestion_selected =
      request.request_type() != ConversionRequest::CONVERSION;
  const uint64_t last_access_time = absl::ToUnixSeconds(Clock::GetAbslTime());

  InsertHistoryForConversionSegments(request, is_suggestion_selected,
                                     last_access_time, learning_segments,
                                     revert_entries);
  InsertHistoryForHistorySegments(request, is_suggestion_selected,
                                  last_access_time, learning_segments,
                                  revert_entries);
}

void UserHistoryPredictor::InsertHistoryForHistorySegments(
    const ConversionRequest &request, bool is_suggestion_selected,
    uint64_t last_access_time, const SegmentsForLearning &learning_segments,
    UserHistoryPredictor::RevertEntries *revert_entries) {
  absl::string_view all_key = learning_segments.conversion_segments_key;
  absl::string_view all_value = learning_segments.conversion_segments_value;

  // Inserts all_key/all_value.
  // We don't insert it for mobile.
  if (!request.request().zero_query_suggestion() &&
      learning_segments.conversion_segments.size() > 1 && !all_key.empty() &&
      !all_value.empty()) {
    TryInsert(request, 0, 0, all_key, all_value, "", is_suggestion_selected, {},
              last_access_time, revert_entries);
  }

  // Makes a link from the last history_segment to the first conversion segment
  // or to the entire user input.
  if (!learning_segments.history_segments.empty() &&
      !learning_segments.conversion_segments.empty()) {
    const SegmentForLearning &history_segment =
        learning_segments.history_segments.back();
    const SegmentForLearning &conversion_segment =
        learning_segments.conversion_segments.front();
    absl::string_view history_value = history_segment.value;
    if (history_value.empty() || conversion_segment.value.empty()) {
      return;
    }
    // 1) Don't learn a link from a history which ends with punctuation.
    if (IsPunctuation(Util::Utf8SubString(
            history_value, Util::CharsLen(history_value) - 1, 1))) {
      return;
    }
    // 2) Don't learn a link to a punctuation.
    // Exception: For zero query suggestion, we learn a link to a single
    //            punctuation segment.
    // Example: "よろしく|。" -> OK
    //          "よろしく|。でも" -> NG
    //          "よろしく|。。" -> NG
    // Note that another piece of code handles learning for
    // (sentence + punctuation) form; see Finish().
    if (IsPunctuation(Util::Utf8SubString(conversion_segment.value, 0, 1)) &&
        (!request.request().zero_query_suggestion() ||
         Util::CharsLen(conversion_segment.value) > 1)) {
      return;
    }
    Entry *history_entry = dic_->MutableLookupWithoutInsert(
        LearningSegmentFingerprint(history_segment));
    if (history_entry) {
      NextEntry next_entry;
      if (!is_suggestion_selected) {
        for (const auto next_fp :
             LearningSegmentFingerprints(conversion_segment)) {
          next_entry.set_entry_fp(next_fp);
          InsertNextEntry(next_entry, history_entry);
        }
      }

      // Entire user input or SUGGESTION
      if (is_suggestion_selected ||
          learning_segments.conversion_segments.size() > 1) {
        next_entry.set_entry_fp(Fingerprint(all_key, all_value));
        InsertNextEntry(next_entry, history_entry);
      }
    }
  }
}

void UserHistoryPredictor::InsertHistoryForConversionSegments(
    const ConversionRequest &request, bool is_suggestion_selected,
    uint64_t last_access_time, const SegmentsForLearning &learning_segments,
    UserHistoryPredictor::RevertEntries *revert_entries) {
  absl::flat_hash_set<std::vector<uint32_t>> seen;
  bool this_was_seen = false;

  for (size_t i = 0; i < learning_segments.conversion_segments.size(); ++i) {
    const SegmentForLearning &segment =
        learning_segments.conversion_segments[i];
    std::vector<uint32_t> next_fps;
    if (i + 1 < learning_segments.conversion_segments.size()) {
      next_fps = LearningSegmentFingerprints(
          learning_segments.conversion_segments[i + 1]);
    }
    // remember the first segment
    if (i == 0) {
      seen.insert(LearningSegmentFingerprints(segment));
    }
    std::vector<uint32_t> next_fps_to_set = next_fps;
    // If two duplicate segments exist, kills the link
    // TO/FROM the second one to prevent loops.
    // Only killing "TO" link caused bug #2982886:
    // after converting "らいおん（もうじゅう）とぞうりむし（びせいぶつ）"
    // and typing "ぞうりむし", "ゾウリムシ（猛獣" was suggested.
    if (this_was_seen) {
      next_fps_to_set.clear();
    }
    if (!seen.insert(next_fps).second) {
      next_fps_to_set.clear();
      this_was_seen = true;
    } else {
      this_was_seen = false;
    }
    TryInsert(request, segment.key_begin, segment.value_begin, segment.key,
              segment.value, segment.description, is_suggestion_selected,
              next_fps_to_set, last_access_time, revert_entries);
    if (request.request().mixed_conversion() &&
        segment.content_key != segment.key &&
        segment.content_value != segment.value) {
      TryInsert(request, segment.key_begin, segment.value_begin,
                segment.content_key, segment.content_value, segment.description,
                is_suggestion_selected, {}, last_access_time, revert_entries);
    }
  }
}

void UserHistoryPredictor::Revert(uint32_t revert_id) {
  if (!CheckSyncerAndDelete()) {
    LOG(WARNING) << "Syncer is running";
    return;
  }

  const RevertEntries *revert_entries =
      revert_cache_.LookupWithoutInsert(revert_id);
  if (!revert_entries) {
    return;
  }

  // `last_committed_entries` keeps the original entries before Revert.
  auto last_committed_entries = std::make_unique<RevertEntries>();

  // `revert_entries->entries` store the entries before the commit,
  // while `last_committed_entries->entries` will store the entries after the
  // commit.
  last_committed_entries->result = revert_entries->result;

  for (const auto &[key_begin, value_begin, revert_entry] :
       revert_entries->entries) {
    // We do not explicitly remove the entry from the `dic_`, but simply
    // rollback to the previous entry. This behavior is consistent with the
    // partial-revert operation. Entry with zero frequency is not suggested in
    // the first place.
    // Currently, dic_ stores the committed entries, but wants to rollback
    // to them to reverted entries..
    if (Entry *committed_entry =
            dic_->MutableLookupWithoutInsert(EntryFingerprint(revert_entry));
        committed_entry) {
      // Stores the entries after the commit so we can redo the commit
      // operations after Revert().
      last_committed_entries->entries.emplace_back(key_begin, value_begin,
                                                   *committed_entry);
      // Performs the actual revert operation. dic_ will store the
      // reverted entries.
      *committed_entry = revert_entry;
    }
  }

  last_committed_entries_ = std::move(last_committed_entries);
}

// static
UserHistoryPredictor::MatchType UserHistoryPredictor::GetMatchType(
    const absl::string_view lstr, const absl::string_view rstr) {
  if (lstr.empty() && !rstr.empty()) {
    return MatchType::LEFT_EMPTY_MATCH;
  }

  const size_t size = std::min(lstr.size(), rstr.size());
  if (size == 0) {
    return MatchType::NO_MATCH;
  }

  if (lstr.substr(0, size) != rstr.substr(0, size)) {
    return MatchType::NO_MATCH;
  }

  if (lstr.size() == rstr.size()) {
    return MatchType::EXACT_MATCH;
  } else if (lstr.size() < rstr.size()) {
    return MatchType::LEFT_PREFIX_MATCH;
  } else {
    return MatchType::RIGHT_PREFIX_MATCH;
  }

  return MatchType::NO_MATCH;
}

// static
UserHistoryPredictor::MatchType UserHistoryPredictor::GetMatchTypeFromInput(
    const absl::string_view request_key, const absl::string_view key_base,
    const Trie<std::string> *key_expanded, const absl::string_view target) {
  if (key_expanded == nullptr) {
    // |request_key| and |key_base| can be different by composer modification.
    // For example, |request_key|, "８，＋", and |base| "８、＋".
    return GetMatchType(key_base, target);
  }

  // We can assume key_expanded != nullptr from here.
  if (key_base.empty()) {
    std::string value;
    size_t key_length = 0;
    bool has_subtrie = false;
    if (!key_expanded->LookUpPrefix(target, &value, &key_length,
                                    &has_subtrie)) {
      return MatchType::NO_MATCH;
    } else if (value == target && value == request_key) {
      return MatchType::EXACT_MATCH;
    } else {
      return MatchType::LEFT_PREFIX_MATCH;
    }
  } else {
    const size_t size = std::min(key_base.size(), target.size());
    if (size == 0) {
      return MatchType::NO_MATCH;
    }
    if (key_base.substr(0, size) != target.substr(0, size)) {
      return MatchType::NO_MATCH;
    }
    if (target.size() <= key_base.size()) {
      return MatchType::RIGHT_PREFIX_MATCH;
    }
    std::string value;
    size_t key_length = 0;
    bool has_subtrie = false;
    if (!key_expanded->LookUpPrefix(target.data() + key_base.size(), &value,
                                    &key_length, &has_subtrie)) {
      return MatchType::NO_MATCH;
    }
    const std::string matched = absl::StrCat(key_base, value);
    if (matched == target && matched == request_key) {
      return MatchType::EXACT_MATCH;
    } else {
      return MatchType::LEFT_PREFIX_MATCH;
    }
  }

  DCHECK(false) << "Should not come here";
  return MatchType::NO_MATCH;
}

// static
uint32_t UserHistoryPredictor::Fingerprint(const absl::string_view key,
                                           const absl::string_view value) {
  return Fingerprint32(absl::StrCat(key, kDelimiter, value));
}

// static
uint32_t UserHistoryPredictor::EntryFingerprint(const Entry &entry) {
  return Fingerprint(entry.key(), entry.value());
}

// static
uint32_t UserHistoryPredictor::LearningSegmentFingerprint(
    const SegmentForLearning &segment) {
  return Fingerprint(segment.key, segment.value);
}

std::vector<uint32_t> UserHistoryPredictor::LearningSegmentFingerprints(
    const SegmentForLearning &segment) const {
  std::vector<uint32_t> fps;
  fps.reserve(2);
  fps.push_back(Fingerprint(segment.key, segment.value));
  if (segment.key != segment.content_key) {
    fps.push_back(Fingerprint(segment.content_key, segment.content_value));
  }
  return fps;
}

bool UserHistoryPredictor::IsValidSuggestionForMixedConversion(
    const ConversionRequest &request, uint32_t prefix_len, const Entry &entry) {
  if (entry.suggestion_freq() < 2 && Util::CharsLen(entry.value()) > 8) {
    // Don't show long history for mixed conversion
    MOZC_VLOG(2) << "long candidate: " << entry.value();
    return false;
  }

  return true;
}

// static
bool UserHistoryPredictor::IsValidSuggestion(const ConversionRequest &request,
                                             uint32_t prefix_len,
                                             const Entry &entry) {
  // When bigram_boost is true, that means that previous user input
  // and current input have bigram relation.
  if (entry.bigram_boost()) {
    return true;
  }
  // When zero_query_suggestion is true, that means that
  // predictor is running on mobile device. In this case,
  // make the behavior more aggressive.
  if (request.request().zero_query_suggestion()) {
    return true;
  }
  const uint32_t freq = entry.suggestion_freq();
  // TODO(taku,komatsu): better to make it simpler and easier to be understood.
  const uint32_t base_prefix_len = 3 - std::min<uint32_t>(2, freq);
  return (prefix_len >= base_prefix_len);
}

// static
// 1) sort by last_access_time, which is basically the same as LRU policy.
// 2) boost shorter candidate, if having the same last_access_time.
// 3) add a bigram boost as a special bonus.
// TODO(taku): better to take "frequency" into consideration
uint32_t UserHistoryPredictor::GetScore(const Entry &entry) {
  constexpr uint32_t kBigramBoostAsTime = 7 * 24 * 60 * 60;  // 1 week.
  return entry.last_access_time() - Util::CharsLen(entry.value()) +
         (entry.bigram_boost() ? kBigramBoostAsTime : 0);
}

// static
int32_t UserHistoryPredictor::GuessRevertedValueOffset(
    absl::string_view reverted_value, absl::string_view left_context) {
  if (reverted_value.empty() || left_context.empty()) {
    return 0;
  }

  // Removes specified suffixes which may be inserted directly.
  while (true) {
    bool removed = false;
    // Includes full width whitespace.
    for (absl::string_view suffix : {"\r", "\n", "\t", " ", "　"}) {
      if (absl::EndsWith(left_context, suffix)) {
        left_context.remove_suffix(suffix.size());
        removed = true;
        break;
      }
    }
    if (!removed) break;
  }

  absl::string_view value = reverted_value;
  absl::string_view remain;
  char32_t last_char;  // not used.

  // Strip the last character one-by-one, emulating the backspace key.
  while (Util::SplitLastChar32(value, &remain, &last_char)) {
    if (absl::EndsWith(left_context, value)) {
      return value.size();
    }
    value = remain;
  }

  return 0;
}

// Example
// 1) Commit "東京駅|に".
//    1-1) Remembers {東京駅, 東京駅に} in `revert_entries`.
//    1-2) Increments the frequency of entries and updates the LRU.
// 2) Revert.
//    2-1) Remembers the entries in dic_ in `last_committed_entries`.
//    2-1) Rollback the entries in dic_ to `revert_entries`.
// 3) Erase "に" with backspace.
// 4) Type new characters
//    `MaybeProcessPartialRevertEntry` is called in the step 4).
//    4-1) "東京駅" is the suffix of the left-context ->
//          re-commits the entries by inserting `last_committed_entries`
//          to dic_.
//    4-2) "東京駅に" exceeds the left-context-boundary, so do nothing.
//
//    Note that step 4) may generate the entries which are not in the
//    revert_entries depending on the cursor position. e.g. "東京".
//    In this case, creates a new entry and insert it to dic_.
void UserHistoryPredictor::MaybeProcessPartialRevertEntry(
    const ConversionRequest &request) const {
  if (!last_committed_entries_) {
    return;
  }

  auto last_committed_entries = std::move(last_committed_entries_);
  last_committed_entries_.reset();

  const commands::DecoderExperimentParams &params =
      request.request().decoder_experiment_params();
  if (params.user_history_partial_revert_mode() == 0) {
    return;
  }

  // Gets the actual cursor position after committing the result.
  const int32_t actual_value_end = GuessRevertedValueOffset(
      last_committed_entries->result.value, request.context().preceding_text());

  // actual_value_end == 0 means that all characters are removed.
  // There are no entries to rollback.
  if (actual_value_end == 0) {
    return;
  }

  auto insert_entry = [&](const Entry &new_entry) {
    if (Entry *entry =
            dic_->MutableLookupWithoutInsert(EntryFingerprint(new_entry));
        entry) {
      *entry = new_entry;
      updated_ = true;
    }
  };

  auto force_insert_entry = [&](const Entry &new_entry) {
    const uint32_t dic_key = EntryFingerprint(new_entry);
    const bool has_dic_key = dic_->HasKey(dic_key);
    DicElement *elm = dic_->Insert(dic_key);
    if (!elm) return;

    Entry *entry = &(elm->value);
    if (has_dic_key) {
      // reuse key, value, description and other fields.
      entry->set_suggestion_freq(entry->suggestion_freq() + 1);
    } else {
      *entry = new_entry;  // copy key and value
      entry->set_suggestion_freq(1);
    }
    entry->set_last_access_time(new_entry.last_access_time());
    updated_ = true;
  };

  absl::flat_hash_map<int32_t, int32_t> value_to_key_end_map;
  absl::flat_hash_set<std::pair<int32_t, int32_t>> range_map;

  auto maybe_initialize_map = [&]() {
    if (!value_to_key_end_map.empty() && !range_map.empty()) return;
    for (const auto &[key_begin, value_begin, committed_entry] :
         last_committed_entries->entries) {
      const int32_t key_end = key_begin + committed_entry.key().size();
      const int32_t value_end = value_begin + committed_entry.value().size();
      value_to_key_end_map.emplace(value_end, key_end);
      range_map.emplace(value_begin, value_end);
    }
  };

  // Returns the key_end corresponding to `value_end`.
  auto get_key_end = [&](int32_t value_end) {
    maybe_initialize_map();
    return value_to_key_end_map[value_end];
  };

  // Returns true if there is an entry at [value_begin, value_end).
  auto has_entry_in = [&](int32_t value_begin, int32_t value_end) {
    maybe_initialize_map();
    return range_map.contains(std::make_pair(value_begin, value_end));
  };

  // Utility functions to obtain the prefix/suffix after
  // removing n- last characters.
  auto get_suffix = [](absl::string_view str, int32_t n) {
    DCHECK_LE(n, str.size());
    return str.substr(str.size() - n, n);
  };
  auto get_prefix = [](absl::string_view str, int32_t n) {
    DCHECK_LE(n, str.size());
    return str.substr(0, str.size() - n);
  };

  for (const auto &[key_begin, value_begin, committed_entry] :
       last_committed_entries->entries) {
    absl::string_view cvalue = committed_entry.value();
    absl::string_view ckey = committed_entry.key();
    const int32_t value_end = value_begin + cvalue.size();
    const int32_t key_end = key_begin + ckey.size();

    if (value_end <= actual_value_end) {
      // `committed_entry` exists in the remained context.
      // safely rollback the `committed_entry`.
      insert_entry(committed_entry);
    } else if (value_begin < actual_value_end && actual_value_end < value_end) {
      // Cursor is at the middle of the `committed_entry.value()`.
      // Heuristically obtain the prefix key (= ckey_prefix).
      const int32_t cvalue_suffix_len = value_end - actual_value_end;
      absl::string_view cvalue_suffix = get_suffix(cvalue, cvalue_suffix_len);
      absl::string_view cvalue_prefix = get_prefix(cvalue, cvalue_suffix_len);
      absl::string_view ckey_prefix;  // unknown here.
      if (has_entry_in(value_begin, actual_value_end)) {
        // There is an another entry in [value_begin, actual_value_end).
      } else if (const int32_t actual_key_end = get_key_end(actual_value_end);
                 actual_key_end > 0 && actual_key_end > key_begin &&
                 actual_key_end < key_end) {
        // Reuses the boundary of other entries ending at `actual_key_end`.
        ckey_prefix = get_prefix(ckey, key_end - actual_key_end);
      } else if (cvalue_suffix_len < ckey.size() &&
                 cvalue_suffix == get_suffix(ckey, cvalue_suffix_len) &&
                 Util::GetScriptType(cvalue_suffix) == Util::HIRAGANA) {
        // suffix value/key are same and script type is HIRAGANA.
        ckey_prefix = get_prefix(ckey, cvalue_suffix_len);
      } else {
        // Uses the aligner to get the ckey_prefix.
        // GetReadingAlignment returns the per-char alignment, e.g.
        // {東京駅, とうきょうえき} -> {{東, とう}, {京, きょう}, {駅, えき}}
        int32_t key_consumed = 0, value_consumed = 0;
        for (const auto &[surface, reading] :
             modules_.GetSupplementalModel().GetReadingAlignment(cvalue,
                                                                 ckey)) {
          if (value_consumed > cvalue_prefix.size()) break;
          if (value_consumed == cvalue_prefix.size()) {
            ckey_prefix = ckey.substr(0, key_consumed);
            break;
          }
          key_consumed += reading.size();
          value_consumed += surface.size();
        }
      }

      if (!ckey_prefix.empty() && !cvalue_prefix.empty()) {
        Entry prefix_entry = committed_entry;
        prefix_entry.set_key(ckey_prefix);
        prefix_entry.set_value(cvalue_prefix);
        force_insert_entry(prefix_entry);
      }
    }
  }
}

// Returns the size of cache.
// static
uint32_t UserHistoryPredictor::cache_size() { return kLruCacheSize; }

// Returns the size of next entries.
uint32_t UserHistoryPredictor::max_next_entries_size() const {
  return kMaxNextEntriesSize;
}

}  // namespace mozc::prediction
