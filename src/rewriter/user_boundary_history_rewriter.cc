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

#include <algorithm>
#include <deque>
#include <string>
#include <utility>
#include <vector>
#include "base/config_file_stream.h"
#include "base/util.h"
#include "converter/segments.h"
#include "converter/converter_interface.h"
#include "storage/lru_storage.h"
#include "rewriter/rewriter_interface.h"
#include "rewriter/user_boundary_history_rewriter.h"
#include "session/config_handler.h"
#include "session/config.pb.h"
#include "usage_stats/usage_stats.h"

namespace {
static const int kValueSize  = 4;
static const uint32 kLRUSize = 5000;
static const uint32 kSeedValue = 0x761fea81;

enum { INSERT, RESIZE };

class LengthArray {
 public:
  void ToUCharArray(uint8 *array) const {
    array[0] = length0_;
    array[1] = length1_;
    array[2] = length2_;
    array[3] = length3_;
    array[4] = length4_;
    array[5] = length5_;
    array[6] = length6_;
    array[7] = length7_;
  }

  void CopyFromUCharArray(const uint8 *array) {
    length0_ = array[0];
    length1_ = array[1];
    length2_ = array[2];
    length3_ = array[3];
    length4_ = array[4];
    length5_ = array[5];
    length6_ = array[6];
    length7_ = array[7];
  }

  bool Equal(const LengthArray &r) const {
    return (length0_ == r.length0_ &&
            length1_ == r.length1_ &&
            length2_ == r.length2_ &&
            length3_ == r.length3_ &&
            length4_ == r.length4_ &&
            length5_ == r.length5_ &&
            length6_ == r.length6_ &&
            length7_ == r.length7_);
  }

 private:
  uint8 length0_ : 4;
  uint8 length1_ : 4;
  uint8 length2_ : 4;
  uint8 length3_ : 4;
  uint8 length4_ : 4;
  uint8 length5_ : 4;
  uint8 length6_ : 4;
  uint8 length7_ : 4;
};
}

namespace mozc {

UserBoundaryHistoryRewriter::UserBoundaryHistoryRewriter()
    : storage_(new LRUStorage) {
  static const char kFileName[] = "user://boundary.db";
  const string filename = ConfigFileStream::GetFileName(kFileName);
  if (!storage_->OpenOrCreate(filename.c_str(),
                              kValueSize, kLRUSize, kSeedValue)) {
    LOG(WARNING) << "cannot initialize UserBoundaryHistoryRewriter";
    storage_.reset(NULL);
  }
}

UserBoundaryHistoryRewriter::~UserBoundaryHistoryRewriter() {}

void UserBoundaryHistoryRewriter::Finish(Segments *segments) {
  if (GET_CONFIG(incognito_mode)) {
    VLOG(2) << "incognito mode";
    return;
  }

  if (GET_CONFIG(history_learning_level) !=
      config::Config::DEFAULT_HISTORY) {
    VLOG(2) << "history_learning_level is not DEFAULT_HISTORY";
    return;
  }

  if (!segments->use_user_history()) {
    VLOG(2) << "no use_user_history";
    return;
  }

  if (storage_.get() == NULL) {
    VLOG(2) << "storage is NULL";
    return;
  }

  if (segments->has_resized()) {
    ResizeOrInsert(segments, INSERT);
    // update usage stats here
    usage_stats::UsageStats::SetInteger(
        "UserBoundaryHistoryEntrySize",
        static_cast<int>(storage_->used_size()));
  }
}

bool UserBoundaryHistoryRewriter::Rewrite(Segments *segments) const {
  if (GET_CONFIG(incognito_mode)) {
    VLOG(2) << "incognito mode";
    return false;
  }

  if (GET_CONFIG(history_learning_level) == config::Config::NO_HISTORY) {
    VLOG(2) << "history_learning_level is NO_HISTORY";
    return false;
  }

  if (!segments->use_user_history()) {
    VLOG(2) << "no use_user_history";
    return false;
  }

  if (storage_.get() == NULL) {
    VLOG(2) << "storage is NULL";
    return false;
  }

  if (!segments->has_resized()) {
    return ResizeOrInsert(segments, RESIZE);
  }

  return false;
}

//TODO(taku): split Reize/Insert into different functions
bool UserBoundaryHistoryRewriter::ResizeOrInsert(Segments *segments,
                                                 int type) const {
  bool result = false;
  uint8 length_array[8];

  const size_t history_segments_size = segments->history_segments_size();

  // resize segments in [history_segments_size .. target_segments_size - 1]
  size_t target_segments_size = segments->segments_size();

  // when INSERTING new history,
  // Get the prefix of segments having FIXED_VALUE state.
  if (type == INSERT) {
    target_segments_size = history_segments_size;
    for (size_t i = history_segments_size; i < segments->segments_size(); ++i) {
      const Segment &segment = segments->segment(i);
      if (segment.segment_type() == Segment::FIXED_VALUE) {
        ++target_segments_size;
      }
    }
  }

  // No effective segments found
  if (target_segments_size <= history_segments_size) {
    return false;
  }

  ConverterInterface *converter = ConverterFactory::GetConverter();

  deque<pair<string, size_t> > keys(target_segments_size -
                                    history_segments_size);
  for (size_t i = history_segments_size; i < target_segments_size; ++i) {
    const Segment &segment = segments->segment(i);
    keys[i - history_segments_size].first = segment.key();
    const size_t length = Util::CharsLen(segment.key());
    if (length > 255) {   // too long segment
      VLOG(2) << "too long segment";
      return false;
    }
    keys[i - history_segments_size].second = length;
  }

  for (size_t i = history_segments_size; i < target_segments_size; ++i) {
    static const size_t kMaxKeysSize = 5;
    const size_t keys_size = min(kMaxKeysSize, keys.size());
    string key;
    memset(length_array, 0, sizeof(length_array));
    for (size_t k = 0; k < keys_size; ++k) {
      key += keys[k].first;
      length_array[k] = static_cast<uint8>(keys[k].second);
    }
    for (int j = static_cast<int>(keys_size) - 1; j >= 0; --j) {
      if (type == RESIZE) {
        const LengthArray *value =
            reinterpret_cast<const LengthArray *>(storage_->Lookup(key));
        if (value != NULL) {
          LengthArray orig_value;
          orig_value.CopyFromUCharArray(length_array);
          if (!value->Equal(orig_value)) {
            value->ToUCharArray(length_array);
            const int old_segments_size = static_cast<int>(target_segments_size);
            VLOG(2) << "ResizeSegment key: " << key << " "
                    << i - history_segments_size << " " << j + 1
                    << " " << (int)length_array[0] << " " << (int)length_array[1]
                    << " " << (int)length_array[2] << " " << (int)length_array[3]
                    << " " << (int)length_array[4] << " " << (int)length_array[5]
                    << " " << (int)length_array[6] << " " << (int)length_array[7];
            converter->ResizeSegment(segments,
                                     i - history_segments_size,
                                     j + 1,
                                     length_array, 8);
            i += (j + target_segments_size - old_segments_size);
            result = true;
            break;
          }
        }
      } else if (type == INSERT) {
        VLOG(2) << "InserteSegment key: " << key << " "
                << i - history_segments_size << " " << j + 1
                << " " << (int)length_array[0] << " " << (int)length_array[1]
                << " " << (int)length_array[2] << " " << (int)length_array[3]
                << " " << (int)length_array[4] << " " << (int)length_array[5]
                << " " << (int)length_array[6] << " " << (int)length_array[7];
        LengthArray inserted_value;
        inserted_value.CopyFromUCharArray(length_array);
        storage_->Insert(key, reinterpret_cast<const char *>(&inserted_value));
      }

      length_array[j] = 0;
      key = key.substr(0, key.size() - keys[j].first.size());
    }

    keys.pop_front();  // delete first item
  }

  return result;
}

void UserBoundaryHistoryRewriter::Clear() {
  if (storage_.get() != NULL) {
    VLOG(1) << "Clearing user segment data";
    storage_->Clear();
  }
}
}  // namespace mozc
