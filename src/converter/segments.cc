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

#include "converter/segments.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <iterator>
#include <memory>
#include <numeric>
#include <ostream>
#include <sstream>  // For DebugString()
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/util.h"
#include "base/vlog.h"
#include "converter/candidate.h"

namespace mozc {

using ::mozc::converter::Candidate;

namespace {
constexpr size_t kMaxHistorySize = 32;
}  // namespace

Segment::Segment(const Segment &x)
    : removed_candidates_for_debug_(x.removed_candidates_for_debug_),
      segment_type_(x.segment_type_),
      key_(x.key_),
      key_len_(x.key_len_),
      meta_candidates_(x.meta_candidates_) {
  DeepCopyCandidates(x.candidates_);
}

Segment &Segment::operator=(const Segment &x) {
  removed_candidates_for_debug_ = x.removed_candidates_for_debug_;
  segment_type_ = x.segment_type_;
  key_ = x.key_;
  key_len_ = x.key_len_;
  meta_candidates_ = x.meta_candidates_;

  clear_candidates();
  DeepCopyCandidates(x.candidates_);

  return *this;
}

void Segment::clear_candidates() {
  pool_.clear();
  candidates_.clear();
}

Candidate *Segment::push_back_candidate() {
  auto candidate = std::make_unique<Candidate>();
  Candidate *ptr = candidate.get();
  pool_.push_back(std::move(candidate));
  candidates_.push_back(ptr);
  return ptr;
}

Candidate *Segment::push_front_candidate() {
  auto candidate = std::make_unique<Candidate>();
  Candidate *ptr = candidate.get();
  pool_.push_back(std::move(candidate));
  candidates_.push_front(ptr);
  return ptr;
}

Candidate *Segment::insert_candidate(int i) {
  if (i < 0) {
    LOG(WARNING) << "Invalid insert position [negative]: " << i << " / "
                 << candidates_.size();
    return nullptr;
  }
  if (i > static_cast<int>(candidates_.size())) {
    LOG(DFATAL) << "Invalid insert position [out of bounds]: " << i << " / "
                << candidates_.size();
    i = static_cast<int>(candidates_.size());
  }
  Candidate *candidate =
      pool_.emplace_back(std::make_unique<Candidate>()).get();
  candidates_.insert(candidates_.begin() + i, candidate);
  return candidate;
}

void Segment::insert_candidate(int i, std::unique_ptr<Candidate> candidate) {
  Candidate *cand_ptr = pool_.emplace_back(std::move(candidate)).get();
  if (i <= 0) {
    candidates_.push_front(cand_ptr);
  } else if (i >= static_cast<int>(candidates_.size())) {
    candidates_.push_back(cand_ptr);
  } else {
    candidates_.insert(candidates_.begin() + i, cand_ptr);
  }
}

void Segment::insert_candidates(
    int i, std::vector<std::unique_ptr<Candidate>> candidates) {
  if (i < 0) {
    i = 0;
  } else if (i > static_cast<int>(candidates_.size())) {
    i = static_cast<int>(candidates_.size());
  }

  const size_t orig_size = candidates_.size();
  candidates_.resize(orig_size + candidates.size());
  std::copy_backward(candidates_.begin() + i, candidates_.begin() + orig_size,
                     candidates_.end());
  for (const auto &candidate : candidates) {
    candidates_[i++] = candidate.get();
  }

  pool_.insert(pool_.end(), std::make_move_iterator(candidates.begin()),
               std::make_move_iterator(candidates.end()));
}

void Segment::pop_front_candidate() {
  if (!candidates_.empty()) {
    // The unique_ptr in pool_ is deleted when the candidate is deleted.
    candidates_.pop_front();
  }
}

void Segment::pop_back_candidate() {
  if (!candidates_.empty()) {
    // The unique_ptr in pool_ is deleted when the candidate is deleted.
    candidates_.pop_back();
  }
}

void Segment::erase_candidate(int i) {
  if (i < 0 || i >= static_cast<int>(candidates_size())) {
    LOG(WARNING) << "invalid index";
    return;
  }
  candidates_.erase(candidates_.begin() + i);
}

void Segment::erase_candidates(int i, size_t size) {
  const size_t end = i + size;
  if (i < 0 || i >= static_cast<int>(candidates_size()) ||
      end > candidates_size()) {
    LOG(WARNING) << "invalid index";
    return;
  }
  candidates_.erase(candidates_.begin() + i, candidates_.begin() + end);
}

const Candidate &Segment::meta_candidate(size_t i) const {
  if (i >= meta_candidates_.size()) {
    LOG(ERROR) << "Invalid index number of meta_candidate: " << i;
    i = 0;
  }
  return meta_candidates_[i];
}

Candidate *Segment::mutable_meta_candidate(size_t i) {
  if (i >= meta_candidates_.size()) {
    LOG(ERROR) << "Invalid index number of meta_candidate: " << i;
    i = 0;
  }
  return &meta_candidates_[i];
}

Candidate *Segment::add_meta_candidate() {
  return &meta_candidates_.emplace_back();
}

void Segment::move_candidate(int old_idx, int new_idx) {
  // meta candidates
  if (old_idx < 0) {
    const int meta_idx = -old_idx - 1;
    DCHECK_LT(meta_idx, meta_candidates_size());
    Candidate *c = insert_candidate(new_idx);
    *c = meta_candidates_[meta_idx];
    return;
  }

  // normal segment
  if (old_idx < 0 || old_idx >= static_cast<int>(candidates_size()) ||
      new_idx >= static_cast<int>(candidates_size()) || old_idx == new_idx) {
    MOZC_VLOG(1) << "old_idx and new_idx are the same";
    return;
  }
  if (old_idx > new_idx) {  // promotion
    Candidate *c = candidates_[old_idx];
    for (int i = old_idx; i >= new_idx + 1; --i) {
      candidates_[i] = candidates_[i - 1];
    }
    candidates_[new_idx] = c;
  } else {  // demotion
    Candidate *c = candidates_[old_idx];
    for (int i = old_idx; i < new_idx; ++i) {
      candidates_[i] = candidates_[i + 1];
    }
    candidates_[new_idx] = c;
  }
}

void Segment::Clear() {
  clear_candidates();
  key_.clear();
  key_len_ = 0;
  meta_candidates_.clear();
  segment_type_ = FREE;
}

void Segment::DeepCopyCandidates(const std::deque<Candidate *> &candidates) {
  DCHECK(pool_.empty());
  pool_.reserve(candidates.size());
  for (const Candidate *cand : candidates) {
    auto new_cand = std::make_unique<Candidate>(*cand);
    candidates_.push_back(new_cand.get());
    pool_.push_back(std::move(new_cand));
  }
}

std::string Segment::DebugString() const {
  std::stringstream os;
  os << "[segtype=" << segment_type() << " key=" << key() << std::endl;
  const int size = static_cast<int>(candidates_size() + meta_candidates_size());
  for (int l = 0; l < size; ++l) {
    int j = 0;
    if (l < meta_candidates_size()) {
      j = -l - 1;
    } else {
      j = l - meta_candidates_size();
    }
    os << "    cand " << j << " " << candidate(j).DebugString();
  }
  os << "]" << std::endl;
  return os.str();
}

Segments::Segments(const Segments &x)
    : max_history_segments_size_(x.max_history_segments_size_),
      resized_(x.resized_),
      pool_(32),
      revert_entries_(x.revert_entries_),
      cached_lattice_() {
  // Deep-copy segments.
  for (const Segment *segment : x.segments_) {
    *add_segment() = *segment;
  }
  // Note: cached_lattice_ is not copied to follow the old copy policy.
  // TODO(noriyukit): This design is not intuitive. It'd be better to manage
  // cached_lattice_ in a better way.
}

Segments &Segments::operator=(const Segments &x) {
  Clear();

  max_history_segments_size_ = x.max_history_segments_size_;
  resized_ = x.resized_;
  // Deep-copy segments.
  for (const Segment *segment : x.segments_) {
    *add_segment() = *segment;
  }
  revert_entries_ = x.revert_entries_;
  // Note: cached_lattice_ is not copied; see the comment for the copy
  // constructor.
  return *this;
}

Segment *Segments::insert_segment(size_t i) {
  Segment *segment = pool_.Alloc();
  segment->Clear();
  segments_.insert(segments_.begin() + i, segment);
  return segment;
}

Segment *Segments::push_back_segment() {
  Segment *segment = pool_.Alloc();
  segment->Clear();
  segments_.push_back(segment);
  return segment;
}

Segment *Segments::push_front_segment() {
  Segment *segment = pool_.Alloc();
  segment->Clear();
  segments_.push_front(segment);
  return segment;
}

// Returns an `Iterator` for the end of history segments.
template <typename Iterator>
static Iterator history_segments_end_of(Iterator begin, Iterator end) {
  Iterator it = begin;
  for (; it != end; ++it) {
    const Segment &segment = **it;
    if (segment.segment_type() != Segment::HISTORY &&
        segment.segment_type() != Segment::SUBMITTED) {
      break;
    }
  }
  return it;
}

Segments::const_iterator Segments::history_segments_end() const {
  return const_iterator{
      history_segments_end_of<decltype(segments_)::const_iterator>(
          segments_.begin(), segments_.end())};
}

Segments::iterator Segments::history_segments_end() {
  return iterator{history_segments_end_of<decltype(segments_)::iterator>(
      segments_.begin(), segments_.end())};
}

size_t Segments::history_segments_size() const {
  const auto end = history_segments_end_of<decltype(segments_)::const_iterator>(
      segments_.begin(), segments_.end());
  return end - segments_.begin();
}

Segments::range Segments::history_segments() {
  return make_range(begin(), history_segments_end());
}

Segments::const_range Segments::history_segments() const {
  return make_range(begin(), history_segments_end());
}

Segments::range Segments::conversion_segments() {
  return make_range(history_segments_end(), end());
}

Segments::const_range Segments::conversion_segments() const {
  return make_range(history_segments_end(), end());
}

void Segments::erase_segment(size_t i) {
  if (i >= segments_size()) {
    return;
  }
  erase_segment(begin() + i);
}

Segments::iterator Segments::erase_segment(iterator position) {
  pool_.Release(&*position);
  return iterator{segments_.erase(position.iterator_)};
}

void Segments::erase_segments(size_t i, size_t size) {
  const size_t end = i + size;
  if (i >= segments_size() || end > segments_size()) {
    return;
  }
  erase_segments(begin() + i, begin() + end);
}

Segments::iterator Segments::erase_segments(iterator first, iterator last) {
  for (auto it = first; it != last; ++it) {
    pool_.Release(&*it);
  }
  return iterator{segments_.erase(first.iterator_, last.iterator_)};
}

void Segments::pop_front_segment() {
  if (!segments_.empty()) {
    Segment *seg = segments_.front();
    pool_.Release(seg);
    segments_.pop_front();
  }
}

void Segments::pop_back_segment() {
  if (!segments_.empty()) {
    Segment *seg = segments_.back();
    pool_.Release(seg);
    segments_.pop_back();
  }
}

void Segments::InitForConvert(absl::string_view key) {
  set_max_history_segments_size(4);
  clear_conversion_segments();

  Segment *segment = add_segment();
  segment->set_key(key);
  segment->set_segment_type(mozc::Segment::FREE);

  MOZC_VLOG(2) << DebugString();
}

void Segments::InitForCommit(absl::string_view key, absl::string_view value) {
  clear_conversion_segments();

  Segment *segment = add_segment();
  segment->set_key(key);
  segment->set_segment_type(Segment::FIXED_VALUE);

  Candidate *candidate = segment->add_candidate();
  candidate->key = std::string(key);
  candidate->content_key = std::string(key);
  candidate->value = std::string(value);
  candidate->content_value = std::string(value);
}

void Segments::Clear() {
  clear_segments();
  clear_revert_entries();
}

void Segments::PrependCandidates(const Segment &previous_segment) {
  if (conversion_segments_size() == 0) {
    clear_conversion_segments();
    Segment *segment = add_segment();
    segment->set_key(previous_segment.key());
  }

  DCHECK_EQ(conversion_segments_size(), 1);
  Segment *segment = mutable_conversion_segment(0);
  DCHECK(segment);

  const size_t cands_size = previous_segment.candidates_size();
  for (size_t i = 0; i < cands_size; ++i) {
    Candidate *candidate = segment->insert_candidate(i);
    *candidate = previous_segment.candidate(i);
  }
  segment->mutable_meta_candidates()->assign(
      previous_segment.meta_candidates().begin(),
      previous_segment.meta_candidates().end());
}

bool Segments::Resize(size_t start_index, absl::Span<const uint8_t> new_sizes) {
  const size_t total_size =
      std::accumulate(new_sizes.begin(), new_sizes.end(), 0);
  if (total_size == 0) {
    return false;
  }

  std::string key;
  size_t key_len = 0;
  size_t modified_segments_size = 0;
  for (const Segment &segment : all().drop(start_index)) {
    absl::StrAppend(&key, segment.key());
    key_len += segment.key_len();
    ++modified_segments_size;
    if (key_len >= total_size) {
      break;
    }
  }

  // If key is empty or less than the total size of new segments, return false.
  if (key_len == 0 || key_len < total_size) {
    return false;
  }

  size_t consumed = 0;
  std::vector<std::string> new_keys;
  new_keys.reserve(new_sizes.size());

  for (size_t new_size : new_sizes) {
    if (new_size != 0 && consumed < key_len) {
      new_keys.emplace_back(Util::Utf8SubString(key, consumed, new_size));
      consumed += new_size;
    }
  }

  erase_segments(start_index, modified_segments_size);

  for (size_t i = 0; i < new_keys.size(); ++i) {
    Segment *seg = insert_segment(start_index + i);
    seg->set_segment_type(Segment::FIXED_BOUNDARY);
    seg->set_key(std::move(new_keys[i]));
  }

  // If there is a remaining key, replace the next segment with the new key
  // prepending the remaining key to the next segment as a FREE type.
  if (consumed < key_len) {
    std::string next_segment_key(
        Util::Utf8SubString(key, consumed, key_len - consumed));
    const size_t next_segment_index = start_index + new_keys.size();
    if (next_segment_index < segments_size()) {
      absl::StrAppend(&next_segment_key, segment(next_segment_index).key());
      erase_segment(next_segment_index);
    }
    Segment *seg = insert_segment(next_segment_index);
    seg->set_segment_type(Segment::FREE);
    seg->set_key(next_segment_key);
  }

  set_resized(true);
  return true;
}

void Segments::clear_segments() {
  pool_.Free();
  resized_ = false;
  segments_.clear();
}

void Segments::clear_history_segments() {
  while (!segments_.empty()) {
    Segment *seg = segments_.front();
    if (seg->segment_type() != Segment::HISTORY &&
        seg->segment_type() != Segment::SUBMITTED) {
      break;
    }
    pop_front_segment();
  }
}

void Segments::clear_conversion_segments() {
  resized_ = false;
  erase_segments(history_segments_end(), end());
}

void Segments::set_max_history_segments_size(size_t max_history_segments_size) {
  max_history_segments_size_ =
      std::max(static_cast<size_t>(0),
               std::min(max_history_segments_size, kMaxHistorySize));
}

Segments::RevertEntry *Segments::push_back_revert_entry() {
  revert_entries_.resize(revert_entries_.size() + 1);
  Segments::RevertEntry *entry = &revert_entries_.back();
  entry->revert_entry_type = Segments::RevertEntry::CREATE_ENTRY;
  entry->id = 0;
  entry->timestamp = 0;
  entry->key.clear();
  return entry;
}

std::string Segments::history_key(int size) const {
  const_range segments = history_segments();
  if (size != -1) {
    segments = segments.take_last(size);
  }

  std::string history_key;
  for (const Segment &seg : segments) {
    if (seg.candidates_size() == 0) continue;
    history_key.append(seg.candidate(0).key);
  }

  return history_key;
}

std::string Segments::history_value(int size) const {
  const_range segments = history_segments();
  if (size != -1) {
    segments = segments.take_last(size);
  }

  std::string history_value;
  for (const Segment &seg : segments) {
    if (seg.candidates_size() == 0) continue;
    history_value.append(seg.candidate(0).value);
  }

  return history_value;
}

std::string Segments::DebugString() const {
  std::stringstream os;
  os << "{" << std::endl;
  for (size_t i = 0; i < segments_size(); ++i) {
    os << "  seg " << i << " " << segment(i).DebugString();
  }
  os << "}" << std::endl;
  return os.str();
}

}  // namespace mozc
