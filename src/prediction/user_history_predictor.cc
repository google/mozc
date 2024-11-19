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
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_set.h"
#include "absl/hash/hash.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "base/bits.h"
#include "base/clock.h"
#include "base/config_file_stream.h"
#include "base/container/freelist.h"
#include "base/container/trie.h"
#include "base/hash.h"
#include "base/japanese_util.h"
#include "base/util.h"
#include "base/vlog.h"
#include "composer/composer.h"
#include "converter/segments.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suppression_dictionary.h"
#include "engine/modules.h"
#include "prediction/user_history_predictor.pb.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "rewriter/variants_rewriter.h"
#include "storage/encrypted_string_storage.h"
#include "storage/lru_cache.h"
#include "usage_stats/usage_stats.h"

namespace mozc::prediction {
namespace {

using ::mozc::composer::TypeCorrectedQuery;
using ::mozc::usage_stats::UsageStats;

// Finds suggestion candidates from the most recent 3000 history in LRU.
// We don't check all history, since suggestion is called every key event
constexpr size_t kMaxSuggestionTrial = 3000;

// Finds suffix matches of history_segments from the most recent 500 histories
// in LRU.
constexpr size_t kMaxPrevValueTrial = 500;

// Cache size
// Typically memory/storage footprint becomes kLruCacheSize * 70 bytes.
#ifdef __ANDROID__
constexpr size_t kLruCacheSize = 4000;
#else   // __ANDROID__
constexpr size_t kLruCacheSize = 10000;
#endif  // __ANDROID__

// Don't save key/value that are
// longer than kMaxCandidateSize to avoid memory explosion
constexpr size_t kMaxStringLength = 256;

// Maximum size of next_entries
constexpr size_t kMaxNextEntriesSize = 4;

// Revert id for user_history_predictor
const uint16_t kRevertId = 1;

// File name for the history
#ifdef _WIN32
constexpr char kFileName[] = "user://history.db";
#else   // _WIN32
constexpr char kFileName[] = "user://.history.db";
#endif  // _WIN32

// Uses '\t' as a key/value delimiter
constexpr absl::string_view kDelimiter = "\t";
constexpr absl::string_view kEmojiDescription = "絵文字";

constexpr absl::Duration k62Days = absl::Hours(62 * 24);

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

bool IsSentenceLikeCandidate(const Segment::Candidate &candidate) {
  // A sentence should have a long reading.  Length check is done using key to
  // absorb length difference in value variation, e.g.,
  // "〜ください" and "〜下さい".
  if (candidate.value.empty() || Util::CharsLen(candidate.key) < 8) {
    return false;
  }
  // Our primary target sentence ends with Hiragana, e.g., "〜ます".
  const ConstChar32ReverseIterator iter(candidate.value);
  bool ret = Util::GetScriptType(iter.Get()) == Util::HIRAGANA;
  return ret;
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
std::string GetDescription(const Segment::Candidate &candidate) {
  if (candidate.attributes & (Segment::Candidate::SPELLING_CORRECTION |
                              Segment::Candidate::TYPING_CORRECTION |
                              Segment::Candidate::AUTO_PARTIAL_SUGGESTION)) {
    return "";
  }
  return candidate.description;
}

}  // namespace

// Returns true if the input first candidate seems to be a privacy sensitive
// such like password.
bool UserHistoryPredictor::IsPrivacySensitive(const Segments *segments) const {
  constexpr bool kNonSensitive = false;
  constexpr bool kSensitive = true;

  // Skips privacy sensitive check if |segments| consists of multiple conversion
  // segment. That is, segments like "パスワードは|x7LAGhaR" where '|'
  // represents segment boundary is not considered to be privacy sensitive.
  // TODO(team): Revisit this rule if necessary.
  if (segments->conversion_segments_size() != 1) {
    return kNonSensitive;
  }

  // Hereafter, we must have only one conversion segment.
  const Segment &conversion_segment = segments->conversion_segment(0);
  const std::string &segment_key = conversion_segment.key();

  // The top candidate, which is about to be committed.
  const Segment::Candidate &candidate = conversion_segment.candidate(0);
  const std::string &candidate_value = candidate.value;

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
  if (Util::GetScriptType(segment_key) == Util::NUMBER) {
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

  const int num_deleted = DeleteEntriesUntouchedFor62Days();
  LOG_IF(INFO, num_deleted > 0)
      << num_deleted << " old entries were not loaded "
      << proto_.entries_size();

  MOZC_VLOG(1) << "Loaded user history, size=" << proto_.entries_size();
  return true;
}

bool UserHistoryStorage::Save() {
  if (proto_.entries_size() == 0) {
    LOG(WARNING) << "etries size is 0. Not saved";
    return false;
  }

  const int num_deleted = DeleteEntriesUntouchedFor62Days();
  LOG_IF(INFO, num_deleted > 0)
      << num_deleted << " old entries were removed before save";

  std::string output;
  if (!proto_.AppendToString(&output)) {
    LOG(ERROR) << "AppendToString failed";
    return false;
  }

  if (!storage_.Save(output)) {
    LOG(ERROR) << "Can't save user history data.";
    return false;
  }

  return true;
}

int UserHistoryStorage::DeleteEntriesBefore(uint64_t timestamp) {
  // Partition entries so that [0, new_size) is kept and [new_size, size) is
  // deleted.
  int i = 0;
  int new_size = proto_.entries_size();
  while (i < new_size) {
    if (proto_.entries(i).entry_type() !=
            UserHistoryPredictor::Entry::DEFAULT_ENTRY ||
        proto_.entries(i).last_access_time() >= timestamp) {
      ++i;
      continue;
    }
    // Swap this entry (to be deleted) and the last entry (not yet checked) for
    // batch deletion.
    --new_size;
    if (i != new_size) {
      proto_.mutable_entries()->SwapElements(i, new_size);
    }
  }
  if (new_size == proto_.entries_size()) {
    return 0;
  }
  const int num_deleted = proto_.entries_size() - new_size;
  proto_.mutable_entries()->DeleteSubrange(new_size, num_deleted);
  return num_deleted;
}

int UserHistoryStorage::DeleteEntriesUntouchedFor62Days() {
  const absl::Time now = Clock::GetAbslTime();
  const absl::Time timestamp = std::max(now - k62Days, absl::UnixEpoch());
  return DeleteEntriesBefore(absl::ToUnixSeconds(timestamp));
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

UserHistoryPredictor::Entry *UserHistoryPredictor::EntryPriorityQueue::Pop() {
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

UserHistoryPredictor::UserHistoryPredictor(const engine::Modules &modules,
                                           bool enable_content_word_learning)
    : dictionary_(modules.GetDictionary()),
      pos_matcher_(modules.GetPosMatcher()),
      suppression_dictionary_(modules.GetSuppressionDictionary()),
      predictor_name_("UserHistoryPredictor"),
      content_word_learning_enabled_(enable_content_word_learning),
      updated_(false),
      dic_(new DicCache(UserHistoryPredictor::cache_size())),
      modules_(modules) {
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

// Returns revert id
// static
uint16_t UserHistoryPredictor::revert_id() { return kRevertId; }

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

bool UserHistoryPredictor::Sync() {
  return AsyncSave();
  // return Save();   blocking version
}

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
  return Load(history);
}

bool UserHistoryPredictor::Load(const UserHistoryStorage &history) {
  dic_->Clear();
  for (const Entry &entry : history.GetProto().entries()) {
    // Workaround for b/116826494: Some garbled characters are suggested
    // from user history. This filters such entries.
    if (!Util::IsValidUtf8(entry.value())) {
      LOG(ERROR) << "Invalid UTF8 found in user history: " << entry;
      continue;
    }
    dic_->Insert(EntryFingerprint(entry), entry);
  }

  MOZC_VLOG(1) << "Loaded user history, size="
               << history.GetProto().entries_size();

  return true;
}

bool UserHistoryPredictor::Save() {
  if (!updated_) {
    return true;
  }

  // Do not check incognito_mode or use_history_suggest in Config here.
  // The input data should not have been inserted when those flags are on.

  const DicElement *tail = dic_->Tail();
  if (tail == nullptr) {
    return true;
  }

  const std::string filename = GetUserHistoryFileName();

  UserHistoryStorage history(filename);
  for (const DicElement *elm = tail; elm != nullptr; elm = elm->prev) {
    *history.GetProto().add_entries() = elm->value;
  }

  // Updates usage stats here.
  UsageStats::SetInteger("UserHistoryPredictorEntrySize",
                         static_cast<int>(history.GetProto().entries_size()));

  if (!history.Save()) {
    LOG(ERROR) << "UserHistoryStorage::Save() failed";
    return false;
  }
  Load(history);

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

  // insert a dummy event entry.
  InsertEvent(Entry::CLEAN_ALL_EVENT);

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

  // Inserts a dummy event entry.
  InsertEvent(Entry::CLEAN_UNUSED_EVENT);

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
        case DONE:
          return DONE;
        case TAIL:
          // |entry| is the second-to-the-last node. So cut the link to the
          // child entry.
          EraseNextEntries(fp, entry);
          return DONE;
        default:
          break;
      }
    }
    // Recovers the state.
    key_ngrams->pop_back();
    value_ngrams->pop_back();
    return NOT_FOUND;
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
      return TAIL;
    }
    key_ngrams->pop_back();
    value_ngrams->pop_back();
    return NOT_FOUND;
  }

  return NOT_FOUND;
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
      entry->set_conversion_freq(0);
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
      if (!absl::StartsWith(key, entry->key()) ||
          !absl::StartsWith(value, entry->value())) {
        continue;
      }
      std::vector<absl::string_view> key_ngrams, value_ngrams;
      if (RemoveNgramChain(key, value, entry, &key_ngrams, 0, &value_ngrams,
                           0) == DONE) {
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
    const ConversionRequest &request, const Segments &segments) {
  if (request.config().preedit_method() != config::Config::ROMAN) {
    return "";
  }

  const std::string &preedit = segments.conversion_segment(0).key();
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
    const ConversionRequest &request, const Segments &segments) const {
  const int size = request.request()
                       .decoder_experiment_params()
                       .typing_correction_apply_user_history_size();
  if (size == 0 || !request.config().use_typing_correction()) return {};

  const engine::SupplementalModelInterface *supplemental_model =
      modules_.GetSupplementalModel();
  if (supplemental_model == nullptr) return {};

  const std::optional<std::vector<TypeCorrectedQuery>> corrected =
      supplemental_model->CorrectComposition(request, segments);
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
        if (absl::StartsWith(str, replaced_prefix)) {
          return true;
        }
      }
    } else {
      // deletion.
      std::string inserted_prefix(prefix);
      inserted_prefix.insert(i, 1, str[i]);
      if (absl::StartsWith(str, inserted_prefix)) {
        return true;
      }

      // swap.
      if (i + 1 < prefix.size()) {
        using std::swap;
        std::string swapped_prefix(prefix);
        swap(swapped_prefix[i], swapped_prefix[i + 1]);
        if (absl::StartsWith(str, swapped_prefix)) {
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
    RequestType request_type, absl::string_view input_key, const Entry *entry,
    const Entry *prev_entry, EntryPriorityQueue *results) const {
  DCHECK(entry);
  DCHECK(results);

  // - input_key is empty,
  // - prev_entry(history) = 東京
  // - entry in LRU = 東京で
  //   Then we suggest で as zero query suggestion.

  // when `perv_entry` is not null, it is guaranteed that
  // the history segment is in the LRU cache.
  if (prev_entry && aggressive_bigram_enabled_ &&
      request_type == ZERO_QUERY_SUGGESTION && input_key.empty() &&
      entry->key().size() > prev_entry->key().size() &&
      entry->value().size() > prev_entry->value().size() &&
      absl::StartsWith(entry->key(), prev_entry->key()) &&
      absl::StartsWith(entry->value(), prev_entry->value())) {
    // suffix must starts with Japanese characters.
    std::string key = entry->key().substr(prev_entry->key().size());
    std::string value = entry->value().substr(prev_entry->value().size());
    const auto type = Util::GetFirstScriptType(value);
    if (type != Util::KANJI && type != Util::HIRAGANA &&
        type != Util::KATAKANA) {
      return false;
    }
    Entry *result = results->NewEntry();
    DCHECK(result);
    // Copy timestamp from `entry`
    *result = *entry;
    result->set_key(std::move(key));
    result->set_value(std::move(value));
    result->set_bigram_boost(true);
    results->Push(result);
    return true;
  }

  return false;
}

bool UserHistoryPredictor::RomanFuzzyLookupEntry(
    const absl::string_view roman_input_key, const Entry *entry,
    EntryPriorityQueue *results) const {
  if (roman_input_key.empty()) {
    return false;
  }

  DCHECK(entry);
  DCHECK(results);

  if (!RomanFuzzyPrefixMatch(ToRoman(entry->key()), roman_input_key)) {
    return false;
  }

  Entry *result = results->NewEntry();
  DCHECK(result);
  *result = *entry;
  result->set_spelling_correction(true);
  results->Push(result);

  return true;
}

UserHistoryPredictor::Entry *UserHistoryPredictor::AddEntry(
    const Entry &entry, EntryPriorityQueue *results) const {
  // We add an entry even if it was marked as removed so that it can be used to
  // generate prediction by entry chaining. The deleted entry itself is never
  // shown in the final prediction result as it is filtered finally.
  Entry *new_entry = results->NewEntry();
  *new_entry = entry;
  return new_entry;
}

UserHistoryPredictor::Entry *UserHistoryPredictor::AddEntryWithNewKeyValue(
    std::string key, std::string value, Entry entry,
    EntryPriorityQueue *results) const {
  // We add an entry even if it was marked as removed so that it can be used to
  // generate prediction by entry chaining. The deleted entry itself is never
  // shown in the final prediction result as it is filtered finally.
  Entry *new_entry = results->NewEntry();
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
    const absl::string_view input_key, const Entry *entry,
    const Entry **result_last_entry, uint64_t *left_last_access_time,
    uint64_t *left_most_last_access_time, std::string *result_key,
    std::string *result_value) const {
  std::string key = entry->key();
  std::string value = entry->value();
  const Entry *current_entry = entry;
  absl::flat_hash_set<std::pair<absl::string_view, absl::string_view>> seen;
  seen.emplace(current_entry->key(), current_entry->value());
  // Until target entry gets longer than input_key.
  while (key.size() <= input_key.size()) {
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
          GetMatchType(absl::StrCat(key, tmp_next_entry->key()), input_key);
      if (mtype_joined == NO_MATCH || mtype_joined == LEFT_EMPTY_MATCH) {
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

  if (key.size() < input_key.size()) {
    MOZC_VLOG(3) << "Cannot find prefix match even after chain rules";
    return false;
  }

  *result_key = std::move(key);
  *result_value = std::move(value);
  return true;
}

bool UserHistoryPredictor::LookupEntry(RequestType request_type,
                                       const absl::string_view input_key,
                                       const absl::string_view key_base,
                                       const Trie<std::string> *key_expanded,
                                       const Entry *entry,
                                       const Entry *prev_entry,
                                       EntryPriorityQueue *results) const {
  CHECK(entry);
  CHECK(results);

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

  // |input_key| is a query user is now typing.
  // |entry->key()| is a target value saved in the database.
  //  const string input_key = key_base;

  const MatchType mtype =
      GetMatchTypeFromInput(input_key, key_base, key_expanded, entry->key());
  if (mtype == NO_MATCH) {
    return false;
  } else if (mtype == LEFT_EMPTY_MATCH) {  // zero-query-suggestion
    // if |input_key| is empty, the |prev_entry| and |entry| must
    // have bigram relation.
    if (prev_entry != nullptr && HasBigramEntry(*entry, *prev_entry)) {
      result = AddEntry(*entry, results);
      if (result) {
        last_entry = entry;
        left_last_access_time = entry->last_access_time();
        left_most_last_access_time =
            IsContentWord(entry->value()) ? left_last_access_time : 0;
      }
    } else {
      return false;
    }
  } else if (mtype == LEFT_PREFIX_MATCH) {
    // |input_key| is shorter than |entry->key()|
    // This scenario is a simple prefix match.
    // e.g., |input_key|="foo", |entry->key()|="foobar"
    result = AddEntry(*entry, results);
    if (result) {
      last_entry = entry;
      left_last_access_time = entry->last_access_time();
      left_most_last_access_time =
          IsContentWord(entry->value()) ? left_last_access_time : 0;
    }
  } else if (mtype == RIGHT_PREFIX_MATCH || mtype == EXACT_MATCH) {
    // |input_key| is longer than or the same as |entry->key()|.
    // In this case, recursively traverse "next_entries" until
    // target entry gets longer than input_key.
    // e.g., |input_key|="foobar", |entry->key()|="foo"
    if (request_type == ZERO_QUERY_SUGGESTION && mtype == EXACT_MATCH) {
      // For mobile, we don't generate joined result.
      result = AddEntry(*entry, results);
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
              input_key, entry, &last_entry, &left_last_access_time,
              &left_most_last_access_time, &key, &value)) {
        return false;
      }
      result = AddEntryWithNewKeyValue(std::move(key), std::move(value), *entry,
                                       results);
    }
  } else {
    LOG(ERROR) << "Unknown match mode: " << mtype;
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
    results->Push(result);
  }

  if (request_type == ZERO_QUERY_SUGGESTION) {
    // For mobile, we don't generate joined result.
    return true;
  }

  // Generates joined result using |last_entry|.
  if (last_entry != nullptr && Util::CharsLen(result->key()) >= 1 &&
      2 * Util::CharsLen(input_key) >= Util::CharsLen(result->key())) {
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
          absl::StrCat(result->value(), next_entry->value()), *result, results);
      if (!result2->removed()) {
        results->Push(result2);
      }
    }
  }

  return true;
}

bool UserHistoryPredictor::Predict(Segments *segments) const {
  ConversionRequest default_request;
  default_request.set_request_type(ConversionRequest::PREDICTION);
  return PredictForRequest(default_request, segments);
}

bool UserHistoryPredictor::PredictForRequest(const ConversionRequest &request,
                                             Segments *segments) const {
  const RequestType request_type = request.request().zero_query_suggestion()
                                       ? ZERO_QUERY_SUGGESTION
                                       : DEFAULT;
  if (!ShouldPredict(request_type, request, *segments)) {
    return false;
  }

  const size_t input_key_len =
      Util::CharsLen(segments->conversion_segment(0).key());
  const Entry *prev_entry = LookupPrevEntry(*segments);
  if (input_key_len == 0 && prev_entry == nullptr) {
    MOZC_VLOG(1) << "If input_key_len is 0, prev_entry must be set";
    return false;
  }

  const auto &params = request.request().decoder_experiment_params();

  const bool is_zero_query =
      ((request_type == ZERO_QUERY_SUGGESTION) && (input_key_len == 0));
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

  aggressive_bigram_enabled_ =
      params.user_history_prediction_aggressive_bigram();

  EntryPriorityQueue results;
  GetResultsFromHistoryDictionary(request_type, request, *segments, prev_entry,
                                  max_prediction_size * 5, &results);
  if (results.size() == 0) {
    MOZC_VLOG(2) << "no prefix match candidate is found.";
    return false;
  }

  return InsertCandidates(request_type, request, max_prediction_size,
                          max_prediction_char_coverage, segments, &results);
}

bool UserHistoryPredictor::ShouldPredict(RequestType request_type,
                                         const ConversionRequest &request,
                                         const Segments &segments) const {
  if (!CheckSyncerAndDelete()) {
    LOG(WARNING) << "Syncer is running";
    return false;
  }

  if (request.config().incognito_mode()) {
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

  if (segments.conversion_segments_size() < 1) {
    MOZC_VLOG(2) << "segment size < 1";
    return false;
  }

  if (dic_->empty()) {
    MOZC_VLOG(2) << "dic is empty";
    return false;
  }

  const std::string &input_key = segments.conversion_segment(0).key();
  if (IsPunctuation(Util::Utf8SubString(input_key, 0, 1))) {
    MOZC_VLOG(2) << "input_key starts with punctuations";
    return false;
  }

  const size_t input_key_len = Util::CharsLen(input_key);
  if (input_key_len == 0 && request_type == DEFAULT) {
    MOZC_VLOG(2) << "key length is 0";
    return false;
  }

  return true;
}

const UserHistoryPredictor::Entry *UserHistoryPredictor::LookupPrevEntry(
    const Segments &segments) const {
  const Segments::const_range history_segments = segments.history_segments();
  const Entry *prev_entry = nullptr;
  // When there are non-zero history segments, lookup an entry
  // from the LRU dictionary, which is corresponding to the last
  // history segment.
  if (history_segments.empty()) {
    return nullptr;
  }

  const Segment &history_segment = history_segments.back();

  if (aggressive_bigram_enabled_) {
    // Finds the prev_entry from the longest context.
    // Even when the original value is split into content_value and suffix,
    // longest context information is used.
    std::string all_history_key, all_history_value;
    for (const auto &segment : history_segments) {
      if (segment.candidates_size() == 0) {
        all_history_value.clear();
        all_history_key.clear();
        break;
      }
      absl::StrAppend(&all_history_value, segment.candidate(0).value);
      absl::StrAppend(&all_history_key, segment.candidate(0).key);
    }
    absl::string_view suffix_key = all_history_key;
    absl::string_view suffix_value = all_history_value;
    for (const auto &segment : history_segments) {
      if (suffix_key.empty() || suffix_value.empty()) break;
      prev_entry =
          dic_->LookupWithoutInsert(Fingerprint(suffix_key, suffix_value));
      if (prev_entry) break;
      suffix_value.remove_prefix(segment.candidate(0).value.size());
      suffix_key.remove_prefix(segment.candidate(0).key.size());
    }
  } else {
    prev_entry = dic_->LookupWithoutInsert(SegmentFingerprint(history_segment));
  }

  // Check the timestamp of prev_entry.
  const absl::Time now = Clock::GetAbslTime();
  if (prev_entry != nullptr &&
      absl::FromUnixSeconds(prev_entry->last_access_time()) + k62Days < now) {
    updated_ = true;  // We found an entry to be deleted at next save.
    return nullptr;
  }

  // When |prev_entry| is nullptr or |prev_entry| has no valid next_entries,
  // do linear-search over the LRU.
  if ((prev_entry == nullptr && history_segment.candidates_size() > 0) ||
      (prev_entry != nullptr && prev_entry->next_entries_size() == 0)) {
    const std::string &prev_value = prev_entry == nullptr
                                        ? history_segment.candidate(0).value
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
           absl::EndsWith(prev_value, entry->value()))) {
        prev_entry = entry;
        break;
      }
    }
  }
  return prev_entry;
}

void UserHistoryPredictor::GetResultsFromHistoryDictionary(
    RequestType request_type, const ConversionRequest &request,
    const Segments &segments, const Entry *prev_entry, size_t max_results_size,
    EntryPriorityQueue *results) const {
  DCHECK(results);
  // Gets romanized input key if the given preedit looks misspelled.
  const std::string roman_input_key = GetRomanMisspelledKey(request, segments);

  const std::vector<TypeCorrectedQuery> corrected =
      GetTypingCorrectedQueries(request, segments);

  // If we have ambiguity for the input, get expanded key.
  // Example1 roman input: for "あk", we will get |base|, "あ" and |expanded|,
  // "か", "き", etc
  // Example2 kana input: for "あか", we will get |base|, "あ" and |expanded|,
  // "か", and "が".

  // |base_key| and |input_key| could be different
  // For kana-input, we will expand the ambiguity for "゛".
  // When we input "もす",
  // |base_key|: "も"
  // |expanded|: "す", "ず"
  // |input_key|: "もす"
  // In this case, we want to show candidates for "もす" as EXACT match,
  // and candidates for "もず" as LEFT_PREFIX_MATCH
  //
  // For roman-input, when we input "あｋ",
  // |input_key| is "あｋ" and |base_key| is "あ"
  std::string input_key;
  std::string base_key;
  std::unique_ptr<Trie<std::string>> expanded;
  GetInputKeyFromSegments(request, segments, &input_key, &base_key, &expanded);

  const absl::Time now = Clock::GetAbslTime();
  int trial = 0;
  for (const DicElement &elm : *dic_) {
    // already found enough results.
    if (results->size() >= max_results_size) {
      break;
    }

    if (!IsValidEntryIgnoringRemovedField(elm.value)) {
      continue;
    }
    if (absl::FromUnixSeconds(elm.value.last_access_time()) + k62Days < now) {
      updated_ = true;  // We found an entry to be deleted at next save.
      continue;
    }
    if (request.request_type() == ConversionRequest::SUGGESTION &&
        trial++ >= kMaxSuggestionTrial) {
      MOZC_VLOG(2) << "too many trials";
      break;
    }

    // Lookup key from elm_value and prev_entry.
    // If a new entry is found, the entry is pushed to the results.
    if (LookupEntry(request_type, input_key, base_key, expanded.get(),
                    &(elm.value), prev_entry, results) ||
        RomanFuzzyLookupEntry(roman_input_key, &(elm.value), results) ||
        ZeroQueryLookupEntry(request_type, input_key, &(elm.value), prev_entry,
                             results)) {
      continue;
    }

    // Lookup typing corrected keys when the original `input_key` doesn't match.
    // Since dic_ is sorted in LRU, typing corrected queries are ranked lower
    // than the original key.
    for (const auto &c : corrected) {
      // Only apply when score > 0. When score < 0, we trigger literal-on-top
      // in dictionary predictor.
      if (c.score > 0.0 &&
          LookupEntry(request_type, c.correction, c.correction, nullptr,
                      &(elm.value), prev_entry, results)) {
        break;
      }
    }
  }
}

// static
void UserHistoryPredictor::GetInputKeyFromSegments(
    const ConversionRequest &request, const Segments &segments,
    std::string *input_key, std::string *base,
    std::unique_ptr<Trie<std::string>> *expanded) {
  DCHECK(input_key);
  DCHECK(base);

  if (!request.has_composer()) {
    *input_key = segments.conversion_segment(0).key();
    *base = segments.conversion_segment(0).key();
    return;
  }

  *input_key = request.composer().GetStringForPreedit();
  // auto = std::pair<std::string, absl::btree_set<std::string>>
  const auto [query_base, expanded_set] =
      request.composer().GetQueriesForPrediction();
  *base = std::move(query_base);
  if (!expanded_set.empty()) {
    *expanded = std::make_unique<Trie<std::string>>();
    for (const std::string &expanded_key : expanded_set) {
      // For getting matched key, insert values
      (*expanded)->AddEntry(expanded_key, expanded_key);
    }
  }
}

UserHistoryPredictor::ResultType UserHistoryPredictor::GetResultType(
    const ConversionRequest &request, RequestType request_type,
    bool is_top_candidate, uint32_t input_key_len, const Entry &entry) {
  if (request.request().mixed_conversion()) {
    if (IsValidSuggestionForMixedConversion(request, input_key_len, entry)) {
      return GOOD_RESULT;
    }
    return BAD_RESULT;
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
    if (IsValidSuggestion(request_type, input_key_len, entry)) {
      return GOOD_RESULT;
    }
    if (is_top_candidate) {
      MOZC_VLOG(2) << "candidates size is 0";
      return STOP_ENUMERATION;
    }
    return BAD_RESULT;
  }

  return GOOD_RESULT;
}

bool UserHistoryPredictor::InsertCandidates(RequestType request_type,
                                            const ConversionRequest &request,
                                            size_t max_prediction_size,
                                            size_t max_prediction_char_coverage,
                                            Segments *segments,
                                            EntryPriorityQueue *results) const {
  DCHECK(results);
  Segment *segment = segments->mutable_conversion_segment(0);
  if (segment == nullptr) {
    LOG(ERROR) << "segment is nullptr";
    return false;
  }
  if (request.request_type() != ConversionRequest::SUGGESTION &&
      request.request_type() != ConversionRequest::PREDICTION) {
    LOG(ERROR) << "Unknown mode";
    return false;
  }
  const uint32_t input_key_len = Util::CharsLen(segment->key());

  const int filter_mode =
      request.request()
          .decoder_experiment_params()
          .user_history_prediction_filter_redundant_candidates_mode();

  size_t inserted_num = 0;
  size_t inserted_char_coverage = 0;

  std::vector<const UserHistoryPredictor::Entry *> entries;
  entries.reserve(results->size());

  // Bit fields to specify the filtering mode.
  enum FilterMode {
    // Current LRU-based candidates = ["東京"]
    //  target="東京は" -> Remove.   (target is longer)
    //  target="東"     -> Keep.     (target is shorter)
    FILTER_LONG_ENTRY = 1,

    // Current LRU-based candidates = ["東京は"]
    //  target="東京"     -> Remove. (target is shorter)
    //  target="東京はが" -> Keep.   (target is longer)
    FILTER_SHORT_ENTRY = 2,

    // Current LRU-based candidates = ["東京は"]
    //   target="東京" -> Replace "東京は" and "東京"
    // FILTER_SHORT_ENTRY and REPLACE_SHORT_ENTRY are exclusive.
    REPLACE_SHORT_ENTRY = 4,

    // - Non-shared suffix can be any script type.
    //   Note that this flag is for prefix match.
    //  Current LRU-based candidates = ["東京"]
    //   target="東京タワー" -> Remove. (suffix can be any type)
    SUFFIX_IS_ALL_CHAR_TYPE = 8,
  };

  auto starts_with = [&filter_mode](absl::string_view text,
                                    absl::string_view prefix) {
    if (filter_mode & SUFFIX_IS_ALL_CHAR_TYPE) {
      return absl::StartsWith(text, prefix);
    }
    return absl::StartsWith(text, prefix) &&
           Util::IsScriptType(text.substr(prefix.size()), Util::HIRAGANA);
  };

  auto is_redandant = [&](absl::string_view target,
                          absl::string_view inserted) {
    return ((filter_mode & FILTER_LONG_ENTRY) &&
            starts_with(target, inserted)) ||
           ((filter_mode & FILTER_SHORT_ENTRY) &&
            starts_with(inserted, target));
  };

  auto is_redandant_entry = [&](const Entry &entry) {
    return absl::c_find_if(entries, [&](const Entry *inserted_entry) {
             return is_redandant(entry.value(), inserted_entry->value());
           }) != entries.end();
  };

  // Replace `entry` with the one entry in `entries`.
  auto maybe_replace_entry = [&](const Entry *entry) {
    if (!(filter_mode & REPLACE_SHORT_ENTRY)) return false;

    auto it = absl::c_find_if(entries, [&](const Entry *inserted_entry) {
      return starts_with(inserted_entry->value(), entry->value());
    });
    if (it == entries.end()) return false;

    inserted_char_coverage -= Util::CharsLen((*it)->value());
    inserted_char_coverage += Util::CharsLen(entry->value());
    *it = entry;

    return true;
  };

  while (inserted_num < max_prediction_size) {
    // |results| is a priority queue where the element
    // in the queue is sorted by the score defined in GetScore().
    const Entry *result_entry = results->Pop();
    if (result_entry == nullptr) {
      // Pop() returns nullptr when no more valid entry exists.
      break;
    }

    const ResultType result =
        GetResultType(request, request_type, segment->candidates_size() == 0,
                      input_key_len, *result_entry);
    if (result == STOP_ENUMERATION) {
      break;
    } else if (result == BAD_RESULT) {
      continue;
    }

    // Check candidate redundancy.
    if (filter_mode > 0 && (is_redandant_entry(*result_entry) ||
                            maybe_replace_entry(result_entry))) {
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

  for (const auto *result_entry : entries) {
    Segment::Candidate *candidate = segment->push_back_candidate();
    DCHECK(candidate);
    candidate->key = result_entry->key();
    candidate->content_key = result_entry->key();
    candidate->value = result_entry->value();
    candidate->content_value = result_entry->value();
    candidate->attributes |= Segment::Candidate::USER_HISTORY_PREDICTION |
                             Segment::Candidate::NO_VARIANTS_EXPANSION;
    candidate->source_info |= Segment::Candidate::USER_HISTORY_PREDICTOR;
    if (result_entry->spelling_correction()) {
      candidate->attributes |= Segment::Candidate::SPELLING_CORRECTION;
    }
    const std::string &description = result_entry->description();
    // If we have stored description, set it exactly.
    if (!description.empty()) {
      candidate->description = description;
      candidate->attributes |= Segment::Candidate::NO_EXTRA_DESCRIPTION;
    } else {
      VariantsRewriter::SetDescriptionForPrediction(*pos_matcher_, candidate);
    }
    MOZC_CANDIDATE_LOG(candidate,
                       "Added by UserHistoryPredictor::InsertCandidates");
#if DEBUG
    if (!absl::StrContains(candidate->description, "History")) {
      candidate->description += " History";
    }
#endif  // DEBUG
  }

  return !entries.empty();
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
  if (entry.entry_type() != Entry::DEFAULT_ENTRY ||
      suppression_dictionary_->SuppressEntry(entry.key(), entry.value())) {
    return false;
  }

  if (IsEmojiEntry(entry) && IsAndroidPuaEmoji(entry.value())) {
    return false;
  }

  if (absl::EndsWith(entry.key(), " ")) {
    // Invalid user history entry from alphanumeric input.
    return false;
  }

  return true;
}

void UserHistoryPredictor::InsertEvent(EntryType type) {
  if (type == Entry::DEFAULT_ENTRY) {
    return;
  }

  const uint64_t last_access_time = absl::ToUnixSeconds(Clock::GetAbslTime());
  const uint32_t dic_key = Fingerprint("", "", type);

  CHECK(dic_.get());
  DicElement *e = dic_->Insert(dic_key);
  if (e == nullptr) {
    MOZC_VLOG(2) << "insert failed";
    return;
  }

  Entry *entry = &(e->value);
  DCHECK(entry);
  entry->Clear();
  entry->set_entry_type(type);
  entry->set_last_access_time(last_access_time);
}

bool UserHistoryPredictor::ShouldInsert(
    RequestType request_type, absl::string_view key, absl::string_view value,
    const absl::string_view description) const {
  if (key.empty() || value.empty() || key.size() > kMaxStringLength ||
      value.size() > kMaxStringLength ||
      description.size() > kMaxStringLength) {
    return false;
  }

  // For mobile, we do not learn candidates that ends with punctuation.
  if (request_type == ZERO_QUERY_SUGGESTION && Util::CharsLen(value) > 1 &&
      IsPunctuation(Util::Utf8SubString(value, Util::CharsLen(value) - 1, 1))) {
    return false;
  }
  return true;
}

void UserHistoryPredictor::TryInsert(
    RequestType request_type, absl::string_view key, absl::string_view value,
    absl::string_view description, bool is_suggestion_selected,
    absl::Span<const uint32_t> next_fps, uint64_t last_access_time,
    Segments *segments) {
  // b/279560433: Preprocess key value
  key = absl::StripTrailingAsciiWhitespace(key);
  value = absl::StripTrailingAsciiWhitespace(value);
  if (ShouldInsert(request_type, key, value, description)) {
    Insert(std::string(key), std::string(value), std::string(description),
           is_suggestion_selected, next_fps, last_access_time, segments);
  }
}

void UserHistoryPredictor::Insert(std::string key, std::string value,
                                  std::string description,
                                  bool is_suggestion_selected,
                                  absl::Span<const uint32_t> next_fps,
                                  uint64_t last_access_time,
                                  Segments *segments) {
  const uint32_t dic_key = Fingerprint(key, value);

  if (!dic_->HasKey(dic_key)) {
    // The key is a new key inserted in the last Finish method.
    // Here we push a new RevertEntry so that the new "key" can be
    // removed when Revert() method is called.
    Segments::RevertEntry *revert_entry = segments->push_back_revert_entry();
    DCHECK(revert_entry);
    revert_entry->key = Uint32ToString(dic_key);
    revert_entry->id = UserHistoryPredictor::revert_id();
    revert_entry->revert_entry_type = Segments::RevertEntry::CREATE_ENTRY;
  } else {
    // The key is a old key not inserted in the last Finish method
    // TODO(taku):
    // add a treatment for UPDATE_ENTRY mode
  }

  DicElement *e = dic_->Insert(dic_key);
  if (e == nullptr) {
    MOZC_VLOG(2) << "insert failed";
    return;
  }

  Entry *entry = &(e->value);
  DCHECK(entry);

  entry->set_key(std::move(key));
  entry->set_value(std::move(value));
  entry->set_removed(false);

  if (description.empty()) {
    entry->clear_description();
  } else {
    entry->set_description(std::move(description));
  }

  entry->set_last_access_time(last_access_time);
  if (is_suggestion_selected) {
    entry->set_suggestion_freq(entry->suggestion_freq() + 1);
  } else {
    entry->set_conversion_freq(entry->conversion_freq() + 1);
  }

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

void UserHistoryPredictor::MaybeRecordUsageStats(
    const Segments &segments) const {
  const Segment &segment = segments.conversion_segment(0);
  if (segment.candidates_size() < 1) {
    MOZC_VLOG(2) << "candidates size < 1";
    return;
  }

  const Segment::Candidate &candidate = segment.candidate(0);
  if (segment.segment_type() != Segment::FIXED_VALUE) {
    MOZC_VLOG(2) << "segment is not FIXED_VALUE" << candidate.value;
    return;
  }

  if (!(candidate.source_info & Segment::Candidate::USER_HISTORY_PREDICTOR)) {
    MOZC_VLOG(2) << "candidate is not from user_history_predictor";
    return;
  }

  UsageStats::IncrementCount("CommitUserHistoryPredictor");
  if (segment.key().empty()) {
    UsageStats::IncrementCount("CommitUserHistoryPredictorZeroQuery");
  }
}

void UserHistoryPredictor::MaybeRemoveUnselectedHistory(
    const Segments &segments) {
  const Segment &segment = segments.conversion_segment(0);
  if (segment.candidates_size() < 1 ||
      segment.segment_type() != Segment::FIXED_VALUE) {
    return;
  }

  static constexpr size_t kMaxHistorySize = 5;
  static constexpr float kMinSelectedRatio = 0.05;
  for (size_t i = 0; i < std::min(segment.candidates_size(), kMaxHistorySize);
       ++i) {
    const Segment::Candidate &candidate = segment.candidate(i);
    Entry *entry = dic_->MutableLookupWithoutInsert(
        Fingerprint(candidate.key, candidate.value));
    if (entry == nullptr) {
      continue;
    }
    // Note(b/339742825): For now shown freq is only used here and it's OK to
    // increment the value here.
    entry->set_shown_freq(entry->shown_freq() + 1);

    const float selected_ratio =
        1.0 * std::max(entry->suggestion_freq(), entry->conversion_freq()) /
        entry->shown_freq();
    if (selected_ratio < kMinSelectedRatio) {
      entry->set_suggestion_freq(0);
      entry->set_conversion_freq(0);
      entry->set_shown_freq(0);
      entry->set_removed(true);
      continue;
    }
  }
}

void UserHistoryPredictor::Finish(const ConversionRequest &request,
                                  Segments *segments) {
  if (request.request_type() == ConversionRequest::REVERSE_CONVERSION) {
    // Do nothing for REVERSE_CONVERSION.
    return;
  }

  if (request.config().incognito_mode()) {
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

  aggressive_bigram_enabled_ = request.request()
                                   .decoder_experiment_params()
                                   .user_history_prediction_aggressive_bigram();

  if (!CheckSyncerAndDelete()) {
    LOG(WARNING) << "Syncer is running";
    return;
  }

  MaybeRecordUsageStats(*segments);

  const RequestType request_type = request.request().zero_query_suggestion()
                                       ? ZERO_QUERY_SUGGESTION
                                       : DEFAULT;
  const bool is_suggestion =
      request.request_type() != ConversionRequest::CONVERSION;
  const uint64_t last_access_time = absl::ToUnixSeconds(Clock::GetAbslTime());

  // If user inputs a punctuation just after some long sentence,
  // we make a new candidate by concatenating the top element in LRU and
  // the punctuation user input. The top element in LRU is supposed to be
  // the long sentence user input before.
  // This is a fix for http://b/issue?id=2216838
  //
  // Note: We don't make such candidates for mobile.
  if (request_type != ZERO_QUERY_SUGGESTION && !dic_->empty() &&
      dic_->Head()->value.last_access_time() + 5 > last_access_time &&
      // Check if the current value is a punctuation.
      segments->conversion_segments_size() == 1 &&
      segments->conversion_segment(0).candidates_size() > 0 &&
      IsPunctuation(segments->conversion_segment(0).candidate(0).value) &&
      // Check if the previous value looks like a sentence.
      !segments->history_segments().empty() &&
      segments->history_segments().back().candidates_size() > 0 &&
      IsSentenceLikeCandidate(
          segments->history_segments().back().candidate(0))) {
    const Entry *entry = &(dic_->Head()->value);
    DCHECK(entry);
    const std::string &last_value =
        segments->history_segments().back().candidate(0).value;
    // Check if the head value in LRU ends with the candidate value in history
    // segments.
    if (absl::EndsWith(entry->value(), last_value)) {
      const Segment::Candidate &candidate =
          segments->conversion_segment(0).candidate(0);
      // Uses the same last_access_time stored in the top element
      // so that this item can be grouped together.
      TryInsert(
          std::move(request_type), absl::StrCat(entry->key(), candidate.key),
          absl::StrCat(entry->value(), candidate.value), entry->description(),
          is_suggestion, {}, entry->last_access_time(), segments);
    }
  }

  // Checks every segment is valid.
  for (const Segment &segment : segments->conversion_segments()) {
    if (segment.candidates_size() < 1) {
      MOZC_VLOG(2) << "candidates size < 1";
      return;
    }
    if (segment.segment_type() != Segment::FIXED_VALUE) {
      MOZC_VLOG(2) << "segment is not FIXED_VALUE";
      return;
    }
    const Segment::Candidate &candidate = segment.candidate(0);
    if (candidate.attributes & Segment::Candidate::NO_SUGGEST_LEARNING) {
      MOZC_VLOG(2) << "NO_SUGGEST_LEARNING";
      return;
    }
  }

  if (IsPrivacySensitive(segments)) {
    MOZC_VLOG(2) << "do not remember privacy sensitive input";
    return;
  }

  InsertHistory(request_type, is_suggestion, last_access_time, segments);

  MaybeRemoveUnselectedHistory(*segments);
}

UserHistoryPredictor::SegmentsForLearning
UserHistoryPredictor::MakeLearningSegments(const Segments &segments) const {
  SegmentsForLearning learning_segments;

  for (const Segment &segment : segments.history_segments()) {
    DCHECK_LE(1, segment.candidates_size());
    auto &candidate = segment.candidate(0);
    learning_segments.history_segments.push_back(
        {candidate.key, candidate.value, candidate.content_key,
         candidate.content_value, GetDescription(candidate)});
  }

  std::string all_key, all_value;
  for (const Segment &segment : segments.conversion_segments()) {
    const Segment::Candidate &candidate = segment.candidate(0);
    absl::StrAppend(&all_key, candidate.key);
    absl::StrAppend(&all_value, candidate.value);
    if (candidate.inner_segment_boundary.empty()) {
      learning_segments.conversion_segments.push_back(
          {candidate.key, candidate.value, candidate.content_key,
           candidate.content_value, GetDescription(candidate)});
    } else {
      for (Segment::Candidate::InnerSegmentIterator iter(&candidate);
           !iter.Done(); iter.Next()) {
        learning_segments.conversion_segments.push_back(
            {std::string(iter.GetKey()), std::string(iter.GetValue()),
             std::string(iter.GetContentKey()),
             std::string(iter.GetContentValue()), ""});
      }
    }
  }

  learning_segments.conversion_segments_key = std::move(all_key);
  learning_segments.conversion_segments_value = std::move(all_value);

  return learning_segments;
}

void UserHistoryPredictor::InsertHistory(RequestType request_type,
                                         bool is_suggestion_selected,
                                         uint64_t last_access_time,
                                         Segments *segments) {
  const SegmentsForLearning learning_segments = MakeLearningSegments(*segments);

  InsertHistoryForConversionSegments(request_type, is_suggestion_selected,
                                     last_access_time, learning_segments,
                                     segments);

  const std::string &all_key = learning_segments.conversion_segments_key;
  const std::string &all_value = learning_segments.conversion_segments_value;

  // Inserts all_key/all_value.
  // We don't insert it for mobile.
  if (request_type != ZERO_QUERY_SUGGESTION &&
      learning_segments.conversion_segments.size() > 1 && !all_key.empty() &&
      !all_value.empty()) {
    TryInsert(request_type, all_key, all_value, "", is_suggestion_selected, {},
              last_access_time, segments);
  }

  // Makes a link from the last history_segment to the first conversion segment
  // or to the entire user input.
  if (!learning_segments.history_segments.empty() &&
      !learning_segments.conversion_segments.empty()) {
    const SegmentForLearning &history_segment =
        learning_segments
            .history_segments[segments->history_segments_size() - 1];
    const SegmentForLearning &conversion_segment =
        learning_segments.conversion_segments[0];
    const std::string &history_value = history_segment.value;
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
        (request_type != ZERO_QUERY_SUGGESTION ||
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
    RequestType request_type, bool is_suggestion_selected,
    uint64_t last_access_time, const SegmentsForLearning &learning_segments,
    Segments *segments) {
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
    TryInsert(request_type, segment.key, segment.value, segment.description,
              is_suggestion_selected, next_fps_to_set, last_access_time,
              segments);
    if (content_word_learning_enabled_ && segment.content_key != segment.key &&
        segment.content_value != segment.value) {
      TryInsert(request_type, segment.content_key, segment.content_value,
                segment.description, is_suggestion_selected, {},
                last_access_time, segments);
    }
  }
}

void UserHistoryPredictor::Revert(Segments *segments) {
  if (!CheckSyncerAndDelete()) {
    LOG(WARNING) << "Syncer is running";
    return;
  }

  for (size_t i = 0; i < segments->revert_entries_size(); ++i) {
    const Segments::RevertEntry &revert_entry = segments->revert_entry(i);
    if (revert_entry.id == UserHistoryPredictor::revert_id() &&
        revert_entry.revert_entry_type == Segments::RevertEntry::CREATE_ENTRY) {
      const uint32_t key = LoadUnaligned<uint32_t>(revert_entry.key.data());
      MOZC_VLOG(2) << "Erasing the key: " << key;
      dic_->Erase(key);
    }
  }
}

// static
UserHistoryPredictor::MatchType UserHistoryPredictor::GetMatchType(
    const absl::string_view lstr, const absl::string_view rstr) {
  if (lstr.empty() && !rstr.empty()) {
    return LEFT_EMPTY_MATCH;
  }

  const size_t size = std::min(lstr.size(), rstr.size());
  if (size == 0) {
    return NO_MATCH;
  }

  if (lstr.substr(0, size) != rstr.substr(0, size)) {
    return NO_MATCH;
  }

  if (lstr.size() == rstr.size()) {
    return EXACT_MATCH;
  } else if (lstr.size() < rstr.size()) {
    return LEFT_PREFIX_MATCH;
  } else {
    return RIGHT_PREFIX_MATCH;
  }

  return NO_MATCH;
}

// static
UserHistoryPredictor::MatchType UserHistoryPredictor::GetMatchTypeFromInput(
    const absl::string_view input_key, const absl::string_view key_base,
    const Trie<std::string> *key_expanded, const absl::string_view target) {
  if (key_expanded == nullptr) {
    // |input_key| and |key_base| can be different by composer modification.
    // For example, |input_key|, "８，＋", and |base| "８、＋".
    return GetMatchType(key_base, target);
  }

  // We can assume key_expanded != nullptr from here.
  if (key_base.empty()) {
    std::string value;
    size_t key_length = 0;
    bool has_subtrie = false;
    if (!key_expanded->LookUpPrefix(target, &value, &key_length,
                                    &has_subtrie)) {
      return NO_MATCH;
    } else if (value == target && value == input_key) {
      return EXACT_MATCH;
    } else {
      return LEFT_PREFIX_MATCH;
    }
  } else {
    const size_t size = std::min(key_base.size(), target.size());
    if (size == 0) {
      return NO_MATCH;
    }
    if (key_base.substr(0, size) != target.substr(0, size)) {
      return NO_MATCH;
    }
    if (target.size() <= key_base.size()) {
      return RIGHT_PREFIX_MATCH;
    }
    std::string value;
    size_t key_length = 0;
    bool has_subtrie = false;
    if (!key_expanded->LookUpPrefix(target.data() + key_base.size(), &value,
                                    &key_length, &has_subtrie)) {
      return NO_MATCH;
    }
    const std::string matched = absl::StrCat(key_base, value);
    if (matched == target && matched == input_key) {
      return EXACT_MATCH;
    } else {
      return LEFT_PREFIX_MATCH;
    }
  }

  DCHECK(false) << "Should not come here";
  return NO_MATCH;
}

// static
uint32_t UserHistoryPredictor::Fingerprint(const absl::string_view key,
                                           const absl::string_view value,
                                           EntryType type) {
  if (type == Entry::DEFAULT_ENTRY) {
    // Since we have already used the fingerprint function for next entries and
    // next entries are saved in user's local machine, we are not able
    // to change the Fingerprint function for the old key/value type.
    return Fingerprint32(absl::StrCat(key, kDelimiter, value));
  } else {
    return Fingerprint32(static_cast<uint8_t>(type));
  }
}

// static
uint32_t UserHistoryPredictor::Fingerprint(const absl::string_view key,
                                           const absl::string_view value) {
  return Fingerprint(key, value, Entry::DEFAULT_ENTRY);
}

uint32_t UserHistoryPredictor::EntryFingerprint(const Entry &entry) {
  return Fingerprint(entry.key(), entry.value());
}

// static
uint32_t UserHistoryPredictor::SegmentFingerprint(const Segment &segment) {
  if (segment.candidates_size() > 0) {
    return Fingerprint(segment.candidate(0).key, segment.candidate(0).value);
  }
  return 0;
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
  if (aggressive_bigram_enabled_ && segment.key != segment.content_key) {
    fps.push_back(Fingerprint(segment.content_key, segment.content_value));
  }
  return fps;
}

// static
std::string UserHistoryPredictor::Uint32ToString(uint32_t fp) {
  std::string buf(reinterpret_cast<const char *>(&fp), sizeof(fp));
  return buf;
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
bool UserHistoryPredictor::IsValidSuggestion(RequestType request_type,
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
  if (request_type == ZERO_QUERY_SUGGESTION) {
    return true;
  }
  // Handles suggestion_freq and conversion_freq differently.
  // conversion_freq is less aggressively affecting to the final decision.
  const uint32_t freq =
      std::max(entry.suggestion_freq(), entry.conversion_freq() / 4);
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

// Returns the size of cache.
// static
uint32_t UserHistoryPredictor::cache_size() { return kLruCacheSize; }

// Returns the size of next entries.
uint32_t UserHistoryPredictor::max_next_entries_size() const {
  return aggressive_bigram_enabled_ ? 6 : kMaxNextEntriesSize;
}

}  // namespace mozc::prediction
