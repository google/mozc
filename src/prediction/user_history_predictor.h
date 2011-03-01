// Copyright 2010, Google Inc.
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

#include <algorithm>
#include <queue>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "base/freelist.h"
#include "prediction/predictor_interface.h"
#include "prediction/user_history_predictor.pb.h"
#include "storage/lru_cache.h"
#include "testing/base/public/gunit_prod.h"  // for FRIEND_TEST

namespace mozc {

class Segments;
class Segment;

class UserHistoryPredictorSyncer;

// UserHistoryPredictor is NOT thread safe.
// Currently, all methods of UserHistoryPredictor is called
// by single thread. Although AsyncSave() and AsyncLoad() make
// worker threads internally, these two functions won't be
// called by multiple-threads at the same time
class UserHistoryPredictor: public PredictorInterface {
 public:
  UserHistoryPredictor();
  virtual ~UserHistoryPredictor();

  bool Predict(Segments *segments) const;

  // Hook(s) for all mutable operations
  void Finish(Segments *segments);

  // Revert last Finish operation
  void Revert(Segments *segments);

  // Sync user history data to local file.
  // You can call either Save() or AsyncSave().
  bool Sync();

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

  // clear LRU data
  bool ClearAllHistory();

  // clear unused data
  bool ClearUnusedHistory();

  // Wait until syncer finishes
  void WaitForSyncer();

  // return id for RevertEntry
  static uint16 revert_id();

  typedef user_history_predictor::UserHistory::Entry Entry;
  typedef user_history_predictor::UserHistory::NextEntry NextEntry;

  enum MatchType {
    NO_MATCH,            // no match
    LEFT_PREFIX_MATCH,   // left string is a prefix of right string
    RIGHT_PREFIX_MATCH,  // right string is a prefix of left string
    LEFT_EMPTY_MATCH,    // left string is empty (for zero_query_suggestion)
    EXACT_MATCH,         // right string == left string
  };

  static MatchType GetMatchType(const string &lstr, const string &rstr);

  // return fingerprints from various object.
  static uint32 Fingerprint(const string &key, const string &value);
  static uint32 EntryFingerprint(const Entry &entry);
  static uint32 SegmentFingerprint(const Segment &segment);

  // Uint32 <=> string conversion
  static string Uint32ToString(uint32 fp);
  static uint32 StringToUint32(const string &input);

  // return true |result_entry| can be handled as
  // a valid result if the length of user input is |prefix_len|.
  static bool IsValidSuggestion(bool zero_query_suggestion,
                                uint32 prefix_len,
                                const Entry &result_entry);

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
    typedef pair<uint32, Entry *> QueueElement;
    typedef priority_queue<QueueElement> Agenda;
    Agenda agenda_;
    FreeList<Entry> pool_;
    set<uint32> seen_;
  };

 private:
  typedef LRUCache<uint32, Entry>  DicCache;
  typedef LRUCache<uint32, Entry>::Element DicElement;

  bool CheckSyncerAndDelete() const;


  bool Lookup(Segments *segments) const;

  // Lookup with a key |input_key|.
  // Can set |prev_entry| if there is a history segment just before |input_key|.
  // |prev_entry| is an optional field. If set NULL, this field is just ignored.
  // This method adds a new result entry with score, pair<score, entry>, to
  // |results|.
  bool LookupEntry(const string &input_key,
                   const Entry *entry,
                   const Entry *prev_entry,
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

  // Insert a new |next_entry| into |entry|.
  // it makes a bigram connection from entry to next_entry.
  void InsertNextEntry(const NextEntry &next_entry,
                       UserHistoryPredictor::Entry *entry) const;

  bool updated_;
  scoped_ptr<DicCache> dic_;
  mutable scoped_ptr<UserHistoryPredictorSyncer> syncer_;
};
}  // namespace mozc

#endif  // MOZC_PREDICTION_USER_HISTORY_PREDICTOR_H_
