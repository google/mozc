// Copyright 2010-2012, Google Inc.
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

#include <queue>
#include <set>
#include <string>
#include <utility>
#include "base/freelist.h"
#include "base/scoped_ptr.h"
#include "base/trie.h"
#include "prediction/predictor_interface.h"
#include "prediction/user_history_predictor.pb.h"
#include "storage/lru_cache.h"
// for FRIEND_TEST
#include "testing/base/public/gunit_prod.h"

namespace mozc {

namespace storage {
class StringStorageInterface;
}  // namespace storage

class ConversionRequest;
class DictionaryInterface;
class POSMatcher;
class Segment;
class Segments;
class SuppressionDictionary;
class UserHistoryPredictorSyncer;

#ifndef __native_client__
// Added serialization method for UserHistory.
// TODO(horo): implement UserHistoryStorage for NaCl.
class UserHistoryStorage : public mozc::user_history_predictor::UserHistory {
 public:
  explicit UserHistoryStorage(const string &filename);
  ~UserHistoryStorage();

  // Load from encrypted file.
  bool Load();

  // Save history into encrypted file.
  bool Save() const;

 private:
  scoped_ptr<storage::StringStorageInterface> storage_;
};
#endif  // __native_client__

// UserHistoryPredictor is NOT thread safe.
// Currently, all methods of UserHistoryPredictor is called
// by single thread. Although AsyncSave() and AsyncLoad() make
// worker threads internally, these two functions won't be
// called by multiple-threads at the same time
class UserHistoryPredictor : public PredictorInterface {
 public:
  UserHistoryPredictor(const DictionaryInterface *dictionary,
                       const POSMatcher *pos_matcher,
                       const SuppressionDictionary *suppression_dictionary);
  virtual ~UserHistoryPredictor();

  virtual bool Predict(Segments *segments) const;
  virtual bool PredictForRequest(const ConversionRequest &request,
                                 Segments *segments) const;

  // Hook(s) for all mutable operations
  virtual void Finish(Segments *segments);

  // Revert last Finish operation
  virtual void Revert(Segments *segments);

  // Sync user history data to local file.
  // You can call either Save() or AsyncSave().
  virtual bool Sync();

  // Reload from local disk.
  // Do not call Sync() before Reload().
  virtual bool Reload();

  // clear LRU data
  virtual bool ClearAllHistory();

  // clear unused data
  virtual bool ClearUnusedHistory();

  // Get user history filename.
  static string GetUserHistoryFileName();

  virtual const string &GetPredictorName() const { return predictor_name_; }

  // Used in user_history_sync_util.
  typedef user_history_predictor::UserHistory::Entry Entry;
  typedef user_history_predictor::UserHistory::NextEntry NextEntry;
  typedef user_history_predictor::UserHistory::Entry::EntryType EntryType;

  // return fingerprints from various object.
  static uint32 Fingerprint(const string &key, const string &value);
  static uint32 Fingerprint(const string &key, const string &value,
                            EntryType type);
  static uint32 EntryFingerprint(const Entry &entry);
  static uint32 SegmentFingerprint(const Segment &segment);

  // return the size of cache.
  static uint32 cache_size();

  // return the size of next entries.
  static uint32 max_next_entries_size();

 private:
  //  friend class UserHistoryStorage;
  friend class UserHistoryPredictorSyncer;

  FRIEND_TEST(UserHistoryPredictorTest, UserHistoryPredictorTest);
  FRIEND_TEST(UserHistoryPredictorTest, UserHistoryPredictorTest_suggestion);
  FRIEND_TEST(UserHistoryPredictorTest, DescriptionTest);
  FRIEND_TEST(UserHistoryPredictorTest,
              UserHistoryPredictorUnusedHistoryTest);
  FRIEND_TEST(UserHistoryPredictorTest, UserHistoryPredictorRevertTest);
  FRIEND_TEST(UserHistoryPredictorTest, UserHistoryPredictorClearTest);
  FRIEND_TEST(UserHistoryPredictorTest,
              UserHistoryPredictorTailingPunctuation);
  FRIEND_TEST(UserHistoryPredictorTest,
              UserHistoryPredictorPreceedingPunctuation);
  FRIEND_TEST(UserHistoryPredictorTest, StartsWithPunctuations);
  FRIEND_TEST(UserHistoryPredictorTest, ZeroQuerySuggestionTest);
  FRIEND_TEST(UserHistoryPredictorTest, MultiSegmentsMultiInput);
  FRIEND_TEST(UserHistoryPredictorTest, MultiSegmentsSingleInput);
  FRIEND_TEST(UserHistoryPredictorTest, Regression2843371_Case1);
  FRIEND_TEST(UserHistoryPredictorTest, Regression2843371_Case2);
  FRIEND_TEST(UserHistoryPredictorTest, Regression2843371_Case3);
  FRIEND_TEST(UserHistoryPredictorTest, Regression2843775);
  FRIEND_TEST(UserHistoryPredictorTest, DuplicateString);
  FRIEND_TEST(UserHistoryPredictorTest, SyncTest);
  FRIEND_TEST(UserHistoryPredictorTest, GetMatchTypeTest);
  FRIEND_TEST(UserHistoryPredictorTest, FingerPrintTest);
  FRIEND_TEST(UserHistoryPredictorTest, Uint32ToStringTest);
  FRIEND_TEST(UserHistoryPredictorTest, GetScore);
  FRIEND_TEST(UserHistoryPredictorTest, IsValidEntry);
  FRIEND_TEST(UserHistoryPredictorTest, IsValidSuggestion);
  FRIEND_TEST(UserHistoryPredictorTest, EntryPriorityQueueTest);
  FRIEND_TEST(UserHistoryPredictorTest, PrivacySensitiveTest);
  FRIEND_TEST(UserHistoryPredictorTest, PrivacySensitiveMultiSegmentsTest);
  FRIEND_TEST(UserHistoryPredictorTest, UserHistoryStorage);
  FRIEND_TEST(UserHistoryPredictorTest, RomanFuzzyPrefixMatch);
  FRIEND_TEST(UserHistoryPredictorTest, MaybeRomanMisspelledKey);
  FRIEND_TEST(UserHistoryPredictorTest, GetRomanMisspelledKey);
  FRIEND_TEST(UserHistoryPredictorTest, RomanFuzzyLookupEntry);
  FRIEND_TEST(UserHistoryPredictorTest, ExpandedLookupRoman);
  FRIEND_TEST(UserHistoryPredictorTest, ExpandedLookupKana);
  FRIEND_TEST(UserHistoryPredictorTest, GetMatchTypeFromInputRoman);
  FRIEND_TEST(UserHistoryPredictorTest, GetMatchTypeFromInputKana);
  FRIEND_TEST(UserHistoryPredictorTest, GetInputKeyFromSegmentsRoman);
  FRIEND_TEST(UserHistoryPredictorTest, GetInputKeyFromSegmentsRomanN);
  FRIEND_TEST(UserHistoryPredictorTest, GetInputKeyFromSegmentsRomanRandom);
  FRIEND_TEST(UserHistoryPredictorTest, GetInputKeyFromSegmentsShouldNotCrash);
  FRIEND_TEST(UserHistoryPredictorTest, GetInputKeyFromSegmentsFlickN);
  FRIEND_TEST(UserHistoryPredictorTest, GetInputKeyFromSegments12KeyN);
  FRIEND_TEST(UserHistoryPredictorTest, GetInputKeyFromSegmentsKana);

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

  // Load user history data to LRU from local file
  bool Load();

  // Save user history data in LRU to local file
  bool Save();

  // non-blocking version of Load
  // This makes a new thread and call Load()
  bool AsyncSave();

  // non-blocking version of Sync
  // This makes a new thread and call Save()
  bool AsyncLoad();

  // Wait until syncer finishes
  void WaitForSyncer();

  // return id for RevertEntry
  static uint16 revert_id();

  // Get match type from two strings
  static MatchType GetMatchType(const string &lstr, const string &rstr);

  // Get match type with ambiguity expansion
  static MatchType GetMatchTypeFromInput(const string &input_key,
                                         const string &key_base,
                                         const Trie<string> *key_expanded,
                                         const string &target);

  // Uint32 <=> string conversion
  static string Uint32ToString(uint32 fp);
  static uint32 StringToUint32(const string &input);

  // return true if prev_entry has a next_fp link to entry
  static bool HasBigramEntry(const Entry &entry,
                             const Entry &prev_entry);

  // return true |result_entry| can be handled as
  // a valid result if the length of user input is |prefix_len|.
  static bool IsValidSuggestion(RequestType request_type,
                                uint32 prefix_len,
                                const Entry &result_entry);

  // return true if entry is DEFAULT_ENTRY and doesn't have removed flag.
  bool IsValidEntry(const Entry &entry) const;

  // return "tweaked" score of result_entry.
  // the score is basically determined by "last_access_time", (a.k.a,
  // LRU policy), but we want to slightly change the score
  // with different signals, including the length of value and/or
  // bigram_boost flags.
  static uint32 GetScore(const Entry &result_entry);

  // Priority Queue class for entry. New item is sorted by
  // |score| internally. By calling Pop() in sequence, you
  // can obtain the list of entry sorted by |score|.
  class EntryPriorityQueue {
   public:
    EntryPriorityQueue();
    virtual ~EntryPriorityQueue();
    size_t size() const {
      return agenda_.size();
    }
    bool Push(Entry *entry);
    Entry *Pop();
    Entry *NewEntry();
   private:
    friend class UserHistoryPredictor;
    typedef pair<uint32, Entry *> QueueElement;
    typedef priority_queue<QueueElement> Agenda;
    Agenda agenda_;
    FreeList<Entry> pool_;
    set<uint32> seen_;
  };

  typedef mozc::storage::LRUCache<uint32, Entry> DicCache;
  typedef DicCache::Element DicElement;

  bool CheckSyncerAndDelete() const;

  bool Lookup(Segments *segments) const;

  // If |entry| is the target of prediction,
  // create a new result and insert it to |results|.
  // Can set |prev_entry| if there is a history segment just before |input_key|.
  // |prev_entry| is an optional field. If set NULL, this field is just ignored.
  // This method adds a new result entry with score, pair<score, entry>, to
  // |results|.
  bool LookupEntry(const string &input_key,
                   const string &key_base,
                   const Trie<string> *key_expanded,
                   const Entry *entry,
                   const Entry *prev_entry,
                   EntryPriorityQueue *results) const;

  const Entry *LookupPrevEntry(const Segments &segments) const;

  void GetResultsFromHistoryDictionary(
      const ConversionRequest &request,
      const Segments &segments,
      const Entry *prev_entry,
      EntryPriorityQueue *results) const;

  // Get input data from segments.
  // These input data include ambiguities.
  static void GetInputKeyFromSegments(
      const ConversionRequest &request, const Segments &segments,
      string *input_key, string *base,
      scoped_ptr<Trie<string> >*expanded);

  bool InsertCandidates(RequestType request_type, Segments *segments,
                        EntryPriorityQueue *results) const;

  // return true if |prefix| is a fuzzy-prefix of |str|.
  // 'Fuzzy' means that
  // 1) Allows one character deletation in the |prefix|.
  // 2) Allows one character swap in the |prefix|.
  static bool RomanFuzzyPrefixMatch(const string &str,
                                    const string &prefix);

  // return romanized preedit string if the preedit looks
  // misspelled. It first tries to get the preedit string with
  // composer() if composer is available. If not, use the key
  // directory. It also use MaybeRomanMisspelledKey() defined
  // below to check the preedit looks missspelled or not.
  static string GetRomanMisspelledKey(const Segments &segments);

  // return true if |key| may contain miss spelling.
  // Currently, this function returns true if
  // 1) key contains only one alphabet.
  // 2) other characters of key are all hiragana.
  static bool MaybeRomanMisspelledKey(const string &key);

  // If roman_input_key can be a target key of entry->key(), creat a new
  // result and insert it to |results|.
  // This method adds a new result entry with score, pair<score, entry>, to
  // |results|.
  bool RomanFuzzyLookupEntry(
      const string &roman_input_key,
      const Entry *entry,
      EntryPriorityQueue *results) const;

  // insert |key,value,description| to the internal dictionary database.
  // |is_suggestion_selected|: key/value is suggestion or conversion.
  // |next_fp|: fingerprint of the next segment.
  // |last_access_time|: the time when this entrty was created
  void Insert(const string &key,
              const string &value,
              const string &description,
              bool is_suggestion_selected,
              uint32 next_fp,
              uint32 last_access_time,
              Segments *segments);

  // Insert event entry (CLEAN_ALL_EVENT|CLEAN_UNUSED_EVENT).
  void InsertEvent(EntryType type);

  // Insert a new |next_entry| into |entry|.
  // it makes a bigram connection from entry to next_entry.
  void InsertNextEntry(const NextEntry &next_entry,
                       UserHistoryPredictor::Entry *entry) const;

  // Returns true if the input first candidate seems to be a privacy sensitive
  // such like password.
  bool IsPrivacySensitive(const Segments *segments) const;

  const DictionaryInterface *dictionary_;
  const POSMatcher *pos_matcher_;
  const SuppressionDictionary *suppression_dictionary_;
  const string predictor_name_;

  bool updated_;
  scoped_ptr<DicCache> dic_;
#ifndef __native_client__
  mutable scoped_ptr<UserHistoryPredictorSyncer> syncer_;
#endif  // __native_client__
};
}  // namespace mozc

#endif  // MOZC_PREDICTION_USER_HISTORY_PREDICTOR_H_
