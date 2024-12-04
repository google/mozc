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

#include "rewriter/user_boundary_history_rewriter.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <utility>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "base/config_file_stream.h"
#include "base/file_util.h"
#include "base/util.h"
#include "base/vlog.h"
#include "converter/converter_interface.h"
#include "converter/segments.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "storage/lru_storage.h"
#include "usage_stats/usage_stats.h"

namespace mozc {
namespace {

constexpr int kValueSize = 4;
constexpr uint32_t kLruSize = 5000;
constexpr uint32_t kSeedValue = 0x761fea81;

constexpr char kFileName[] = "user://boundary.db";

class LengthArray {
 public:
  explicit LengthArray(const std::array<uint8_t, 8> &array) {
    length0_ = array[0];
    length1_ = array[1];
    length2_ = array[2];
    length3_ = array[3];
    length4_ = array[4];
    length5_ = array[5];
    length6_ = array[6];
    length7_ = array[7];
  }

  std::array<uint8_t, 8> ToUint8Array() const {
    return {length0_, length1_, length2_, length3_,
            length4_, length5_, length6_, length7_};
  }

  bool Equal(const std::array<uint8_t, 8> &other) const {
    return length0_ == other[0] && length1_ == other[1] &&
           length2_ == other[2] && length3_ == other[3] &&
           length4_ == other[4] && length5_ == other[5] &&
           length6_ == other[6] && length7_ == other[7];
  }

 private:
  // Variables are initialized and converted by reinterpret_casts.
  // Do not remove / add variables until the reinterpret_casts are removed.
  uint8_t length0_ : 4;
  uint8_t length1_ : 4;
  uint8_t length2_ : 4;
  uint8_t length3_ : 4;
  uint8_t length4_ : 4;
  uint8_t length5_ : 4;
  uint8_t length6_ : 4;
  uint8_t length7_ : 4;
};

}  // namespace

UserBoundaryHistoryRewriter::UserBoundaryHistoryRewriter(
    const ConverterInterface *parent_converter)
    : parent_converter_(parent_converter) {
  DCHECK(parent_converter_);
  Reload();
}

void UserBoundaryHistoryRewriter::Finish(const ConversionRequest &request,
                                         Segments *segments) {
  if (request.request_type() != ConversionRequest::CONVERSION) {
    return;
  }

  if (request.config().incognito_mode()) {
    MOZC_VLOG(2) << "incognito mode";
    return;
  }

  if (request.config().history_learning_level() !=
      config::Config::DEFAULT_HISTORY) {
    MOZC_VLOG(2) << "history_learning_level is not DEFAULT_HISTORY";
    return;
  }

  if (!request.enable_user_history_for_conversion()) {
    MOZC_VLOG(2) << "user history for conversion is disabled";
    return;
  }

  if (segments->resized()) {
    Insert(request, *segments);
#ifdef __ANDROID__
    // TODO(hidehiko): UsageStats requires some functionalities, e.g. network,
    // which are not needed for mozc's main features.
    // So, to focus on the main features' developing, we just skip it for now.
    // Note: we can #ifdef inside SetInteger, but to build it we need to build
    // other methods in usage_stats as well. So we'll exclude the method here
    // for now.
#else   // __ANDROID__
    // update usage stats here
    usage_stats::UsageStats::SetInteger(
        "UserBoundaryHistoryEntrySize",
        static_cast<int>(storage_.used_size()));
#endif  // __ANDROID__
  }
}

bool UserBoundaryHistoryRewriter::Rewrite(const ConversionRequest &request,
                                          Segments *segments) const {
  if (request.config().incognito_mode()) {
    MOZC_VLOG(2) << "incognito mode";
    return false;
  }

  if (request.config().history_learning_level() == config::Config::NO_HISTORY) {
    MOZC_VLOG(2) << "history_learning_level is NO_HISTORY";
    return false;
  }

  if (!request.enable_user_history_for_conversion()) {
    MOZC_VLOG(2) << "user history for conversion is disabled";
    return false;
  }

  if (request.skip_slow_rewriters()) {
    return false;
  }

  if (!segments->resized()) {
    return Resize(request, *segments);
  }

  return false;
}

bool UserBoundaryHistoryRewriter::Sync() {
  storage_.DeleteElementsUntouchedFor62Days();
  return true;
}

bool UserBoundaryHistoryRewriter::Reload() {
  const std::string filename = ConfigFileStream::GetFileName(kFileName);
  if (!storage_.OpenOrCreate(filename.c_str(), kValueSize, kLruSize,
                             kSeedValue)) {
    LOG(WARNING) << "cannot initialize UserBoundaryHistoryRewriter";
    storage_.Clear();
    return false;
  }

  constexpr absl::string_view kFileSuffix = ".merge_pending";
  const std::string merge_pending_file = absl::StrCat(filename, kFileSuffix);

  // merge pending file does not always exist.
  if (absl::Status s = FileUtil::FileExists(merge_pending_file); s.ok()) {
    storage_.Merge(merge_pending_file.c_str());
    FileUtil::UnlinkOrLogError(merge_pending_file);
  } else if (!absl::IsNotFound(s)) {
    LOG(ERROR) << "Cannot check if " << merge_pending_file << " exists: " << s;
  }

  return true;
}

bool UserBoundaryHistoryRewriter::Resize(
    const ConversionRequest &request, Segments &segments) const {
  const size_t target_segments_size = segments.conversion_segments_size();

  // No effective segments found
  if (target_segments_size == 0) {
    return false;
  }

  std::deque<std::pair<std::string, size_t>> keys(target_segments_size);
  for (size_t i = 0; i < target_segments_size; ++i) {
    const Segment &segment = segments.conversion_segment(i);
    keys[i].first = segment.key();
    const size_t length = Util::CharsLen(segment.key());
    if (length > 255) {  // too long segment
      MOZC_VLOG(2) << "too long segment";
      return false;
    }
    keys[i].second = length;
  }

  bool result = false;
  for (size_t seg_idx = 0; seg_idx < target_segments_size; ++seg_idx) {
    constexpr size_t kMaxKeysSize = 5;
    const size_t keys_size = std::min(kMaxKeysSize, keys.size());
    std::string key;
    std::array<uint8_t, 8> length_array = {0, 0, 0, 0, 0, 0, 0, 0};
    for (size_t k = 0; k < keys_size; ++k) {
      key += keys[k].first;
      length_array[k] = static_cast<uint8_t>(keys[k].second);
    }
    for (int j = static_cast<int>(keys_size) - 1; j >= 0; --j) {
      const LengthArray *value =
          reinterpret_cast<const LengthArray *>(storage_.Lookup(key));
      if (value != nullptr) {
        if (!value->Equal(length_array)) {
          length_array = value->ToUint8Array();
          MOZC_VLOG(2) << "ResizeSegment key: " << key << " segments: ["
                       << seg_idx << ", " << j + 1 << "] "
                       << "resize: [" << absl::StrJoin(length_array, " ")
                       << "]";
          if (parent_converter_->ResizeSegment(&segments, request,
                                               seg_idx, j + 1,
                                               length_array)) {
            result = true;
          } else {
            LOG(WARNING) << "ResizeSegment failed for key: " << key;
          }
          seg_idx += j;
          break;
        }
      }

      length_array[j] = 0;
      key.erase(key.size() - keys[j].first.size());
    }

    keys.pop_front();  // delete first item
  }

  return result;
}

bool UserBoundaryHistoryRewriter::Insert(const ConversionRequest &request,
                                         Segments &segments) {
  // Get the prefix of segments having FIXED_VALUE state.
  size_t target_segments_size = 0;
  for (const Segment &segment : segments.conversion_segments()) {
    if (segment.segment_type() == Segment::FIXED_VALUE) {
      ++target_segments_size;
    }
  }

  // No effective segments found
  if (target_segments_size == 0) {
    return false;
  }

  std::deque<std::pair<std::string, size_t>> keys(target_segments_size);
  for (size_t i = 0; i < target_segments_size; ++i) {
    const Segment &segment = segments.conversion_segment(i);
    keys[i].first = segment.key();
    const size_t length = Util::CharsLen(segment.key());
    if (length > 255) {  // too long segment
      MOZC_VLOG(2) << "too long segment";
      return false;
    }
    keys[i].second = length;
  }

  bool result = false;
  for (size_t seg_idx = 0; seg_idx < target_segments_size; ++seg_idx) {
    constexpr size_t kMaxKeysSize = 5;
    const size_t keys_size = std::min(kMaxKeysSize, keys.size());
    std::string key;
    std::array<uint8_t, 8> length_array = {0, 0, 0, 0, 0, 0, 0, 0};
    for (size_t k = 0; k < keys_size; ++k) {
      key += keys[k].first;
      length_array[k] = static_cast<uint8_t>(keys[k].second);
    }
    for (int j = static_cast<int>(keys_size) - 1; j >= 0; --j) {
      MOZC_VLOG(2) << "InserteSegment key: " << key << " "
                   << seg_idx << " " << j + 1 << " "
                   << absl::StrJoin(length_array, " ");
      const LengthArray inserted_value(length_array);
      storage_.Insert(key, reinterpret_cast<const char *>(&inserted_value));

      length_array[j] = 0;
      key.erase(key.size() - keys[j].first.size());
    }

    keys.pop_front();  // delete first item
  }

  return result;
}

void UserBoundaryHistoryRewriter::Clear() {
  MOZC_VLOG(1) << "Clearing user segment data";
  storage_.Clear();
}

}  // namespace mozc
