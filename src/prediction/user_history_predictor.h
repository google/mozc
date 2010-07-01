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

#include <string>
#include "prediction/predictor_interface.h"
#include "storage/lru_cache.h"

namespace mozc {

class Segments;

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

 private:
  bool CheckSyncerAndDelete() const;

  bool updated_;

  struct Entry {
    uint32 length;
    uint32 suggestion_prefix_length;
    uint32 suggestion_freq;
    uint32 conversion_freq;
    uint32 last_access_time;
    string description;
    Entry()
        : length(0), suggestion_prefix_length(0),
          suggestion_freq(0), conversion_freq(0),
          last_access_time(0) {}
  };

  void Insert(const string &key, const string &value,
              const string &description,
              bool is_suggestion_selected,
              Segments *segments);

  typedef LRUCache<string, Entry> DicCache;
  typedef LRUCache<string, Entry>::Element DicElement;

  scoped_ptr<DicCache> dic_;
  mutable scoped_ptr<UserHistoryPredictorSyncer> syncer_;
};
}  // namespace mozc

#endif  // MOZC_PREDICTION_USER_HISTORY_PREDICTOR_H_
