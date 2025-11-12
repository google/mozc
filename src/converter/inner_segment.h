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

#ifndef MOZC_CONVERTER_INNER_SEGMENT_H_
#define MOZC_CONVERTER_INNER_SEGMENT_H_

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/container/fixed_array.h"
#include "absl/log/check.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"

namespace mozc {
namespace converter {
namespace internal {

struct LengthData {
  uint32_t key_len : 8;
  uint32_t value_len : 8;
  uint32_t content_key_len : 8;
  uint32_t content_value_len : 8;
};

static_assert(sizeof(struct internal::LengthData) == sizeof(uint32_t),
              "Size of LengthData must be equal to sizeof(uint32_t)");
static_assert(alignof(struct internal::LengthData) == alignof(uint32_t),
              "Alignment of LengthData must be equal to alignof(uint32_t)");

}  // namespace internal

// Data structure for storing segment boundary information.
// Clients should not depend on the internal data structure but
// need to use this alias as the underlying encoding schema will be updated.
using InnerSegmentBoundary = std::vector<uint32_t>;
using InnerSegmentBoundarySpan = absl::Span<const uint32_t>;

// TODO(taku): Deprecate EncodeLengths since they expose the internal
// implementations. Migrates to InnerSegmentBoundaryBuilder.
inline std::optional<uint32_t> EncodeLengths(uint32_t key_len,
                                             uint32_t value_len,
                                             uint32_t content_key_len,
                                             uint32_t content_value_len) {
  auto fix_content_len = [](uint32_t len, uint32_t full_len) {
    return len == 0 || len > full_len ? full_len : len;
  };

  // Workaround for the case when Candidate::content_(key|value) are invalid.
  content_key_len = fix_content_len(content_key_len, key_len);
  content_value_len = fix_content_len(content_value_len, value_len);

  if (key_len == 0 || value_len == 0 || content_key_len == 0 ||
      content_value_len == 0 || key_len > std::numeric_limits<uint8_t>::max() ||
      value_len > std::numeric_limits<uint8_t>::max() ||
      content_key_len > std::numeric_limits<uint8_t>::max() ||
      content_value_len > std::numeric_limits<uint8_t>::max()) {
    return std::nullopt;
  }

  const internal::LengthData data{key_len, value_len, content_key_len,
                                  content_value_len};
  return std::bit_cast<uint32_t>(data);
}

inline internal::LengthData DecodeLengths(uint32_t encoded) {
  return std::bit_cast<internal::LengthData>(encoded);
}

// Iterator class to access inner segments.
// Allows to access the inner segment via range-based-loop.
//
//  for (const auto &iter : result.inner_segments()) {
//     LOG(INFO) << iter.GetKey() << " " << iter.GetValue();
//  }
class InnerSegments {
 public:
  class IteratorData {
   public:
    IteratorData() = delete;

    absl::string_view GetKey() const { return key_.substr(0, key_len()); }
    absl::string_view GetValue() const { return value_.substr(0, value_len()); }
    absl::string_view GetContentKey() const {
      return key_.substr(0, content_key_len());
    }
    absl::string_view GetContentValue() const {
      return value_.substr(0, content_value_len());
    }
    absl::string_view GetFunctionalKey() const {
      return GetKey().substr(GetContentKey().size());
    }
    absl::string_view GetFunctionalValue() const {
      return GetValue().substr(GetContentValue().size());
    }

    friend class InnerSegments;
    friend class Iterator;

   private:
    IteratorData(absl::string_view key, absl::string_view value,
                 InnerSegmentBoundarySpan encoded_lengths)
        : key_(key), value_(value), encoded_lengths_(encoded_lengths) {}

    bool empty() const { return encoded_lengths_.empty(); }

    internal::LengthData length_data() const {
      DCHECK(!encoded_lengths_.empty());
      return DecodeLengths(encoded_lengths_.front());
    }

    // When `encoded_lengths` is not available, returns (key_|values_).size()
    // to consume all keys/values. This treatment supports empty
    // `encoded_lengths`.
    size_t key_len() const {
      return std::min(key_.size(),
                      empty() ? key_.size() : length_data().key_len);
    }

    size_t value_len() const {
      return std::min(value_.size(),
                      empty() ? value_.size() : length_data().value_len);
    }

    size_t content_key_len() const {
      return std::min(key_.size(),
                      empty() ? key_.size() : length_data().content_key_len);
    }

    size_t content_value_len() const {
      return std::min(value_.size(), empty() ? value_.size()
                                             : length_data().content_value_len);
    }

    void Next() {
      key_.remove_prefix(key_len());
      value_.remove_prefix(value_len());
      if (!encoded_lengths_.empty()) encoded_lengths_.remove_prefix(1);
    }

    absl::string_view key_;
    absl::string_view value_;
    InnerSegmentBoundarySpan encoded_lengths_;
  };

  class Iterator {
   public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = IteratorData;
    using difference_type = std::ptrdiff_t;
    using pointer = IteratorData*;
    using reference = IteratorData&;

    reference operator*() { return data_; }

    Iterator& operator++() {
      data_.Next();
      return *this;
    }

    bool operator==(const Iterator& other) const {
      return (data_.key_.size() == other.data_.key_.size() &&
              data_.value_.size() == other.data_.value_.size());
    }

    bool operator!=(const Iterator& other) const {
      return (data_.key_.size() != other.data_.key_.size() ||
              data_.value_.size() != other.data_.value_.size());
    }

    friend class IteratorData;
    friend class InnerSegments;

    Iterator() = delete;

   private:
    Iterator(absl::string_view key, absl::string_view value,
             InnerSegmentBoundarySpan encoded_lengths)
        : data_(key, value, encoded_lengths) {}
    IteratorData data_;
  };

  Iterator begin() const { return begin_; }
  Iterator end() const { return Iterator("", "", {}); }

  // size() returns the number of segments. Zero segment is only allowed when
  // both key and value are empty. Note that end() - begin() is not
  // always the same as size(), as actual size can only be computed by
  // iterating all lengths. When inner segment boundary is not defined,
  // the segment size is 1.
  size_t size() const {
    if (begin_.data_.key_.empty() && begin_.data_.value_.empty()) {
      return 0;
    }
    return std::max<size_t>(1, begin_.data_.encoded_lengths_.size());
  }

  // Returns the content key and content value after merging the segments.
  // Simply removing the functional key and value of last segment.
  // This method is used to treat the multi-segments as single-segment.
  std::pair<absl::string_view, absl::string_view> GetMergedContentKeyAndValue()
      const {
    uint32_t last_functional_key_len = 0;
    uint32_t last_functional_value_len = 0;
    for (const auto& iter : *this) {  // Remember the last functional key/value.
      last_functional_key_len = iter.GetFunctionalKey().size();
      last_functional_value_len = iter.GetFunctionalValue().size();
    }
    absl::string_view key = begin_.data_.key_;
    absl::string_view value = begin_.data_.value_;
    key.remove_suffix(last_functional_key_len);
    value.remove_suffix(last_functional_value_len);
    return std::make_pair(key, value);
  }

  // Returns the concatenated prefix key and value of segments size `size`, used
  // in history result. When size is -1, returns all key/value.
  std::pair<absl::string_view, absl::string_view> GetPrefixKeyAndValue(
      int size = -1) const {
    absl::string_view key = begin_.data_.key_;
    absl::string_view value = begin_.data_.value_;

    if (size < 0) return std::make_pair(key, value);

    int key_len = 0;
    int value_len = 0;
    for (const auto& iter : *this) {
      if (size-- == 0 || key_len >= key.size() || value_len >= value.size()) {
        return std::make_pair(key.substr(0, key_len),
                              value.substr(0, value_len));
      }
      key_len += iter.GetKey().size();
      value_len += iter.GetValue().size();
    }

    return std::make_pair(key, value);
  }

  // Returns the concatenated suffix key and value of segments size `size`, used
  // in history result. When size is -1, returns all key/value.
  std::pair<absl::string_view, absl::string_view> GetSuffixKeyAndValue(
      int size = -1) const {
    absl::string_view key = begin_.data_.key_;
    absl::string_view value = begin_.data_.value_;

    if (size == 0) {
      // Returns last empty string so to allow pointer diff.
      return std::make_pair(key.substr(key.size()), value.substr(value.size()));
    }

    int index = this->size() - size - 1;

    if (size < 0 || index < 0) {
      return std::make_pair(key, value);
    } else {
      for (const auto& iter : *this) {
        key.remove_prefix(iter.GetKey().size());
        value.remove_prefix(iter.GetValue().size());
        if (index-- == 0) return std::make_pair(key, value);
      }
    }

    return std::make_pair(key, value);
  }

  // Constructor for the structure only with key/value.
  InnerSegments(absl::string_view key, absl::string_view value,
                InnerSegmentBoundarySpan inner_segment_boundary)
      : begin_(key, value, inner_segment_boundary) {}

  // Constructor for the structure with content_key/value.
  // When inner_segment_boundary is empty, generates the placeholder
  // inner_segment_boundary from content_key/value.
  InnerSegments(absl::string_view key, absl::string_view value,
                absl::string_view content_key, absl::string_view content_value,
                InnerSegmentBoundarySpan inner_segment_boundary)
      : boundary_storage_(1),
        begin_(
            key, value,
            fix_inner_segment_boundary(key, value, content_key, content_value,
                                       inner_segment_boundary)) {}

  InnerSegments() = delete;

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const InnerSegments& inner_segments) {
    for (const auto& iter : inner_segments) {
      absl::Format(&sink, "<%d,%d,%d,%d>", iter.GetKey().size(),
                   iter.GetValue().size(), iter.GetContentKey().size(),
                   iter.GetContentValue().size());
    }
  }

 private:
  InnerSegmentBoundarySpan fix_inner_segment_boundary(
      absl::string_view key, absl::string_view value,
      absl::string_view content_key, absl::string_view content_value,
      InnerSegmentBoundarySpan inner_segment_boundary) {
    if (inner_segment_boundary.empty()) {
      if (std::optional<uint32_t> encoded =
              EncodeLengths(key.size(), value.size(), content_key.size(),
                            content_value.size());
          encoded.has_value()) {
        boundary_storage_[0] = encoded.value();
        return boundary_storage_;
      }
    }
    return inner_segment_boundary;
  }

  // boundary info to store different key and content_key with
  // empty inner_segment_boundary.
  absl::FixedArray<uint32_t, 1> boundary_storage_ = {};

  const Iterator begin_;
};

// Builder class for inner segment boundary.
//
// Example:
//   InnerSegmentBoundaryBuilder builder;
//   for (...) {
//     builder.Add(2, 3, 1, 1)  // key/value/content_key/content_value len.
//   }
//   auto boundary = builder.Build("key", "value");
//
// The last Build() method checks whether the encoded lengths are consistent
// with key/value. When inconsistent lengths are passed, return empty boundary
class InnerSegmentBoundaryBuilder {
 public:
  InnerSegmentBoundaryBuilder() = default;

  InnerSegmentBoundaryBuilder& Add(uint32_t key_len, uint32_t value_len,
                                   uint32_t content_key_len,
                                   uint32_t content_value_len) {
    if (error_) {
      return *this;
    }

    if (std::optional<uint32_t> encoded = EncodeLengths(
            key_len, value_len, content_key_len, content_value_len);
        encoded.has_value()) {
      key_consumed_ += key_len;
      value_consumed_ += value_len;
      boundary_.emplace_back(encoded.value());
    } else {
      error_ = true;
    }
    return *this;
  }

  InnerSegmentBoundaryBuilder& Add(const InnerSegments::IteratorData& data) {
    return Add(data.GetKey().size(), data.GetValue().size(),
               data.GetContentKey().size(), data.GetContentValue().size());
  }

  InnerSegmentBoundaryBuilder& Add(InnerSegments::Iterator& iter) {
    return Add(*iter);
  }

  InnerSegmentBoundaryBuilder& AddEncoded(uint32_t encoded) {
    const internal::LengthData data = DecodeLengths(encoded);
    key_consumed_ += data.key_len;
    value_consumed_ += data.value_len;
    boundary_.emplace_back(encoded);
    return *this;
  }

  InnerSegmentBoundary Build(absl::string_view key, absl::string_view value) {
    if (error_ || key_consumed_ != key.size() ||
        value_consumed_ != value.size()) {
      boundary_.clear();
    }
    return boundary_;
  }

 private:
  bool error_ = false;
  InnerSegmentBoundary boundary_;
  uint32_t key_consumed_ = 0;
  uint32_t value_consumed_ = 0;
};

// Utility function to accept fixed lengths array.
//
// auto boundary = BuildInnerSegmentBoundary(
//                  {{2, 2, 1, 1}, {3, 3, 2, 2}}, "key", "value")
//
inline InnerSegmentBoundary BuildInnerSegmentBoundary(
    absl::Span<const std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>>
        boundary,
    absl::string_view key, absl::string_view value) {
  InnerSegmentBoundaryBuilder builder;
  for (const auto& [key_len, value_len, content_key_len, content_value_len] :
       boundary) {
    builder.Add(key_len, value_len, content_key_len, content_value_len);
  }
  return builder.Build(key, value);
}

}  // namespace converter
}  // namespace mozc

#endif  // MOZC_CONVERTER_INNER_SEGMENT_H_
