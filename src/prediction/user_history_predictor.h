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
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "base/container/freelist.h"
#include "base/container/trie.h"
#include "base/thread.h"
#include "composer/query.h"
#include "converter/segments.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suppression_dictionary.h"
#include "engine/modules.h"
#include "prediction/predictor_interface.h"
#include "prediction/user_history_predictor.pb.h"
#include "request/conversion_request.h"
#include "storage/encrypted_string_storage.h"
#include "storage/lru_cache.h"
#include "testing/friend_test.h"  // IWYU pragma: keep

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

  // Deletes entries before the given timestamp.  Returns the number of deleted
  // entries.
  int DeleteEntriesBefore(uint64_t timestamp);

  // Deletes entries that are not accessed for 62 days.  Returns the number of
  // deleted entries.
  int DeleteEntriesUntouchedFor62Days();

  mozc::user_history_predictor::UserHistory &GetProto() { return proto_; }
  const mozc::user_history_predictor::UserHistory &GetProto() const {
    return proto_;
  }

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
  UserHistoryPredictor(const engine::Modules &modules,
                       bool enable_content_word_learning);
  ~UserHistoryPredictor() override;

  void set_content_word_learning_enabled(bool value) {
    content_word_learning_enabled_ = value;
  }

  bool Predict(Segments *segments) const;
  bool PredictForRequest(const ConversionRequest &request,
                         Segments *segments) const override;

  // Hook(s) for all mutable operations.
  void Finish(const ConversionRequest &request, Segments *segments) override;

  // Revert last Finish operation.
  void Revert(Segments *segments) override;

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

  const std::string &GetPredictorName() const override {
    return predictor_name_;
  }

  // From user_history_predictor.proto
  using Entry = user_history_predictor::UserHistory::Entry;
  using NextEntry = user_history_predictor::UserHistory::NextEntry;
  using EntryType = user_history_predictor::UserHistory::Entry::EntryType;

  // Returns fingerprints from various object.
  static uint32_t Fingerprint(absl::string_view key, absl::string_view value);
  static uint32_t Fingerprint(absl::string_view key, absl::string_view value,
                              EntryType type);
  static uint32_t EntryFingerprint(const Entry &entry);
  static uint32_t SegmentFingerprint(const Segment &segment);

  // Returns the size of cache.
  static uint32_t cache_size();

  // Returns the size of next entries.
  uint32_t max_next_entries_size() const;

 private:
  struct SegmentForLearning {
    std::string key;
    std::string value;
    std::string content_key;
    std::string content_value;
    std::string description;
  };

  // Fingerprint of key/value.
  static uint32_t LearningSegmentFingerprint(const SegmentForLearning &segment);

  // Fingerprints of key/value and content_key/content_value.
  std::vector<uint32_t> LearningSegmentFingerprints(
      const SegmentForLearning &segment) const;

  struct SegmentsForLearning {
    std::string conversion_segments_key;
    std::string conversion_segments_value;

    std::vector<SegmentForLearning> history_segments;
    std::vector<SegmentForLearning> conversion_segments;
  };

  friend class UserHistoryPredictorTest;

  FRIEND_TEST(UserHistoryPredictorTest, UserHistoryPredictorTestSuggestion);
  FRIEND_TEST(UserHistoryPredictorTest, GetMatchTypeTest);
  FRIEND_TEST(UserHistoryPredictorTest, Uint32ToStringTest);
  FRIEND_TEST(UserHistoryPredictorTest, GetScore);
  FRIEND_TEST(UserHistoryPredictorTest, IsValidEntry);
  FRIEND_TEST(UserHistoryPredictorTest, IsValidSuggestion);
  FRIEND_TEST(UserHistoryPredictorTest, IsValidSuggestionForMixedConversion);
  FRIEND_TEST(UserHistoryPredictorTest, EntryPriorityQueueTest);
  FRIEND_TEST(UserHistoryPredictorTest, RomanFuzzyPrefixMatch);
  FRIEND_TEST(UserHistoryPredictorTest, MaybeRomanMisspelledKey);
  FRIEND_TEST(UserHistoryPredictorTest, GetRomanMisspelledKey);
  FRIEND_TEST(UserHistoryPredictorTest, RomanFuzzyLookupEntry);
  FRIEND_TEST(UserHistoryPredictorTest, ExpandedLookupRoman);
  FRIEND_TEST(UserHistoryPredictorTest, ExpandedLookupKana);
  FRIEND_TEST(UserHistoryPredictorTest, GetMatchTypeFromInputRoman);
  FRIEND_TEST(UserHistoryPredictorTest, GetMatchTypeFromInputKana);
  FRIEND_TEST(UserHistoryPredictorTest, GetInputKeyFromSegmentsRoman);
  FRIEND_TEST(UserHistoryPredictorTest, GetInputKeyFromSegmentsRomanRandom);
  FRIEND_TEST(UserHistoryPredictorTest, GetInputKeyFromSegmentsShouldNotCrash);
  FRIEND_TEST(UserHistoryPredictorTest, GetInputKeyFromSegmentsRomanN);
  FRIEND_TEST(UserHistoryPredictorTest, GetInputKeyFromSegmentsFlickN);
  FRIEND_TEST(UserHistoryPredictorTest, GetInputKeyFromSegments12KeyN);
  FRIEND_TEST(UserHistoryPredictorTest, GetInputKeyFromSegmentsKana);
  FRIEND_TEST(UserHistoryPredictorTest, EraseNextEntries);
  FRIEND_TEST(UserHistoryPredictorTest, RemoveNgramChain);
  FRIEND_TEST(UserHistoryPredictorTest, ClearHistoryEntryUnigram);
  FRIEND_TEST(UserHistoryPredictorTest, ClearHistoryEntryBigramDeleteWhole);
  FRIEND_TEST(UserHistoryPredictorTest, ClearHistoryEntryBigramDeleteFirst);
  FRIEND_TEST(UserHistoryPredictorTest, ClearHistoryEntryBigramDeleteSecond);
  FRIEND_TEST(UserHistoryPredictorTest, ClearHistoryEntryTrigramDeleteWhole);
  FRIEND_TEST(UserHistoryPredictorTest, ClearHistoryEntryTrigramDeleteFirst);
  FRIEND_TEST(UserHistoryPredictorTest, ClearHistoryEntryTrigramDeleteSecond);
  FRIEND_TEST(UserHistoryPredictorTest, ClearHistoryEntryTrigramDeleteThird);
  FRIEND_TEST(UserHistoryPredictorTest,
              ClearHistoryEntryTrigramDeleteFirstBigram);
  FRIEND_TEST(UserHistoryPredictorTest,
              ClearHistoryEntryTrigramDeleteSecondBigram);
  FRIEND_TEST(UserHistoryPredictorTest, 62DayOldEntriesAreDeletedAtSync);

  enum MatchType {
    NO_MATCH,            // no match
    LEFT_PREFIX_MATCH,   // left string is a prefix of right string
    RIGHT_PREFIX_MATCH,  // right string is a prefix of left string
    LEFT_EMPTY_MATCH,    // left string is empty (for zero_query_suggestion)
    EXACT_MATCH,         // right string == left string
  };

  enum RequestType {
    DEFAULT,
    ZERO_QUERY_SUGGESTION,
  };

  // Returns value of RemoveNgramChain() method. See the comments in
  // implementation.
  enum RemoveNgramChainResult {
    DONE,
    TAIL,
    NOT_FOUND,
  };

  // Result type for IsValidCandidate() check.
  enum ResultType {
    GOOD_RESULT,
    BAD_RESULT,
    STOP_ENUMERATION,  // Do not insert and stop enumerations
  };

  // Returns true if this predictor should return results for the input.
  bool ShouldPredict(RequestType request_type, const ConversionRequest &request,
                     const Segments &segments) const;

  // Loads user history data to an on-memory LRU from the local file.
  bool Load();
  // Loads user history data to an on-memory LRU.
  bool Load(const UserHistoryStorage &history);

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

  // Returns id for RevertEntry
  static uint16_t revert_id();

  // Gets match type from two strings
  static MatchType GetMatchType(absl::string_view lstr, absl::string_view rstr);

  // Gets match type with ambiguity expansion
  static MatchType GetMatchTypeFromInput(absl::string_view input_key,
                                         absl::string_view key_base,
                                         const Trie<std::string> *key_expanded,
                                         absl::string_view target);

  // Uint32 <=> string conversion
  static std::string Uint32ToString(uint32_t fp);

  // Returns true if prev_entry has a next_fp link to entry
  static bool HasBigramEntry(const Entry &entry, const Entry &prev_entry);

  // Returns true |result_entry| can be handled as
  // a valid result if the length of user input is |prefix_len|.
  static bool IsValidSuggestion(RequestType request_type, uint32_t prefix_len,
                                const Entry &entry);

  // IsValidSuggestion used in mixed conversion (mobile).
  static bool IsValidSuggestionForMixedConversion(
      const ConversionRequest &request, uint32_t prefix_len,
      const Entry &entry);

  static ResultType GetResultType(const ConversionRequest &request,
                                  RequestType request_type,
                                  bool is_top_candidate, uint32_t input_key_len,
                                  const Entry &entry);

  // Returns true if entry is DEFAULT_ENTRY, satisfies certain conditions, and
  // doesn't have removed flag.
  bool IsValidEntry(const Entry &entry) const;
  // The same as IsValidEntry except that removed field is ignored.
  bool IsValidEntryIgnoringRemovedField(const Entry &entry) const;

  // Returns "tweaked" score of result_entry.
  // the score is basically determined by "last_access_time", (a.k.a,
  // LRU policy), but we want to slightly change the score
  // with different signals, including the length of value and/or
  // bigram_boost flags.
  static uint32_t GetScore(const Entry &result_entry);

  // Priority Queue class for entry. New item is sorted by
  // |score| internally. By calling Pop() in sequence, you
  // can obtain the list of entry sorted by |score|.
  class EntryPriorityQueue final {
   public:
    EntryPriorityQueue() : pool_(kEntryPoolSize) {}
    size_t size() const { return agenda_.size(); }
    bool Push(Entry *entry);
    Entry *Pop();
    Entry *NewEntry();

   private:
    using QueueElement = std::pair<uint32_t, Entry *>;
    using Agenda = std::priority_queue<QueueElement>;

    friend class UserHistoryPredictor;

    // Default object pool size for EntryPriorityQueue
    static constexpr size_t kEntryPoolSize = 16;
    Agenda agenda_;
    FreeList<Entry> pool_;
    absl::flat_hash_set<size_t> seen_;
  };

  using DicCache = mozc::storage::LruCache<uint32_t, Entry>;
  using DicElement = DicCache::Element;

  bool CheckSyncerAndDelete() const;

  // If |entry| is the target of prediction,
  // create a new result and insert it to |results|.
  // Can set |prev_entry| if there is a history segment just before |input_key|.
  // |prev_entry| is an optional field. If set nullptr, this field is just
  // ignored. This method adds a new result entry with score,
  // pair<score, entry>, to |results|.
  bool LookupEntry(RequestType request_type, absl::string_view input_key,
                   absl::string_view key_base,
                   const Trie<std::string> *key_expanded, const Entry *entry,
                   const Entry *prev_entry, EntryPriorityQueue *results) const;

  // For the EXACT and RIGHT_PREFIX match, we will generate joined
  // candidates by looking up the history link.
  // Gets key value pair and assigns them to |result_key| and |result_value|
  // for prediction result. The last entry which was joined
  // will be assigned to |result_last_entry|.
  // Returns false if we don't have the result for this match.
  // |left_last_access_time| and |left_most_last_access_time| will be updated
  // according to the entry lookup.
  bool GetKeyValueForExactAndRightPrefixMatch(
      absl::string_view input_key, const Entry *entry,
      const Entry **result_last_entry, uint64_t *left_last_access_time,
      uint64_t *left_most_last_access_time, std::string *result_key,
      std::string *result_value) const;

  const Entry *LookupPrevEntry(const Segments &segments) const;

  // Adds an entry to a priority queue.
  Entry *AddEntry(const Entry &entry, EntryPriorityQueue *results) const;

  // Adds the entry whose key and value are modified to a priority queue.
  Entry *AddEntryWithNewKeyValue(std::string key, std::string value,
                                 Entry entry,
                                 EntryPriorityQueue *results) const;

  void GetResultsFromHistoryDictionary(RequestType request_type,
                                       const ConversionRequest &request,
                                       const Segments &segments,
                                       const Entry *prev_entry,
                                       size_t max_results_size,
                                       EntryPriorityQueue *results) const;

  // Gets input data from segments.
  // These input data include ambiguities.
  static void GetInputKeyFromSegments(
      const ConversionRequest &request, const Segments &segments,
      std::string *input_key, std::string *base,
      std::unique_ptr<Trie<std::string>> *expanded);

  bool InsertCandidates(RequestType request_type,
                        const ConversionRequest &request,
                        size_t max_prediction_size,
                        size_t max_prediction_char_coverage, Segments *segments,
                        EntryPriorityQueue *results) const;

  SegmentsForLearning MakeLearningSegments(const Segments &segments) const;

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
  static std::string GetRomanMisspelledKey(const ConversionRequest &request,
                                           const Segments &segments);

  // Returns the typing corrected queries.
  std::vector<::mozc::composer::TypeCorrectedQuery> GetTypingCorrectedQueries(
      const ConversionRequest &request, const Segments &segments) const;

  // Returns true if |key| may contain miss spelling.
  // Currently, this function returns true if
  // 1) key contains only one alphabet.
  // 2) other characters of key are all hiragana.
  static bool MaybeRomanMisspelledKey(absl::string_view key);

  // If roman_input_key can be a target key of entry->key(), creat a new
  // result and insert it to |results|.
  // This method adds a new result entry with score, pair<score, entry>, to
  // |results|.
  bool RomanFuzzyLookupEntry(absl::string_view roman_input_key,
                             const Entry *entry,
                             EntryPriorityQueue *results) const;

  // if `prev_entry` is the prefix of `entry`, add the suffix part as
  // zero-query suggestion.
  bool ZeroQueryLookupEntry(RequestType request_type,
                            absl::string_view input_key, const Entry *entry,
                            const Entry *prev_entry,
                            EntryPriorityQueue *results) const;

  void InsertHistory(RequestType request_type, bool is_suggestion_selected,
                     uint64_t last_access_time, Segments *segments);

  void InsertHistoryForConversionSegments(
      RequestType request_type, bool is_suggestion_selected,
      uint64_t last_access_time, const SegmentsForLearning &learning_segments,
      Segments *segments);

  // Inserts |key,value,description| to the internal dictionary database.
  // |is_suggestion_selected|: key/value is suggestion or conversion.
  // |next_fp|: fingerprints of the next segment.
  // |last_access_time|: the time when this entry was created
  void Insert(std::string key, std::string value, std::string description,
              bool is_suggestion_selected, absl::Span<const uint32_t> next_fps,
              uint64_t last_access_time, Segments *segments);

  // Called by TryInsert to check the Entry to insert.
  bool ShouldInsert(RequestType request_type, absl::string_view key,
                    absl::string_view value,
                    absl::string_view description) const;

  // Tries to insert entry.
  // Entry's contents and request_type will be checked before insertion.
  void TryInsert(RequestType request_type, absl::string_view key,
                 absl::string_view value, absl::string_view description,
                 bool is_suggestion_selected,
                 absl::Span<const uint32_t> next_fps, uint64_t last_access_time,
                 Segments *segments);

  // Inserts event entry (CLEAN_ALL_EVENT|CLEAN_UNUSED_EVENT).
  void InsertEvent(EntryType type);

  // Inserts a new |next_entry| into |entry|.
  // it makes a bigram connection from entry to next_entry.
  void InsertNextEntry(const NextEntry &next_entry, Entry *entry) const;

  static void EraseNextEntries(uint32_t fp, Entry *entry);

  // Recursively removes a chain of Entries in |dic_|. See the comment in
  // implementation for details.
  RemoveNgramChainResult RemoveNgramChain(
      absl::string_view target_key, absl::string_view target_value,
      Entry *entry, std::vector<absl::string_view> *key_ngrams,
      size_t key_ngrams_len, std::vector<absl::string_view> *value_ngrams,
      size_t value_ngrams_len);

  // Returns true if the input first candidate seems to be a privacy sensitive
  // such like password.
  bool IsPrivacySensitive(const Segments *segments) const;

  // Removes history entries when the selected ratio is under the threshold.
  // Selected ratio:
  //  (# of candidate committed) / (# of candidate shown on commit event)
  void MaybeRemoveUnselectedHistory(const Segments &segments);

  const dictionary::DictionaryInterface *dictionary_;
  const dictionary::PosMatcher *pos_matcher_;
  const dictionary::SuppressionDictionary *suppression_dictionary_;
  const std::string predictor_name_;

  bool content_word_learning_enabled_;
  mutable std::atomic<bool> updated_;
  std::unique_ptr<DicCache> dic_;
  mutable std::optional<BackgroundFuture<void>> sync_;
  const engine::Modules &modules_;

  mutable std::atomic<bool> aggressive_bigram_enabled_ = false;
};

}  // namespace mozc::prediction

#endif  // MOZC_PREDICTION_USER_HISTORY_PREDICTOR_H_
