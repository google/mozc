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
#include <optional>
#include <string>
#include <utility>
#include <vector>

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
#include "converter/segments.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"
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

  bool Equal(const LengthArray &other) const {
    return length0_ == other.length0_ && length1_ == other.length1_ &&
           length2_ == other.length2_ && length3_ == other.length3_ &&
           length4_ == other.length4_ && length5_ == other.length5_ &&
           length6_ == other.length6_ && length7_ == other.length7_;
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

class SegmentsKey {
 public:
  SegmentsKey() = delete;
  SegmentsKey(std::string &&whole_key, std::vector<int> &&byte_indexs,
              std::vector<uint8_t> &&char_sizes)
      : whole_key_(std::move(whole_key)),
        byte_indexs_(std::move(byte_indexs)),
        char_sizes_(std::move(char_sizes)) {}

  static std::optional<const SegmentsKey> Create(const Segments &segments) {
    std::string whole_key;
    std::vector<int> byte_indexs;  // indexes in bytes (3 for "あ")
    std::vector<uint8_t> char_sizes;  // sizes in chars (1 for "あ")

    size_t byte_index = 0;
    for (const Segment &segment : segments.conversion_segments()) {
      absl::string_view key = segment.key();
      absl::StrAppend(&whole_key, key);
      byte_indexs.push_back(byte_index);
      byte_index += key.size();

      const size_t key_size = segment.key_len();
      if (key_size > 255) {  // too long segment
        return std::nullopt;
      }
      char_sizes.push_back(static_cast<uint8_t>(key_size));
    }
    byte_indexs.push_back(byte_index);
    return SegmentsKey(std::move(whole_key), std::move(byte_indexs),
                       std::move(char_sizes));
  }

  // Returns the key of the segments at segment_index with segment_size.
  absl::string_view GetKey(size_t segment_index, size_t segment_size) const {
    DCHECK_LT(segment_index + segment_size, byte_indexs_.size());
    const size_t start_index = byte_indexs_[segment_index];
    const size_t end_index = byte_indexs_[segment_index + segment_size];
    const size_t key_size = end_index - start_index;
    return absl::string_view(whole_key_.data() + start_index, key_size);
  }

  // Returns the length array of the segments at segment_index with
  // segment_size.
  LengthArray GetLengthArray(size_t segment_index, size_t segment_size) const {
    const size_t size = std::min<size_t>(segment_size, 8);
    std::array<uint8_t, 8> length_array = {0, 0, 0, 0, 0, 0, 0, 0};
    for (size_t i = 0; i < size; ++i) {
      length_array[i] = char_sizes_[segment_index + i];
    }
    return LengthArray(length_array);
  }

 private:
  // If segments are {"これは", "Mozcの", "こーどです"},
  // whole_key_: "これはMozcのこーどです" (31 bytes in total)
  // byte_indexs_: {0, 9, 16, 31}
  // char_sizes_: {3, 5, 5}
  const std::string whole_key_;
  const std::vector<int> byte_indexs_;  // indexes in bytes (3 for "あ")
  const std::vector<uint8_t> char_sizes_;  // sizes in chars (1 for "あ")
};

}  // namespace

UserBoundaryHistoryRewriter::UserBoundaryHistoryRewriter() {
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

std::optional<RewriterInterface::ResizeSegmentsRequest>
UserBoundaryHistoryRewriter::CheckResizeSegmentsRequest(
    const ConversionRequest &request, const Segments &segments) const {
  if (segments.resized()) {
    return std::nullopt;
  }

  if (request.config().incognito_mode()) {
    MOZC_VLOG(2) << "incognito mode";
    return std::nullopt;
  }

  if (request.config().history_learning_level() == config::Config::NO_HISTORY) {
    MOZC_VLOG(2) << "history_learning_level is NO_HISTORY";
    return std::nullopt;
  }

  if (!request.enable_user_history_for_conversion()) {
    MOZC_VLOG(2) << "user history for conversion is disabled";
    return std::nullopt;
  }

  if (request.skip_slow_rewriters()) {
    return std::nullopt;
  }

  const size_t target_segments_size = segments.conversion_segments_size();

  // No effective segments found
  if (target_segments_size == 0) {
    return std::nullopt;
  }

  std::optional<const SegmentsKey> segments_key = SegmentsKey::Create(segments);
  if (!segments_key) {
    MOZC_VLOG(2) << "too long segment";
    return std::nullopt;
  }

  for (size_t seg_idx = 0; seg_idx < target_segments_size; ++seg_idx) {
    constexpr int kMaxKeysSize = 5;
    const int keys_size =
        std::clamp<int>(target_segments_size - seg_idx, 0, kMaxKeysSize);
    for (size_t seg_size = keys_size; seg_size != 0; --seg_size) {
      absl::string_view key = segments_key->GetKey(seg_idx, seg_size);
      const LengthArray *value =
          reinterpret_cast<const LengthArray *>(storage_.Lookup(key));
      if (value == nullptr) {
        // If the key is not in the history, resize is not needed.
        // Continue to the next step with a smaller segment key.
        continue;
      }

      const LengthArray length_array =
          segments_key->GetLengthArray(seg_idx, seg_size);
      if (value->Equal(length_array)) {
        // If the segments are already same as the history, resize is not
        // needed. Skip the checked segments.
        seg_idx += seg_size - 1;  // -1 as the main loop will add +1.
        break;
      }

      const std::array<uint8_t, 8> updated_array = value->ToUint8Array();
      MOZC_VLOG(2) << "ResizeSegment key: " << key << " segments: [" << seg_idx
                   << ", " << seg_size << "] "
                   << "resize: [" << absl::StrJoin(updated_array, " ") << "]";

      const ResizeSegmentsRequest resize_request = {
          .segment_index = seg_idx,
          .segment_sizes = std::move(updated_array),
      };
      return resize_request;
    }
  }

  return std::nullopt;
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

  std::optional<const SegmentsKey> segments_key = SegmentsKey::Create(segments);
  if (!segments_key) {
    MOZC_VLOG(2) << "too long segment";
    return false;
  }

  for (size_t seg_idx = 0; seg_idx < target_segments_size; ++seg_idx) {
    constexpr size_t kMaxKeysSize = 5;
    const int keys_size =
        std::clamp<int>(target_segments_size - seg_idx, 0, kMaxKeysSize);
    for (size_t seg_size = keys_size; seg_size != 0; --seg_size) {
      const absl::string_view key = segments_key->GetKey(seg_idx, seg_size);
      const LengthArray length_array =
          segments_key->GetLengthArray(seg_idx, seg_size);
      MOZC_VLOG(2) << "InserteSegment key: " << key << " " << seg_idx << " "
                   << seg_size << " "
                   << absl::StrJoin(length_array.ToUint8Array(), " ");
      storage_.Insert(key, reinterpret_cast<const char *>(&length_array));
    }
  }

  return true;
}

void UserBoundaryHistoryRewriter::Clear() {
  MOZC_VLOG(1) << "Clearing user segment data";
  storage_.Clear();
}

}  // namespace mozc
