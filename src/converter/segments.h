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

#ifndef MOZC_CONVERTER_SEGMENTS_H_
#define MOZC_CONVERTER_SEGMENTS_H_

#include <cstddef>
#include <cstdint>
#include <deque>
#include <iterator>
#include <memory>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/container/freelist.h"
#include "base/strings/assign.h"
#include "base/util.h"
#include "converter/candidate.h"
#include "converter/lattice.h"
#include "testing/friend_test.h"

namespace mozc {

class Segment final {
 public:
  enum SegmentType {
    FREE,            // FULL automatic conversion.
    FIXED_BOUNDARY,  // cannot consist of multiple segments.
    FIXED_VALUE,     // cannot consist of multiple segments.
                     // and result is also fixed
    SUBMITTED,       // submitted node
    HISTORY          // history node. It is hidden from user.
  };

  // This is an alias for backward compatibility.
  // Using ::mozc::converter::Candidate is preferred.
  using Candidate = ::mozc::converter::Candidate;

  Segment() : segment_type_(FREE), pool_(kCandidatesPoolSize) {}

  Segment(const Segment &x);
  Segment &operator=(const Segment &x);

  SegmentType segment_type() const { return segment_type_; }
  void set_segment_type(const SegmentType segment_type) {
    segment_type_ = segment_type;
  }

  absl::string_view key() const { return key_; }

  // Returns the length of the key in Unicode characters. (e.g. 1 for "あ")
  size_t key_len() const { return key_len_; }

  template <typename T>
  void set_key(T &&key) {
    strings::Assign(key_, std::forward<T>(key));
    key_len_ = Util::CharsLen(key_);
  }

  // check if the specified index is valid or not.
  bool is_valid_index(int i) const;

  // Candidate manipulations
  // getter
  const Candidate &candidate(int i) const;

  // setter
  Candidate *mutable_candidate(int i);

  // push and insert candidates
  Candidate *push_front_candidate();
  Candidate *push_back_candidate();
  // alias of push_back_candidate()
  Candidate *add_candidate() { return push_back_candidate(); }
  Candidate *insert_candidate(int i);
  void insert_candidate(int i, std::unique_ptr<Candidate> candidate);
  void insert_candidates(int i,
                         std::vector<std::unique_ptr<Candidate>> candidates);

  // get size of candidates
  size_t candidates_size() const { return candidates_.size(); }

  // erase candidate
  void pop_front_candidate();
  void pop_back_candidate();
  void erase_candidate(int i);
  void erase_candidates(int i, size_t size);

  // erase all candidates
  // do not erase meta candidates
  void clear_candidates();

  // meta candidates
  // TODO(toshiyuki): Integrate meta candidates to candidate and delete these
  size_t meta_candidates_size() const { return meta_candidates_.size(); }
  void clear_meta_candidates() { meta_candidates_.clear(); }
  absl::Span<const Candidate> meta_candidates() const {
    return meta_candidates_;
  }
  std::vector<Candidate> *mutable_meta_candidates() {
    return &meta_candidates_;
  }
  const Candidate &meta_candidate(size_t i) const;
  Candidate *mutable_meta_candidate(size_t i);
  Candidate *add_meta_candidate();

  // move old_idx-th-candidate to new_index
  void move_candidate(int old_idx, int new_idx);

  void Clear();

  // Keep clear() method as other modules are still using the old method
  void clear() { Clear(); }

  std::string DebugString() const;

  friend std::ostream &operator<<(std::ostream &os, const Segment &segment) {
    return os << segment.DebugString();
  }

  const std::deque<Candidate *> &candidates() const { return candidates_; }

  // For debug. Candidate words removed through conversion process.
  std::vector<Candidate> removed_candidates_for_debug_;

 private:
  void DeepCopyCandidates(const std::deque<Candidate *> &candidates);

  static constexpr int kCandidatesPoolSize = 16;

  // LINT.IfChange
  SegmentType segment_type_;
  // Note that |key_| is shorter than usual when partial suggestion is
  // performed.
  // For example if the preedit text is "しれ|ません", there is only a segment
  // whose |key_| is "しれ".
  // There is no way to detect by using only a segment whether this segment is
  // for partial suggestion or not.
  // You should detect that by using both Composer and Segments.
  std::string key_;
  size_t key_len_ = 0;
  std::deque<Candidate *> candidates_;
  std::vector<Candidate> meta_candidates_;
  std::vector<std::unique_ptr<Candidate>> pool_;
  // LINT.ThenChange(//converter/segments_matchers.h)
};

// Segments is basically an array of Segment.
// Note that there are two types of Segment
// a) History Segment (SegmentType == HISTORY OR SUBMITTED)
//    Segments user entered just before the transacton
// b) Conversion Segment
//    Current segments user inputs
//
// Array of segment is represented as an array as follows
// segments_array[] = {HS_0,HS_1,...HS_N, CS0, CS1, CS2...}
//
// * segment(i) and mutable_segment(int i)
//  access segment regardless of History/Conversion distinctions
//
// * history_segment(i) and mutable_history_segment(i)
//  access only History Segment
//
// conversion_segment(i) and mutable_conversion_segment(i)
//  access only Conversion Segment
//  segment(i + history_segments_size()) == conversion_segment(i)
class Segments final {
 public:
  // This class wraps an iterator as is, except that `operator*` dereferences
  // twice. For example, if `InnerIterator` is the iterator of
  // `std::deque<Segment *>`, `operator*` dereferences to `Segment&`.
  using inner_iterator = std::deque<Segment *>::iterator;
  using inner_const_iterator = std::deque<Segment *>::const_iterator;
  template <typename InnerIterator, bool is_const = false>
  class Iterator {
   public:
    using inner_value_type =
        typename std::iterator_traits<InnerIterator>::value_type;

    using iterator_category =
        typename std::iterator_traits<InnerIterator>::iterator_category;
    using value_type = std::conditional_t<
        is_const,
        typename std::add_const_t<std::remove_pointer_t<inner_value_type>>,
        typename std::remove_pointer_t<inner_value_type>>;
    using difference_type =
        typename std::iterator_traits<InnerIterator>::difference_type;
    using pointer = value_type *;
    using reference = value_type &;

    explicit Iterator(const InnerIterator &iterator) : iterator_(iterator) {}

    // Make `iterator` type convertible to `const_iterator`.
    template <bool enable = is_const>
    Iterator(typename std::enable_if_t<enable, const Iterator<inner_iterator>>
                 iterator)
        : iterator_(iterator.iterator_) {}

    reference operator*() const { return **iterator_; }
    pointer operator->() const { return *iterator_; }

    Iterator &operator++() {
      ++iterator_;
      return *this;
    }
    Iterator &operator--() {
      --iterator_;
      return *this;
    }
    Iterator operator+(difference_type diff) const {
      return Iterator{iterator_ + diff};
    }
    Iterator operator-(difference_type diff) const {
      return Iterator{iterator_ - diff};
    }
    Iterator &operator+=(difference_type diff) {
      iterator_ += diff;
      return *this;
    }

    difference_type operator-(const Iterator &other) const {
      return iterator_ - other.iterator_;
    }

    bool operator==(const Iterator &other) const {
      return iterator_ == other.iterator_;
    }
    bool operator!=(const Iterator &other) const {
      return iterator_ != other.iterator_;
    }

   private:
    friend class Iterator<inner_const_iterator, /*is_const=*/true>;
    friend class Segments;

    InnerIterator iterator_;
  };
  using iterator = Iterator<inner_iterator>;
  using const_iterator = Iterator<inner_const_iterator, /*is_const=*/true>;
  static_assert(!std::is_const_v<iterator::value_type>);
  static_assert(std::is_const_v<const_iterator::value_type>);

  // This class represents `begin` and `end`, like a `std::span` for
  // non-contiguous iterators.
  template <typename Iterator>
  class Range {
   public:
    using difference_type = typename Iterator::difference_type;
    using reference = typename Iterator::reference;

    Range(const Iterator &begin, const Iterator &end)
        : begin_(begin), end_(end) {}
    // Make `range` type convertible to `const_range`.
    Range(const Range<iterator> &range) : Range(range.begin(), range.end()) {}

    Iterator begin() const { return begin_; }
    Iterator end() const { return end_; }

    difference_type size() const { return end_ - begin_; }
    bool empty() const { return begin_ == end_; }

    reference operator[](difference_type index) const {
      CHECK_GE(index, 0);
      CHECK_LT(index, size());
      return *(begin_ + index);
    }
    reference front() const {
      CHECK(!empty());
      return *begin_;
    }
    reference back() const {
      CHECK(!empty());
      return *(end_ - 1);
    }

    // Skip first `count` elements, similar to `std::ranges::views::drop`.
    Range drop(difference_type count) const {
      CHECK_GE(count, 0);
      return Range{count < size() ? begin_ + count : end_, end_};
    }
    // Take first `count` elements, similar to `std::ranges::views::take`.
    Range take(difference_type count) const {
      CHECK_GE(count, 0);
      return Range{begin_, count < size() ? begin_ + count : end_};
    }
    // Take `count` segments from the end.
    Range take_last(difference_type count) const {
      CHECK_GE(count, 0);
      return drop(std::max<difference_type>(0, size() - count));
    }
    // Same as `drop(drop_count).take(count)`, providing better readability.
    // `std::ranges::subrange` may not provide the same argument types though.
    Range subrange(difference_type index, difference_type count) const {
      return drop(index).take(count);
    }

   private:
    Iterator begin_;
    Iterator end_;
  };
  using range = Range<iterator>;
  using const_range = Range<const_iterator>;

  // constructors
  Segments()
      : max_history_segments_size_(0),
        resized_(false),
        pool_(32),
        cached_lattice_() {}

  Segments(const Segments &x);
  Segments &operator=(const Segments &x);

  // iterators
  iterator begin() { return iterator{segments_.begin()}; }
  iterator end() { return iterator{segments_.end()}; }
  const_iterator begin() const { return const_iterator{segments_.begin()}; }
  const_iterator end() const { return const_iterator{segments_.end()}; }

  // ranges
  template <typename Iterator>
  static Range<Iterator> make_range(const Iterator &begin,
                                    const Iterator &end) {
    return Range<Iterator>(begin, end);
  }

  range all() { return make_range(begin(), end()); }
  const_range all() const { return make_range(begin(), end()); }
  range history_segments();
  const_range history_segments() const;
  range conversion_segments();
  const_range conversion_segments() const;

  // getter
  const Segment &segment(size_t i) const { return *segments_[i]; }
  const Segment &conversion_segment(size_t i) const {
    return *segments_[i + history_segments_size()];
  }
  const Segment &history_segment(size_t i) const { return *segments_[i]; }

  // setter
  Segment *mutable_segment(size_t i) { return segments_[i]; }
  Segment *mutable_conversion_segment(size_t i) {
    return segments_[i + history_segments_size()];
  }
  Segment *mutable_history_segment(size_t i) { return segments_[i]; }

  // push and insert segments
  Segment *push_front_segment();
  Segment *push_back_segment();
  // alias of push_back_segment()
  Segment *add_segment() { return push_back_segment(); }
  Segment *insert_segment(size_t i);

  // get size of segments
  size_t segments_size() const { return segments_.size(); }
  size_t history_segments_size() const;
  size_t conversion_segments_size() const {
    return (segments_size() - history_segments_size());
  }

  // erase segment
  void pop_front_segment();
  void pop_back_segment();
  void erase_segment(size_t i);
  iterator erase_segment(iterator position);
  void erase_segments(size_t i, size_t size);
  iterator erase_segments(iterator first, iterator last);

  // erase all segments
  void clear_history_segments();
  void clear_conversion_segments();
  void clear_segments();

  void set_max_history_segments_size(size_t max_history_segments_size);
  size_t max_history_segments_size() const {
    return max_history_segments_size_;
  }

  bool resized() const { return resized_; }
  void set_resized(bool resized) { resized_ = resized; }

  // Returns history key of `size` segments.
  // Returns all history key when size == -1.
  std::string history_key(int size = -1) const;

  // Returns history value of `size` segments.
  // Returns all history value when size == -1.
  std::string history_value(int size = -1) const;

  // Initializes the segments with the given key as a preparation of conversion.
  void InitForConvert(absl::string_view key);

  // Initializes the segments with the given key and value as a preparation of
  // committing the value.
  void InitForCommit(absl::string_view key, absl::string_view value);

  // clear segments
  void Clear();

  // Dump Segments structure
  std::string DebugString() const;

  // Prepend the candidates of `previous_segment` to the first conversion
  // segment. This is used to merge the previous suggestion results to the
  // prediction results.
  void PrependCandidates(const Segment &previous_segment);

  // Resize the segments starting from `start_index` with the given `new_sizes`.
  // Returns true if the segments are resized.
  bool Resize(size_t start_index, absl::Span<const uint8_t> new_sizes);

  friend std::ostream &operator<<(std::ostream &os, const Segments &segments) {
    return os << segments.DebugString();
  }

  // Revert id.
  // Client remembers the last finish operation with `revert_id`.
  // TODO(taku): support multiple revert ids to support undo/redo.
  void set_revert_id(uint64_t revert_id) { revert_id_ = revert_id; }
  uint64_t revert_id() const { return revert_id_; }

  // setter
  Lattice *mutable_cached_lattice() { return &cached_lattice_; }

 private:
  FRIEND_TEST(SegmentsTest, BasicTest);

  iterator history_segments_end();
  const_iterator history_segments_end() const;

  // LINT.IfChange
  size_t max_history_segments_size_;
  bool resized_;

  ObjectPool<Segment> pool_;
  std::deque<Segment *> segments_;
  uint64_t revert_id_ = 0;
  Lattice cached_lattice_;
  // LINT.ThenChange(//converter/segments_matchers.h)
};

inline bool Segment::is_valid_index(int i) const {
  if (i < 0) {
    return (-i - 1 < meta_candidates_.size());
  } else {
    return (i < candidates_.size());
  }
}

inline const converter::Candidate &Segment::candidate(int i) const {
  if (i < 0) {
    return meta_candidate(-i - 1);
  }
  DCHECK(i < candidates_.size());
  return *candidates_[i];
}

inline converter::Candidate *Segment::mutable_candidate(int i) {
  if (i < 0) {
    const size_t meta_index = -i - 1;
    DCHECK_LT(meta_index, meta_candidates_.size());
    return &meta_candidates_[meta_index];
  }
  DCHECK_LT(i, candidates_.size());
  return candidates_[i];
}

}  // namespace mozc

#endif  // MOZC_CONVERTER_SEGMENTS_H_
