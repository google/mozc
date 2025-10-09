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

#ifndef MOZC_PREDICTION_USER_HISTORY_PREDICTOR_H_
#define MOZC_PREDICTION_USER_HISTORY_PREDICTOR_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "base/container/freelist.h"
#include "base/container/trie.h"
#include "base/thread.h"
#include "composer/query.h"
#include "converter/inner_segment.h"
#include "dictionary/dictionary_interface.h"
#include "engine/modules.h"
#include "prediction/predictor_interface.h"
#include "prediction/result.h"
#include "prediction/user_history_predictor.pb.h"
#include "request/conversion_request.h"
#include "storage/encrypted_string_storage.h"
#include "storage/lru_cache.h"

namespace mozc::prediction {

// Added serialization method for UserHistory.
class UserHistoryStorage {
 public:
  explicit UserHistoryStorage(const absl::string_view filename)
      : storage_(filename) {}

  // Loads from encrypted file.
  bool Load();

  // Saves history into encrypted file.
  bool Save();

  mozc::user_history_predictor::UserHistory& GetProto() { return proto_; }
  const mozc::user_history_predictor::UserHistory& GetProto() const {
    return proto_;
  }

  // Migrate old 32bit Fingerprint to 64bit Fingerprint.
  static void MigrateNextEntries(
      mozc::user_history_predictor::UserHistory* absl_nonnull proto);

 private:
  storage::EncryptedStringStorage storage_;
  mozc::user_history_predictor::UserHistory proto_;
};

// UserHistoryPredictor is NOT thread safe.
// Currently, all methods of UserHistoryPredictor is called
// by single thread. Although AsyncSave() and AsyncLoad() make
// worker threads internally, these two functions won't be
// called by multiple-threads at the same time
class UserHistoryPredictor : public PredictorInterface {
 public:
  explicit UserHistoryPredictor(const engine::Modules& modules);
  ~UserHistoryPredictor() override;

  std::vector<Result> Predict(const ConversionRequest& request) const override;

  // Hook(s) for all mutable operations.
  void Finish(const ConversionRequest& request,
              absl::Span<const Result> results, uint32_t revert_id) override;

  // Revert last Finish operation.
  void Revert(uint32_t revert_id) override;

  // Sync user history data to local file.
  // You can call either Save() or AsyncSave().
  bool Sync() override;

  // Reloads from local disk.
  // Do not call Sync() before Reload().
  bool Reload() override;

  // Clears LRU data.
  bool ClearAllHistory() override;

  // Clears unused data.
  bool ClearUnusedHistory() override;

  // Clears a specific history entry.
  bool ClearHistoryEntry(absl::string_view key,
                         absl::string_view value) override;

  // Implements PredictorInterface.
  bool Wait() override;

  // Gets user history filename.
  static std::string GetUserHistoryFileName();

  absl::string_view GetPredictorName() const override {
    return "UserHistoryPredictor";
  }

  // From user_history_predictor.proto
  using Entry = user_history_predictor::UserHistory::Entry;

  // Returns fingerprints from various object.
  static uint64_t Fingerprint(absl::string_view key, absl::string_view value);
  static uint64_t EntryFingerprint(const Entry& entry);
  static uint64_t ResultFingerprint(const Result& result);

  // Old 32-bit fingerprints functions.
  static uint32_t FingerprintDepereated(absl::string_view key,
                                        absl::string_view value);

 private:
  struct SegmentForLearning {
    // The string byte offset of key and value on result.(key|value).
    int32_t key_begin = 0;
    int32_t value_begin = 0;
    absl::string_view key;
    absl::string_view value;
    absl::string_view content_key;
    absl::string_view content_value;
    absl::string_view description;
  };

  // Fingerprint of key/value.
  static uint64_t LearningSegmentFingerprint(
      const SegmentForLearning& learning_segment);

  // Fingerprints of key/value and content_key/content_value.
  std::vector<uint64_t> LearningSegmentFingerprints(
      const SegmentForLearning& learning_segment) const;

  struct SegmentsForLearning {
    std::string conversion_segments_key;
    std::string conversion_segments_value;
    converter::InnerSegmentBoundary inner_segment_boundary;

    std::vector<SegmentForLearning> history_segments;
    std::vector<SegmentForLearning> conversion_segments;
  };

  friend class UserHistoryPredictorTestPeer;
  friend class UserHistoryPredictorTest;

  enum class MatchType {
    NO_MATCH,            // no match
    LEFT_PREFIX_MATCH,   // left string is a prefix of right string
    RIGHT_PREFIX_MATCH,  // right string is a prefix of left string
    LEFT_EMPTY_MATCH,    // left string is empty (for zero_query_suggestion)
    EXACT_MATCH,         // right string == left string
  };

  // Returns true if this predictor should return results for the input.
  bool ShouldPredict(const ConversionRequest& request) const;

  // Loads user history data to an on-memory LRU from the local file.
  bool Load();

  // Loads user history data to an on-memory LRU.
  bool Load(UserHistoryStorage&& history);

  // Saves user history data in LRU to local file
  bool Save();

  // non-blocking version of Load
  // This makes a new thread and call Load()
  bool AsyncSave();

  // non-blocking version of Sync
  // This makes a new thread and call Save()
  bool AsyncLoad();

  // Waits until syncer finishes.
  void WaitForSyncer();

  // Gets match type from two strings
  static MatchType GetMatchType(absl::string_view lstr, absl::string_view rstr);

  // Gets match type with ambiguity expansion
  static MatchType GetMatchTypeFromInput(absl::string_view request_key,
                                         absl::string_view key_base,
                                         const Trie<std::string>* key_expanded,
                                         absl::string_view target);

  // Returns true if prev_entry has a next_fp link to entry
  static bool HasBigramEntry(const Entry& entry, const Entry& prev_entry);

  // Returns true if `entry` is valid.
  static bool IsValidResult(const ConversionRequest& request,
                            uint32_t request_key_len, const Entry& entry);

  // Returns true if entry is DEFAULT_ENTRY, satisfies certain conditions, and
  // doesn't have removed flag.
  bool IsValidEntry(const Entry& entry) const;
  // The same as IsValidEntry except that removed field is ignored.
  bool IsValidEntryIgnoringRemovedField(const Entry& entry) const;

  // Returns "tweaked" score of result_entry.
  // the score is basically determined by "last_access_time", (a.k.a,
  // LRU policy), but we want to slightly change the score
  // with different signals, including the length of value and/or
  // bigram_boost flags.
  static uint32_t GetScore(const Entry& result_entry);

  // Priority Queue class for entry. New item is sorted by
  // |score| internally. By calling Pop() in sequence, you
  // can obtain the list of entry sorted by |score|.
  class EntryPriorityQueue final {
   public:
    EntryPriorityQueue() : pool_(kEntryPoolSize) {}
    size_t size() const { return agenda_.size(); }
    bool Push(Entry* entry);
    Entry* absl_nullable Pop();
    Entry* NewEntry();

   private:
    using QueueElement = std::pair<uint64_t, Entry*>;
    using Agenda = std::priority_queue<QueueElement>;

    friend class UserHistoryPredictor;

    // Default object pool size for EntryPriorityQueue
    static constexpr size_t kEntryPoolSize = 16;
    Agenda agenda_;
    FreeList<Entry> pool_;
    absl::flat_hash_set<size_t> seen_;
  };

  using DicCache = mozc::storage::LruCache<uint64_t, Entry>;
  using DicElement = DicCache::Element;

  bool CheckSyncerAndDelete() const;

  // If |entry| is the target of prediction,
  // create a new result and insert it to |entry_queue|.
  // Can set |prev_entry| if there is a history segment just before
  // |request_key|. |prev_entry| is an optional field. If set nullptr, this
  // field is just ignored. This method adds a new result entry with score,
  // pair<score, entry>, to |entry_queue|.
  bool LookupEntry(const ConversionRequest& request,
                   absl::string_view request_key, absl::string_view key_base,
                   const Trie<std::string>* key_expanded, const Entry* entry,
                   const Entry* prev_entry,
                   EntryPriorityQueue* entry_queue) const;

  // For the EXACT and RIGHT_PREFIX match, we will generate joined
  // candidates by looking up the history link.
  // Gets key value pair and assigns them to |result_key| and |result_value|
  // for prediction result. The last entry which was joined
  // will be assigned to |result_last_entry|.
  // Returns false if we don't have the result for this match.
  // |left_last_access_time| and |left_most_last_access_time| will be updated
  // according to the entry lookup.
  // If exact match results exist, return them first when |prefer_exact_match|
  // is true.
  bool GetKeyValueForExactAndRightPrefixMatch(
      absl::string_view request_key, bool prefer_exact_match,
      const Entry* entry, const Entry** result_last_entry,
      uint64_t* left_last_access_time, uint64_t* left_most_last_access_time,
      std::string* result_key, std::string* result_value,
      converter::InnerSegmentBoundary* result_inner_segment_boundary) const;

  const Entry* absl_nullable LookupPrevEntry(
      const ConversionRequest& request) const;

  // Adds an entry to a priority queue.
  Entry* absl_nonnull AddEntry(const Entry& entry,
                               EntryPriorityQueue* entry_queue) const;

  // Adds the entry whose key and value are modified to a priority queue.
  Entry* absl_nonnull AddEntryWithNewKeyValue(
      std::string key, std::string value,
      converter::InnerSegmentBoundarySpan inner_segment_boundary, Entry entry,
      EntryPriorityQueue* entry_queue) const;

  EntryPriorityQueue GetEntry_QueueFromHistoryDictionary(
      const ConversionRequest& request, const Entry* prev_entry,
      size_t max_entry_queue_size) const;

  // Gets input data from segments.
  // These input data include ambiguities.
  static void GetInputKeyFromRequest(
      const ConversionRequest& request, std::string* request_key,
      std::string* base, std::unique_ptr<Trie<std::string>>* expanded);

  std::vector<Result> MakeResults(const ConversionRequest& request,
                                  size_t max_prediction_size,
                                  size_t max_prediction_char_coverage,
                                  EntryPriorityQueue* entry_queue) const;

  SegmentsForLearning MakeLearningSegments(
      const ConversionRequest& request, absl::Span<const Result> results) const;

  // Returns true if |prefix| is a fuzzy-prefix of |str|.
  // 'Fuzzy' means that
  // 1) Allows one character deletion in the |prefix|.
  // 2) Allows one character swap in the |prefix|.
  static bool RomanFuzzyPrefixMatch(absl::string_view str,
                                    absl::string_view prefix);

  // Returns romanized preedit string if the preedit looks
  // misspelled. It first tries to get the preedit string with
  // composer() if composer is available. If not, use the key
  // directory. It also use MaybeRomanMisspelledKey() defined
  // below to check the preedit looks misspelled or not.
  static std::string GetRomanMisspelledKey(const ConversionRequest& request);

  // Returns the typing corrected queries.
  std::vector<::mozc::composer::TypeCorrectedQuery> GetTypingCorrectedQueries(
      const ConversionRequest& request) const;

  // Returns true if |key| may contain miss spelling.
  // Currently, this function returns true if
  // 1) key contains only one alphabet.
  // 2) other characters of key are all hiragana.
  static bool MaybeRomanMisspelledKey(absl::string_view key);

  // If roman_request_key can be a target key of entry->key(), creat a new
  // result and insert it to |entry_queue|.
  // This method adds a new result entry with score, pair<score, entry>, to
  // |entry_queue|.
  bool RomanFuzzyLookupEntry(absl::string_view roman_request_key,
                             const Entry* entry,
                             EntryPriorityQueue* entry_queue) const;

  // if `prev_entry` is the prefix of `entry`, add the suffix part as
  // zero-query suggestion.
  bool ZeroQueryLookupEntry(const ConversionRequest& request,
                            absl::string_view request_key, const Entry* entry,
                            const Entry* prev_entry,
                            EntryPriorityQueue* entry_queue) const;

  struct RevertEntries {
    // The result committed.
    Result result;

    // History entry (previous context).
    // The chain link from history entry to the first entries are reverted.
    std::optional<Entry> history_entry;

    // Actual primitive entries associated with the last commit operation.
    // vector of [key_begin, value_begin, revert_entry].
    // `revert_entry` is stored in the LRU cache.
    // entry.key/value() starts from the result.key/value[key/value_begin].
    std::vector<std::tuple<int32_t, int32_t, Entry>> entries;
  };

  void InsertHistory(const ConversionRequest& request,
                     const SegmentsForLearning& learning_segments,
                     RevertEntries* revert_entries);

  void InsertHistoryForHistorySegments(
      const ConversionRequest& request, bool is_suggestion_selected,
      uint64_t last_access_time, const SegmentsForLearning& learning_segments,
      RevertEntries* revert_entries);

  void InsertHistoryForConversionSegments(
      const ConversionRequest& request, bool is_suggestion_selected,
      uint64_t last_access_time, const SegmentsForLearning& learning_segments,
      RevertEntries* revert_entries);

  // Inserts |key,value,description| to the internal dictionary database.
  // |inner_segment_boundary| inner segment boundary information.
  // |key_begin,value_begin|: string byte offset on result.(key|value).
  // |is_suggestion_selected|: key/value is suggestion or conversion.
  // |next_fp|: fingerprints of the next segment.
  // |last_access_time|: the time when this entry was created.
  // Entry's contents and request_type will be checked before insertion.
  void Insert(const ConversionRequest& request, int32_t key_begin,
              int32_t value_begin, absl::string_view key,
              absl::string_view value, absl::string_view description,
              converter::InnerSegmentBoundarySpan inner_segment_boundary,
              bool is_suggestion_selected, absl::Span<const uint64_t> next_fps,
              uint64_t last_access_time, RevertEntries* revert_entries);

  // Inserts a new |fp| into |entry|.
  // it makes a bigram connection from entry to next_entry.
  void InsertNextEntry(uint64_t fp, Entry* entry) const;

  static void EraseNextEntries(uint64_t fp, Entry* entry);

  // Recursively removes a chain of Entries in |dic_|.
  // Returns true if at least one chain is removed.
  bool RemoveNgramChain(absl::string_view target_key,
                        absl::string_view target_value, Entry* entry);

  // Removes entry when key/value are the prefix of entry with
  // the inner boundary constraint. entry->removed() gets true when removed.
  static bool RemoveEntryWithInnerSegment(absl::string_view key,
                                          absl::string_view value,
                                          Entry* entry);

  // Returns true if the input first candidate seems to be a privacy sensitive
  // such like password.
  bool IsPrivacySensitive(absl::Span<const Result> results) const;

  // Removes history entries when the selected ratio is under the threshold.
  // Selected ratio:
  //  (# of candidate committed) / (# of candidate shown on commit event)
  void MaybeRemoveUnselectedHistory(absl::Span<const Result> results,
                                    RevertEntries* revert_entries);

  // Returns the value string byte offset on `reverted_value` with the context
  // information populated from the client. When all characters in
  // `reverted_value` are removed, returns 0.
  // Example:
  //  - reverted_value: 東京駅
  //  - left_context:   ここは東京
  //   -> returns 6.  (only '駅' is removed, and '東京' is kept.).
  // User may type "ここは" -> "東京駅" -> backspace -> ...
  //
  // This function returns the position in best-effort-basis as context
  // information is not always accurate. Returns 0 when failing to detect
  // the position, which leads to revert the entire value, same as the
  // previous behavior.
  static int GuessRevertedValueOffset(absl::string_view reverted_value,
                                      absl::string_view left_context);

  // Re-commits the reverted entries using the left context information.
  // Users may use the backspace key just to remove the last few characters.
  void MaybeProcessPartialRevertEntry(const ConversionRequest& request) const;

  const dictionary::DictionaryInterface& dictionary_;
  const dictionary::UserDictionaryInterface& user_dictionary_;

  mutable std::atomic<bool> updated_;

  // The const methods of `dic_` are not strictly thread-safe. This is
  // because these const methods e.g., `dic_->(Mutable)LookupWithoutInsert()`,
  // return an Entry pointer held by `dic_`, meaning the returned content
  // could be updated by another thread while the current thread is accessing
  // it.
  //
  // When accessing dic_'s Entry pointer in the const method of this
  // class .e.g. Predict(), need to protect `dic_` with a ReaderLock. In
  // addition, WriteLock is required when modifying dic_'s Entry pointer.
  // This rule is not applied when using dic_ in non-const method now, e.g.
  // Finish() and Revert(), but wants to apply them for the sake of consistency.
  //
  // TODO(taku): Add absl's thread annotations to all variables.
  mutable absl::Mutex dic_mutex_;
  mutable std::unique_ptr<DicCache> dic_;

  mutable std::optional<BackgroundFuture<void>> sync_;
  const engine::Modules& modules_;

  // Internal LRU cache to store dic_key/Entry to be reverted.
  storage::LruCache<uint64_t, RevertEntries> revert_cache_;

  // `last_committed_entries_` stores the entries to be re-committed
  // after Revert().  Note that `last_committed_entries_` is not associated with
  // revert_id and shared across different context (text view).
  mutable std::unique_ptr<const RevertEntries> last_committed_entries_;
};

}  // namespace mozc::prediction

#endif  // MOZC_PREDICTION_USER_HISTORY_PREDICTOR_H_
