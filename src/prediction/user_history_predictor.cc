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
#include <cctype>
#include <climits>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>

#include "base/clock.h"
#include "base/config_file_stream.h"
#include "base/hash.h"
#include "base/logging.h"
#include "base/thread.h"
#include "base/trie.h"
#include "base/util.h"
#include "composer/composer.h"
#include "converter/segments.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suppression_dictionary.h"
#include "prediction/predictor_interface.h"
#include "prediction/user_history_predictor.pb.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "rewriter/variants_rewriter.h"
#include "storage/encrypted_string_storage.h"
#include "storage/lru_cache.h"
#include "usage_stats/usage_stats.h"
#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/memory/memory.h"
#include "absl/strings/match.h"
#include "absl/strings/str_join.h"

namespace mozc {
namespace {

using commands::Request;
using dictionary::DictionaryInterface;
using dictionary::PosMatcher;
using dictionary::SuppressionDictionary;
using usage_stats::UsageStats;

// Finds suggestion candidates from the most recent 3000 history in LRU.
// We don't check all history, since suggestion is called every key event
constexpr size_t kMaxSuggestionTrial = 3000;

// Finds suffix matches of history_segments from the most recent 500 histories
// in LRU.
constexpr size_t kMaxPrevValueTrial = 500;

// Cache size
// Typically memory/storage footprint becomes kLruCacheSize * 70 bytes.
#ifdef OS_ANDROID
constexpr size_t kLruCacheSize = 4000;
#else   // OS_ANDROID
constexpr size_t kLruCacheSize = 10000;
#endif  // OS_ANDROID

// Don't save key/value that are
// longer than kMaxCandidateSize to avoid memory explosion
constexpr size_t kMaxStringLength = 256;

// Maximum size of next_entries
constexpr size_t kMaxNextEntriesSize = 4;

// Revert id for user_history_predictor
const uint16_t kRevertId = 1;

// Default object pool size for EntryPriorityQueue
constexpr size_t kEntryPoolSize = 16;

// File name for the history
#ifdef OS_WIN
constexpr char kFileName[] = "user://history.db";
#else   // OS_WIN
constexpr char kFileName[] = "user://.history.db";
#endif  // OS_WIN

// Uses '\t' as a key/value delimiter
constexpr char kDelimiter[] = "\t";
constexpr char kEmojiDescription[] = "絵文字";

const uint64_t k62DaysInSec = 62 * 24 * 60 * 60;

// TODO(peria, hidehiko): Unify this checker and IsEmojiCandidate in
//     EmojiRewriter.  If you make similar functions before the merging in
//     case, put a similar note to avoid twisted dependency.
bool IsEmojiEntry(const UserHistoryPredictor::Entry &entry) {
  return (entry.has_description() &&
          absl::StrContains(entry.description(), kEmojiDescription));
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
std::string ToRoman(const std::string &str) {
  std::string result;
  Util::HiraganaToRomanji(str, &result);
  return result;
}

// Returns true if value looks like a content word.
// Currently, just checks the script type.
bool IsContentWord(const std::string &value) {
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

UserHistoryStorage::UserHistoryStorage(const std::string &filename)
    : storage_(new storage::EncryptedStringStorage(filename)) {}

UserHistoryStorage::~UserHistoryStorage() {}

bool UserHistoryStorage::Load() {
  std::string input;
  if (!storage_->Load(&input)) {
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

  VLOG(1) << "Loaded user history, size=" << proto_.entries_size();
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

  if (!storage_->Save(output)) {
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
  const uint64_t now = Clock::GetTime();
  const uint64_t timestamp = (now > k62DaysInSec) ? now - k62DaysInSec : 0;
  return DeleteEntriesBefore(timestamp);
}

UserHistoryPredictor::EntryPriorityQueue::EntryPriorityQueue()
    : pool_(kEntryPoolSize) {}

UserHistoryPredictor::EntryPriorityQueue::~EntryPriorityQueue() {}

bool UserHistoryPredictor::EntryPriorityQueue::Push(Entry *entry) {
  DCHECK(entry);
  if (!seen_.insert(Hash::Fingerprint32(entry->value())).second) {
    VLOG(2) << "found dups";
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

class UserHistoryPredictorSyncer : public Thread {
 public:
  enum RequestType { LOAD, SAVE };

  UserHistoryPredictorSyncer(UserHistoryPredictor *predictor, RequestType type)
      : predictor_(predictor), type_(type) {
    DCHECK(predictor_);
  }

  void Run() override {
    switch (type_) {
      case LOAD:
        VLOG(1) << "Executing Reload method";
        predictor_->Load();
        break;
      case SAVE:
        VLOG(1) << "Executing Sync method";
        predictor_->Save();
        break;
      default:
        LOG(ERROR) << "Unknown request: " << static_cast<int>(type_);
    }
  }

  ~UserHistoryPredictorSyncer() override { Join(); }

  UserHistoryPredictor *predictor_;
  RequestType type_;
};

UserHistoryPredictor::UserHistoryPredictor(
    const DictionaryInterface *dictionary, const PosMatcher *pos_matcher,
    const SuppressionDictionary *suppression_dictionary,
    bool enable_content_word_learning)
    : dictionary_(dictionary),
      pos_matcher_(pos_matcher),
      suppression_dictionary_(suppression_dictionary),
      predictor_name_("UserHistoryPredictor"),
      content_word_learning_enabled_(enable_content_word_learning),
      updated_(false),
      dic_(new DicCache(UserHistoryPredictor::cache_size())) {
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
  if (syncer_ != nullptr) {
    syncer_->Join();
    syncer_.reset();
  }
}

bool UserHistoryPredictor::Wait() {
  WaitForSyncer();
  return true;
}

bool UserHistoryPredictor::CheckSyncerAndDelete() const {
  if (syncer_ != nullptr) {
    if (syncer_->IsRunning()) {
      return false;
    } else {
      syncer_.reset();
    }
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

  syncer_ = absl::make_unique<UserHistoryPredictorSyncer>(
      this, UserHistoryPredictorSyncer::LOAD);
  syncer_->Start("UserHistoryPredictor:Load");

  return true;
}

bool UserHistoryPredictor::AsyncSave() {
  if (!updated_) {
    return true;
  }

  if (!CheckSyncerAndDelete()) {  // now loading/saving
    return true;
  }

  syncer_ = absl::make_unique<UserHistoryPredictorSyncer>(
      this, UserHistoryPredictorSyncer::SAVE);
  syncer_->Start("UserHistoryPredictor:Save");

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
  for (size_t i = 0; i < history.GetProto().entries_size(); ++i) {
    dic_->Insert(EntryFingerprint(history.GetProto().entries(i)),
                 history.GetProto().entries(i));
  }

  VLOG(1) << "Loaded user history, size=" << history.GetProto().entries_size();

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

  VLOG(1) << "Clearing user prediction";
  // Renews DicCache as LruCache tries to reuse the internal value by
  // using FreeList
  dic_ = absl::make_unique<DicCache>(UserHistoryPredictor::cache_size());

  // insert a dummy event entry.
  InsertEvent(Entry::CLEAN_ALL_EVENT);

  updated_ = true;

  Sync();

  return true;
}

bool UserHistoryPredictor::ClearUnusedHistory() {
  // Waits until syncer finishes
  WaitForSyncer();

  VLOG(1) << "Clearing unused prediction";
  const DicElement *head = dic_->Head();
  if (head == nullptr) {
    VLOG(2) << "dic head is nullptr";
    return false;
  }

  std::vector<uint32_t> keys;
  for (const DicElement *elm = head; elm != nullptr; elm = elm->next) {
    VLOG(3) << elm->key << " " << elm->value.suggestion_freq();
    if (elm->value.suggestion_freq() == 0) {
      keys.push_back(elm->key);
    }
  }

  for (size_t i = 0; i < keys.size(); ++i) {
    VLOG(2) << "Removing: " << keys[i];
    if (!dic_->Erase(keys[i])) {
      LOG(ERROR) << "cannot erase " << keys[i];
    }
  }

  // Inserts a dummy event entry.
  InsertEvent(Entry::CLEAN_UNUSED_EVENT);

  updated_ = true;

  Sync();

  VLOG(1) << keys.size() << " removed";

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
    const std::string &target_key, const std::string &target_value,
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

bool UserHistoryPredictor::ClearHistoryEntry(const std::string &key,
                                             const std::string &value) {
  bool deleted = false;
  {
    // Finds the history entry that has the exactly same key and value and has
    // not been removed yet. If exists, remove it.
    Entry *entry = dic_->MutableLookupWithoutInsert(Fingerprint(key, value));
    if (entry != nullptr && !entry->removed()) {
      entry->set_suggestion_freq(0);
      entry->set_conversion_freq(0);
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
    for (DicElement *elm = dic_->MutableHead(); elm != nullptr;
         elm = elm->next) {
      Entry *entry = &elm->value;
      if (!Util::StartsWith(key, entry->key()) ||
          !Util::StartsWith(value, entry->value())) {
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

// static
bool UserHistoryPredictor::MaybeRomanMisspelledKey(const std::string &key) {
  int num_alpha = 0;
  int num_hiragana = 0;
  int num_unknown = 0;
  for (ConstChar32Iterator iter(key); !iter.Done(); iter.Next()) {
    const char32 w = iter.Get();
    const Util::ScriptType type = Util::GetScriptType(w);
    if (type == Util::HIRAGANA || w == 0x30FC) {  // "ー".
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
bool UserHistoryPredictor::RomanFuzzyPrefixMatch(const std::string &str,
                                                 const std::string &prefix) {
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
        std::string replaced_prefix = prefix;
        replaced_prefix[i] = str[i];
        if (Util::StartsWith(str, replaced_prefix)) {
          return true;
        }
      }
    } else {
      // deletion.
      std::string inserted_prefix = prefix;
      inserted_prefix.insert(i, 1, str[i]);
      if (Util::StartsWith(str, inserted_prefix)) {
        return true;
      }

      // swap.
      if (i + 1 < prefix.size()) {
        std::string swapped_prefix = prefix;
        using std::swap;
        swap(swapped_prefix[i], swapped_prefix[i + 1]);
        if (Util::StartsWith(str, swapped_prefix)) {
          return true;
        }
      }
    }

    return false;
  }

  // |prefix| is an exact suffix of |str|.
  return false;
}

bool UserHistoryPredictor::RomanFuzzyLookupEntry(
    const std::string &roman_input_key, const Entry *entry,
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
    const std::string &key, const std::string &value, const Entry &entry,
    EntryPriorityQueue *results) const {
  // We add an entry even if it was marked as removed so that it can be used to
  // generate prediction by entry chaining. The deleted entry itself is never
  // shown in the final prediction result as it is filtered finally.
  Entry *new_entry = results->NewEntry();
  *new_entry = entry;
  new_entry->set_key(key);
  new_entry->set_value(value);

  // Sets removed field true if the new key and value were removed.
  const Entry *e = dic_->LookupWithoutInsert(Fingerprint(key, value));
  new_entry->set_removed(e != nullptr && e->removed());

  return new_entry;
}

bool UserHistoryPredictor::GetKeyValueForExactAndRightPrefixMatch(
    const std::string &input_key, const Entry *entry,
    const Entry **result_last_entry, uint64_t *left_last_access_time,
    uint64_t *left_most_last_access_time, std::string *result_key,
    std::string *result_value) const {
  std::string key = entry->key();
  std::string value = entry->value();
  const Entry *current_entry = entry;
  absl::flat_hash_set<uint32_t> seen;
  seen.insert(EntryFingerprint(*current_entry));
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
          GetMatchType(key + tmp_next_entry->key(), input_key);
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
    if (!seen.insert(EntryFingerprint(*next_entry)).second) {
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
    VLOG(3) << "Cannot find prefix match even after chain rules";
    return false;
  }

  *result_key = key;
  *result_value = value;
  return true;
}

bool UserHistoryPredictor::LookupEntry(RequestType request_type,
                                       const std::string &input_key,
                                       const std::string &key_base,
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
      result = AddEntryWithNewKeyValue(key, value, *entry, results);
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
          result->key() + next_entry->key(),
          result->value() + next_entry->value(), *result, results);
      if (!result2->removed()) {
        results->Push(result2);
      }
    }
  }

  return true;
}

bool UserHistoryPredictor::Predict(Segments *segments) const {
  ConversionRequest default_request;
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
  const Entry *prev_entry =
      LookupPrevEntry(*segments, request.request().available_emoji_carrier());
  if (input_key_len == 0 && prev_entry == nullptr) {
    VLOG(1) << "If input_key_len is 0, prev_entry must be set";
    return false;
  }

  const bool is_zero_query =
      ((request_type == ZERO_QUERY_SUGGESTION) && (input_key_len == 0));
  const size_t max_prediction_size =
      is_zero_query
          ? request.max_user_history_prediction_candidates_size_for_zero_query()
          : request.max_user_history_prediction_candidates_size();

  EntryPriorityQueue results;
  GetResultsFromHistoryDictionary(request_type, request, *segments, prev_entry,
                                  max_prediction_size * 5, &results);
  if (results.size() == 0) {
    VLOG(2) << "no prefix match candidate is found.";
    return false;
  }

  return InsertCandidates(request_type, request, max_prediction_size, segments,
                          &results);
}

bool UserHistoryPredictor::ShouldPredict(RequestType request_type,
                                         const ConversionRequest &request,
                                         const Segments &segments) const {
  if (!CheckSyncerAndDelete()) {
    LOG(WARNING) << "Syncer is running";
    return false;
  }

  if (request.config().incognito_mode()) {
    VLOG(2) << "incognito mode";
    return false;
  }

  if (request.config().history_learning_level() == config::Config::NO_HISTORY) {
    VLOG(2) << "history learning level is NO_HISTORY";
    return false;
  }

  if (segments.request_type() == Segments::CONVERSION) {
    VLOG(2) << "request type is CONVERSION";
    return false;
  }

  if (!request.config().use_history_suggest() &&
      segments.request_type() == Segments::SUGGESTION) {
    VLOG(2) << "no history suggest";
    return false;
  }

  if (segments.conversion_segments_size() < 1) {
    VLOG(2) << "segment size < 1";
    return false;
  }

  if (dic_->Head() == nullptr) {
    VLOG(2) << "dic head is nullptr";
    return false;
  }

  const std::string &input_key = segments.conversion_segment(0).key();
  if (IsPunctuation(Util::Utf8SubString(input_key, 0, 1))) {
    VLOG(2) << "input_key starts with punctuations";
    return false;
  }

  const size_t input_key_len = Util::CharsLen(input_key);
  if (input_key_len == 0 && request_type == DEFAULT) {
    VLOG(2) << "key length is 0";
    return false;
  }

  return true;
}

const UserHistoryPredictor::Entry *UserHistoryPredictor::LookupPrevEntry(
    const Segments &segments, uint32_t available_emoji_carrier) const {
  const size_t history_segments_size = segments.history_segments_size();
  const Entry *prev_entry = nullptr;
  // When there are non-zero history segments, lookup an entry
  // from the LRU dictionary, which is correspoinding to the last
  // history segment.
  if (history_segments_size == 0) {
    return nullptr;
  }

  const Segment &history_segment =
      segments.history_segment(history_segments_size - 1);

  // Simply lookup the history_segment.
  prev_entry = dic_->LookupWithoutInsert(SegmentFingerprint(history_segment));

  // Check the timestamp of prev_entry.
  const uint64_t now = Clock::GetTime();
  if (prev_entry != nullptr &&
      prev_entry->last_access_time() + k62DaysInSec < now) {
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
    for (const DicElement *elm = dic_->Head();
         trial++ < kMaxPrevValueTrial && elm != nullptr; elm = elm->next) {
      const Entry *entry = &(elm->value);
      // entry->value() equals to the prev_value or
      // entry->value() is a SUFFIX of prev_value.
      // length of entry->value() must be >= 2, as single-length
      // match would be noisy.
      if (IsValidEntry(*entry, available_emoji_carrier) &&
          entry != prev_entry && entry->next_entries_size() > 0 &&
          Util::CharsLen(entry->value()) >= 2 &&
          (entry->value() == prev_value ||
           Util::EndsWith(prev_value, entry->value()))) {
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

  // TODO(team): make GetKanaMisspelledKey(segments);
  // const string kana_input_key = GetKanaMisspelledKey(segments);

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

  const uint64_t now = Clock::GetTime();
  int trial = 0;
  for (const DicElement *elm = dic_->Head(); elm != nullptr; elm = elm->next) {
    if (!IsValidEntryIgnoringRemovedField(
            elm->value, request.request().available_emoji_carrier())) {
      continue;
    }
    if (elm->value.last_access_time() + k62DaysInSec < now) {
      updated_ = true;  // We found an entry to be deleted at next save.
      continue;
    }
    if (segments.request_type() == Segments::SUGGESTION &&
        trial++ >= kMaxSuggestionTrial) {
      VLOG(2) << "too many trials";
      break;
    }

    // Lookup key from elm_value and prev_entry.
    // If a new entry is found, the entry is pushed to the results.
    // TODO(team): make KanaFuzzyLookupEntry().
    if (!LookupEntry(request_type, input_key, base_key, expanded.get(),
                     &(elm->value), prev_entry, results) &&
        !RomanFuzzyLookupEntry(roman_input_key, &(elm->value), results)) {
      continue;
    }

    // already found enough results.
    if (results->size() >= max_results_size) {
      break;
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

  request.composer().GetStringForPreedit(input_key);
  std::set<std::string> expanded_set;
  request.composer().GetQueriesForPrediction(base, &expanded_set);
  if (!expanded_set.empty()) {
    *expanded = absl::make_unique<Trie<std::string>>();
    for (std::set<std::string>::const_iterator itr = expanded_set.begin();
         itr != expanded_set.end(); ++itr) {
      // For getting matched key, insert values
      (*expanded)->AddEntry(*itr, *itr);
    }
  }
}

bool UserHistoryPredictor::InsertCandidates(RequestType request_type,
                                            const ConversionRequest &request,
                                            size_t max_prediction_size,
                                            Segments *segments,
                                            EntryPriorityQueue *results) const {
  DCHECK(results);
  Segment *segment = segments->mutable_conversion_segment(0);
  if (segment == nullptr) {
    LOG(ERROR) << "segment is nullptr";
    return false;
  }
  const uint32_t input_key_len = Util::CharsLen(segment->key());

  size_t inserted_num = 0;
  while (inserted_num < max_prediction_size) {
    // |results| is a priority queue where the elemtnt
    // in the queue is sorted by the score defined in GetScore().
    const Entry *result_entry = results->Pop();
    if (result_entry == nullptr) {
      // Pop() returns nullptr when no more valid entry exists.
      break;
    }
    bool is_valid_candidate = false;
    if (segments->request_type() == Segments::PREDICTION) {
      is_valid_candidate = true;
    } else if (segments->request_type() == Segments::SUGGESTION) {
      // The top result of suggestion should be a VALID suggestion candidate.
      // i.e., SuggestionTrigerFunc should return true for the first
      // candidate.
      // If user types "デスノート" too many times, "デスノート" will be
      // suggested when user types "で". It is expected, but if user types
      // "です" after that,  showing "デスノート" is annoying.
      // In this situation, "です" is in the LRU, but SuggestionTrigerFunc
      // returns false for "です", since it is short.
      if (IsValidSuggestion(request_type, input_key_len, *result_entry)) {
        is_valid_candidate = true;
      } else if (segment->candidates_size() == 0) {
        VLOG(2) << "candidates size is 0";
        return false;
      }
    } else {
      LOG(ERROR) << "Unknown mode";
      return false;
    }

    if (!is_valid_candidate) {
      VLOG(2) << "not a valid candidate: " << result_entry->key();
      continue;
    }

    if (request.request().mixed_conversion() &&
        result_entry->suggestion_freq() < 2 &&
        Util::CharsLen(result_entry->value()) > 8) {
      // Don't show long history for mixed conversion
      // TODO(toshiyuki): Better to merge this into IsValidSuggestion logic.
      VLOG(2) << "long candidate: " << result_entry->value();
      continue;
    }

    Segment::Candidate *candidate = segment->push_back_candidate();
    DCHECK(candidate);
    candidate->Init();
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
#if DEBUG
    if (!absl::StrContains(candidate->description, "History")) {
      candidate->description += " History";
    }
#endif  // DEBUG
    ++inserted_num;
  }

  return inserted_num > 0;
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

bool UserHistoryPredictor::IsValidEntry(
    const Entry &entry, uint32_t available_emoji_carrier) const {
  if (entry.removed() ||
      !IsValidEntryIgnoringRemovedField(entry, available_emoji_carrier)) {
    return false;
  }
  return true;
}

bool UserHistoryPredictor::IsValidEntryIgnoringRemovedField(
    const Entry &entry, uint32_t available_emoji_carrier) const {
  if (entry.entry_type() != Entry::DEFAULT_ENTRY ||
      suppression_dictionary_->SuppressEntry(entry.key(), entry.value())) {
    return false;
  }

  if (IsEmojiEntry(entry)) {
    if (Util::IsAndroidPuaEmoji(entry.value())) {
      // Android carrier dependent emoji.
      constexpr uint32_t kAndroidCarrier =
          Request::DOCOMO_EMOJI | Request::SOFTBANK_EMOJI | Request::KDDI_EMOJI;
      if (!(available_emoji_carrier & kAndroidCarrier)) {
        return false;
      }
    } else {
      // Unicode 6.0 emoji.
      if (!(available_emoji_carrier & Request::UNICODE_EMOJI)) {
        return false;
      }
    }
  }

  // Workaround for b/116826494: Some garbled characters are suggested
  // from user history. This fiters such entries.
  if (!Util::IsValidUtf8(entry.value())) {
    LOG(ERROR) << "Invalid UTF8 found in user history: "
               << entry.Utf8DebugString();
    return false;
  }

  return true;
}

void UserHistoryPredictor::InsertEvent(EntryType type) {
  if (type == Entry::DEFAULT_ENTRY) {
    return;
  }

  const uint64_t last_access_time = Clock::GetTime();
  const uint32_t dic_key = Fingerprint("", "", type);

  CHECK(dic_.get());
  DicElement *e = dic_->Insert(dic_key);
  if (e == nullptr) {
    VLOG(2) << "insert failed";
    return;
  }

  Entry *entry = &(e->value);
  DCHECK(entry);
  entry->Clear();
  entry->set_entry_type(type);
  entry->set_last_access_time(last_access_time);
}

void UserHistoryPredictor::TryInsert(
    RequestType request_type, const std::string &key, const std::string &value,
    const std::string &description, bool is_suggestion_selected,
    uint32_t next_fp, uint64_t last_access_time, Segments *segments) {
  if (key.empty() || value.empty() || key.size() > kMaxStringLength ||
      value.size() > kMaxStringLength ||
      description.size() > kMaxStringLength) {
    return;
  }

  // For mobile, we do not learn candidates that ends with puctuation.
  if (request_type == ZERO_QUERY_SUGGESTION && Util::CharsLen(value) > 1 &&
      IsPunctuation(Util::Utf8SubString(value, Util::CharsLen(value) - 1, 1))) {
    return;
  }

  Insert(key, value, description, is_suggestion_selected, next_fp,
         last_access_time, segments);
}

void UserHistoryPredictor::Insert(const std::string &key,
                                  const std::string &value,
                                  const std::string &description,
                                  bool is_suggestion_selected, uint32_t next_fp,
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
    VLOG(2) << "insert failed";
    return;
  }

  Entry *entry = &(e->value);
  DCHECK(entry);

  entry->set_key(key);
  entry->set_value(value);
  entry->set_removed(false);

  if (description.empty()) {
    entry->clear_description();
  } else {
    entry->set_description(description);
  }

  entry->set_last_access_time(last_access_time);
  if (is_suggestion_selected) {
    entry->set_suggestion_freq(entry->suggestion_freq() + 1);
  } else {
    entry->set_conversion_freq(entry->conversion_freq() + 1);
  }

  // Inserts next_fp to the entry
  if (next_fp != 0) {
    NextEntry next_entry;
    next_entry.set_entry_fp(next_fp);
    InsertNextEntry(next_entry, entry);
  }

  VLOG(2) << key << " " << value
          << " has inserted: " << entry->Utf8DebugString();

  // New entry is inserted to the cache
  updated_ = true;
}

void UserHistoryPredictor::MaybeRecordUsageStats(
    const Segments &segments) const {
  const Segment &segment = segments.conversion_segment(0);
  if (segment.candidates_size() < 1) {
    VLOG(2) << "candidates size < 1";
    return;
  }

  const Segment::Candidate &candidate = segment.candidate(0);
  if (segment.segment_type() != Segment::FIXED_VALUE) {
    VLOG(2) << "segment is not FIXED_VALUE" << candidate.value;
    return;
  }

  if (!(candidate.source_info & Segment::Candidate::USER_HISTORY_PREDICTOR)) {
    VLOG(2) << "candidate is not from user_history_predictor";
    return;
  }

  UsageStats::IncrementCount("CommitUserHistoryPredictor");
  if (segment.key().empty()) {
    UsageStats::IncrementCount("CommitUserHistoryPredictorZeroQuery");
  }
}

void UserHistoryPredictor::Finish(const ConversionRequest &request,
                                  Segments *segments) {
  if (segments->request_type() == Segments::REVERSE_CONVERSION) {
    // Do nothing for REVERSE_CONVERSION.
    return;
  }

  if (request.config().incognito_mode()) {
    VLOG(2) << "incognito mode";
    return;
  }

  if (request.config().history_learning_level() !=
      config::Config::DEFAULT_HISTORY) {
    VLOG(2) << "history learning level is not DEFAULT_HISTORY: "
            << request.config().history_learning_level();
    return;
  }

  if (!request.config().use_history_suggest()) {
    VLOG(2) << "no history suggest";
    return;
  }

  if (!CheckSyncerAndDelete()) {
    LOG(WARNING) << "Syncer is running";
    return;
  }

  MaybeRecordUsageStats(*segments);

  const RequestType request_type = request.request().zero_query_suggestion()
                                       ? ZERO_QUERY_SUGGESTION
                                       : DEFAULT;
  const bool is_suggestion = segments->request_type() != Segments::CONVERSION;
  const uint64_t last_access_time = Clock::GetTime();

  // If user inputs a punctuation just after some long sentence,
  // we make a new candidate by concatenating the top element in LRU and
  // the punctuation user input. The top element in LRU is supposed to be
  // the long sentence user input before.
  // This is a fix for http://b/issue?id=2216838
  //
  // Note: We don't make such candidates for mobile.
  if (request_type != ZERO_QUERY_SUGGESTION && dic_->Head() != nullptr &&
      dic_->Head()->value.last_access_time() + 5 > last_access_time &&
      // Check if the current value is a punctuation.
      segments->conversion_segments_size() == 1 &&
      segments->conversion_segment(0).candidates_size() > 0 &&
      IsPunctuation(segments->conversion_segment(0).candidate(0).value) &&
      // Check if the previous value looks like a sentence.
      segments->history_segments_size() > 0 &&
      segments->history_segment(segments->history_segments_size() - 1)
              .candidates_size() > 0 &&
      IsSentenceLikeCandidate(
          segments->history_segment(segments->history_segments_size() - 1)
              .candidate(0))) {
    const Entry *entry = &(dic_->Head()->value);
    DCHECK(entry);
    const std::string &last_value =
        segments->history_segment(segments->history_segments_size() - 1)
            .candidate(0)
            .value;
    // Check if the head value in LRU ends with the candidate value in history
    // segments.
    if (Util::EndsWith(entry->value(), last_value)) {
      const Segment::Candidate &candidate =
          segments->conversion_segment(0).candidate(0);
      const std::string key = entry->key() + candidate.key;
      const std::string value = entry->value() + candidate.value;
      // Uses the same last_access_time stored in the top element
      // so that this item can be grouped together.
      TryInsert(request_type, key, value, entry->description(), is_suggestion,
                0, entry->last_access_time(), segments);
    }
  }

  const size_t history_segments_size = segments->history_segments_size();

  // Checks every segment is valid.
  for (size_t i = history_segments_size; i < segments->segments_size(); ++i) {
    const Segment &segment = segments->segment(i);
    if (segment.candidates_size() < 1) {
      VLOG(2) << "candidates size < 1";
      return;
    }
    if (segment.segment_type() != Segment::FIXED_VALUE) {
      VLOG(2) << "segment is not FIXED_VALUE";
      return;
    }
    const Segment::Candidate &candidate = segment.candidate(0);
    if (candidate.attributes & Segment::Candidate::NO_SUGGEST_LEARNING) {
      VLOG(2) << "NO_SUGGEST_LEARNING";
      return;
    }
  }

  if (IsPrivacySensitive(segments)) {
    VLOG(2) << "do not remember privacy sensitive input";
    return;
  }

  InsertHistory(request_type, is_suggestion, last_access_time, segments);
}

void UserHistoryPredictor::MakeLearningSegments(
    const Segments &segments, SegmentsForLearning *learning_segments) const {
  DCHECK(learning_segments);

  for (size_t i = 0; i < segments.history_segments_size(); ++i) {
    const Segment &segment = segments.history_segment(i);
    SegmentForLearning learning_segment;
    DCHECK_LE(1, segment.candidates_size());
    learning_segment.key = segment.candidate(0).key;
    learning_segment.value = segment.candidate(0).value;
    learning_segment.content_key = segment.candidate(0).content_key;
    learning_segment.content_value = segment.candidate(0).content_value;
    learning_segment.description = GetDescription(segment.candidate(0));
    learning_segments->push_back_history_segment(learning_segment);
  }
  for (size_t i = 0; i < segments.conversion_segments_size(); ++i) {
    const Segment &segment = segments.conversion_segment(i);
    const Segment::Candidate &candidate = segment.candidate(0);
    if (candidate.inner_segment_boundary.empty()) {
      SegmentForLearning learning_segment;
      learning_segment.key = candidate.key;
      learning_segment.value = candidate.value;
      learning_segment.content_key = candidate.content_key;
      learning_segment.content_value = candidate.content_value;
      learning_segment.description = GetDescription(candidate);
      learning_segments->push_back_conversion_segment(learning_segment);
    } else {
      SegmentForLearning learning_segment;
      for (Segment::Candidate::InnerSegmentIterator iter(&candidate);
           !iter.Done(); iter.Next()) {
        learning_segment.key.assign(iter.GetKey().data(), iter.GetKey().size());
        learning_segment.value.assign(iter.GetValue().data(),
                                      iter.GetValue().size());
        learning_segment.content_key.assign(iter.GetContentKey().data(),
                                            iter.GetContentKey().size());
        learning_segment.content_value.assign(iter.GetContentValue().data(),
                                              iter.GetContentValue().size());
        learning_segments->push_back_conversion_segment(learning_segment);
      }
    }
  }
}

void UserHistoryPredictor::InsertHistory(RequestType request_type,
                                         bool is_suggestion_selected,
                                         uint64_t last_access_time,
                                         Segments *segments) {
  SegmentsForLearning learning_segments;
  MakeLearningSegments(*segments, &learning_segments);

  std::string all_key, all_value;
  absl::flat_hash_set<uint32_t> seen;
  bool this_was_seen = false;
  const size_t history_segments_size =
      learning_segments.history_segments_size();

  for (size_t i = history_segments_size;
       i < learning_segments.all_segments_size(); ++i) {
    const SegmentForLearning &segment = learning_segments.all_segment(i);
    all_key += segment.key;
    all_value += segment.value;
    uint32_t next_fp =
        (i == learning_segments.all_segments_size() - 1)
            ? 0
            : LearningSegmentFingerprint(learning_segments.all_segment(i + 1));
    // remember the first segment
    if (i == history_segments_size) {
      seen.insert(LearningSegmentFingerprint(segment));
    }
    uint32_t next_fp_to_set = next_fp;
    // If two duplicate segments exist, kills the link
    // TO/FROM the second one to prevent loops.
    // Only killing "TO" link caused bug #2982886:
    // after converting "らいおん（もうじゅう）とぞうりむし（びせいぶつ）"
    // and typing "ぞうりむし", "ゾウリムシ（猛獣" was suggested.
    if (this_was_seen) {
      next_fp_to_set = 0;
    }
    if (!seen.insert(next_fp).second) {
      next_fp_to_set = 0;
      this_was_seen = true;
    } else {
      this_was_seen = false;
    }
    TryInsert(request_type, segment.key, segment.value, segment.description,
              is_suggestion_selected, next_fp_to_set, last_access_time,
              segments);
    if (content_word_learning_enabled_ && segment.content_key != segment.key &&
        segment.content_value != segment.value) {
      TryInsert(request_type, segment.content_key, segment.content_value,
                segment.description, is_suggestion_selected, 0,
                last_access_time, segments);
    }
  }

  // Inserts all_key/all_value.
  // We don't insert it for mobile.
  if (request_type != ZERO_QUERY_SUGGESTION &&
      learning_segments.conversion_segments_size() > 1 && !all_key.empty() &&
      !all_value.empty()) {
    TryInsert(request_type, all_key, all_value, "", is_suggestion_selected, 0,
              last_access_time, segments);
  }

  // Makes a link from the last history_segment to the first conversion segment
  // or to the entire user input.
  if (learning_segments.history_segments_size() > 0 &&
      learning_segments.conversion_segments_size() > 0) {
    const SegmentForLearning &history_segment =
        learning_segments.history_segment(segments->history_segments_size() -
                                          1);
    const SegmentForLearning &conversion_segment =
        learning_segments.conversion_segment(0);
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
      if (segments->request_type() == Segments::CONVERSION) {
        next_entry.set_entry_fp(LearningSegmentFingerprint(conversion_segment));
        InsertNextEntry(next_entry, history_entry);
      }

      // Entire user input or SUGGESTION
      if (segments->request_type() != Segments::CONVERSION ||
          learning_segments.conversion_segments_size() > 1) {
        next_entry.set_entry_fp(Fingerprint(all_key, all_value));
        InsertNextEntry(next_entry, history_entry);
      }
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
      VLOG(2) << "Erasing the key: " << StringToUint32(revert_entry.key);
      dic_->Erase(StringToUint32(revert_entry.key));
    }
  }
}

// static
UserHistoryPredictor::MatchType UserHistoryPredictor::GetMatchType(
    const std::string &lstr, const std::string &rstr) {
  if (lstr.empty() && !rstr.empty()) {
    return LEFT_EMPTY_MATCH;
  }

  const size_t size = std::min(lstr.size(), rstr.size());
  if (size == 0) {
    return NO_MATCH;
  }

  const int result = memcmp(lstr.data(), rstr.data(), size);
  if (result != 0) {
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
    const std::string &input_key, const std::string &key_base,
    const Trie<std::string> *key_expanded, const std::string &target) {
  if (key_expanded == nullptr) {
    // |input_key| and |key_base| can be different by compoesr modification.
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
    const int result = memcmp(key_base.data(), target.data(), size);
    if (result != 0) {
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
    const std::string matched = key_base + value;
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
uint32_t UserHistoryPredictor::Fingerprint(const std::string &key,
                                           const std::string &value,
                                           EntryType type) {
  if (type == Entry::DEFAULT_ENTRY) {
    // Since we have already used the fingerprint function for next entries and
    // next entries are saved in user's local machine, we are not able
    // to change the Fingerprint function for the old key/value type.
    return Hash::Fingerprint32(key + kDelimiter + value);
  } else {
    return Hash::Fingerprint32(static_cast<uint8_t>(type));
  }
}

// static
uint32_t UserHistoryPredictor::Fingerprint(const std::string &key,
                                           const std::string &value) {
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

// static
std::string UserHistoryPredictor::Uint32ToString(uint32_t fp) {
  std::string buf(reinterpret_cast<const char *>(&fp), sizeof(fp));
  return buf;
}

// static
uint32_t UserHistoryPredictor::StringToUint32(const std::string &input) {
  uint32_t result = 0;
  if (input.size() == sizeof(result)) {
    memcpy(reinterpret_cast<char *>(&result), input.data(), input.size());
  }
  return result;
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
  const uint32_t base_prefix_len = 3 - std::min(static_cast<uint32_t>(2), freq);
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
// static
uint32_t UserHistoryPredictor::max_next_entries_size() {
  return kMaxNextEntriesSize;
}

}  // namespace mozc
