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
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <ostream>
#include <sstream>  // For DebugString()
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/util.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace {
constexpr size_t kMaxHistorySize = 32;
}  // namespace

absl::string_view Segment::Candidate::functional_key() const {
  return key.size() <= content_key.size()
             ? absl::string_view()
             : absl::string_view(key.data() + content_key.size(),
                                 key.size() - content_key.size());
}

absl::string_view Segment::Candidate::functional_value() const {
  return value.size() <= content_value.size()
             ? absl::string_view()
             : absl::string_view(value.data() + content_value.size(),
                                 value.size() - content_value.size());
}

void Segment::Candidate::Init() {
  key.clear();
  value.clear();
  content_value.clear();
  content_key.clear();
  consumed_key_size = 0;
  prefix.clear();
  suffix.clear();
  description.clear();
  usage_title.clear();
  usage_description.clear();
  cost = 0;
  structure_cost = 0;
  wcost = 0;
  lid = 0;
  rid = 0;
  usage_id = 0;
  attributes = 0;
  source_info = SOURCE_INFO_NONE;
  style = NumberUtil::NumberString::DEFAULT_STYLE;
  command = DEFAULT_COMMAND;
  inner_segment_boundary.clear();
#ifndef NDEBUG
  log.clear();
#endif  // NDEBUG
}

bool Segment::Candidate::IsValid() const {
  if (inner_segment_boundary.empty()) {
    return true;
  }
  // The sums of the lengths of key, value components must coincide with those
  // of key, value, respectively.
  size_t sum_key_len = 0, sum_value_len = 0;
  for (InnerSegmentIterator iter(this); !iter.Done(); iter.Next()) {
    sum_key_len += iter.GetKey().size();
    sum_value_len += iter.GetValue().size();
  }
  return sum_key_len == key.size() && sum_value_len == value.size();
}

bool Segment::Candidate::EncodeLengths(size_t key_len, size_t value_len,
                                       size_t content_key_len,
                                       size_t content_value_len,
                                       uint32_t *result) {
  if (key_len > std::numeric_limits<uint8_t>::max() ||
      value_len > std::numeric_limits<uint8_t>::max() ||
      content_key_len > std::numeric_limits<uint8_t>::max() ||
      content_value_len > std::numeric_limits<uint8_t>::max()) {
    return false;
  }
  *result = (static_cast<uint32_t>(key_len) << 24) |
            (static_cast<uint32_t>(value_len) << 16) |
            (static_cast<uint32_t>(content_key_len) << 8) |
            static_cast<uint32_t>(content_value_len);
  return true;
}

bool Segment::Candidate::PushBackInnerSegmentBoundary(
    size_t key_len, size_t value_len, size_t content_key_len,
    size_t content_value_len) {
  uint32_t encoded;
  if (EncodeLengths(key_len, value_len, content_key_len, content_value_len,
                    &encoded)) {
    inner_segment_boundary.push_back(encoded);
    return true;
  }
  return false;
}

std::string Segment::Candidate::DebugString() const {
  std::stringstream os;
  os << "(key=" << key << " ckey=" << content_key << " val=" << value
     << " cval=" << content_value << " cost=" << cost
     << " scost=" << structure_cost << " wcost=" << wcost << " lid=" << lid
     << " rid=" << rid << " attributes=" << attributes
     << " consumed_key_size=" << consumed_key_size;
  if (!prefix.empty()) {
    os << " prefix=" << prefix;
  }
  if (!suffix.empty()) {
    os << " suffix=" << suffix;
  }
  if (!description.empty()) {
    os << " description=" << description;
  }
  if (!inner_segment_boundary.empty()) {
    os << " segbdd=";
    for (size_t i = 0; i < inner_segment_boundary.size(); ++i) {
      const uint32_t encoded_lengths = inner_segment_boundary[i];
      const int key_len = encoded_lengths >> 24;
      const int value_len = (encoded_lengths >> 16) & 0xff;
      const int content_key_len = (encoded_lengths >> 8) & 0xff;
      const int content_value_len = encoded_lengths & 0xff;
      os << absl::StrFormat("<%d,%d,%d,%d>", key_len, value_len,
                            content_key_len, content_value_len);
    }
  }
  os << ")" << std::endl;
  return os.str();
}

void Segment::Candidate::InnerSegmentIterator::Next() {
  DCHECK_LT(index_, candidate_->inner_segment_boundary.size());
  const uint32_t encoded_lengths = candidate_->inner_segment_boundary[index_++];
  key_offset_ += encoded_lengths >> 24;
  value_offset_ += (encoded_lengths >> 16) & 0xff;
}

absl::string_view Segment::Candidate::InnerSegmentIterator::GetKey() const {
  DCHECK_LT(index_, candidate_->inner_segment_boundary.size());
  const uint32_t encoded_lengths = candidate_->inner_segment_boundary[index_];
  return absl::string_view(key_offset_, encoded_lengths >> 24);
}

absl::string_view Segment::Candidate::InnerSegmentIterator::GetValue() const {
  DCHECK_LT(index_, candidate_->inner_segment_boundary.size());
  const uint32_t encoded_lengths = candidate_->inner_segment_boundary[index_];
  return absl::string_view(value_offset_, (encoded_lengths >> 16) & 0xff);
}

absl::string_view Segment::Candidate::InnerSegmentIterator::GetContentKey()
    const {
  DCHECK_LT(index_, candidate_->inner_segment_boundary.size());
  const uint32_t encoded_lengths = candidate_->inner_segment_boundary[index_];
  return absl::string_view(key_offset_, (encoded_lengths >> 8) & 0xff);
}

absl::string_view Segment::Candidate::InnerSegmentIterator::GetContentValue()
    const {
  DCHECK_LT(index_, candidate_->inner_segment_boundary.size());
  const uint32_t encoded_lengths = candidate_->inner_segment_boundary[index_];
  return absl::string_view(value_offset_, encoded_lengths & 0xff);
}

constexpr int kCandidatesPoolSize = 16;
Segment::Segment() : segment_type_(FREE), pool_(kCandidatesPoolSize) {}

Segment::Segment(const Segment &x)
    : removed_candidates_for_debug_(x.removed_candidates_for_debug_),
      segment_type_(x.segment_type_),
      key_(x.key_),
      meta_candidates_(x.meta_candidates_),
      pool_(x.pool_.size()) {
  // Deep-copy candidates.
  for (const Candidate *cand : x.candidates_) {
    *add_candidate() = *cand;
  }
}

Segment &Segment::operator=(const Segment &x) {
  removed_candidates_for_debug_ = x.removed_candidates_for_debug_;
  segment_type_ = x.segment_type_;
  key_ = x.key_;
  meta_candidates_ = x.meta_candidates_;
  // Deep-copy candidates.
  clear_candidates();
  for (const Candidate *cand : x.candidates_) {
    *add_candidate() = *cand;
  }
  return *this;
}

Segment::~Segment() = default;

Segment::SegmentType Segment::segment_type() const { return segment_type_; }

void Segment::set_segment_type(const Segment::SegmentType &segment_type) {
  segment_type_ = segment_type;
}

const std::string &Segment::key() const { return key_; }

void Segment::set_key(absl::string_view key) {
  key_.assign(key.data(), key.size());
}

bool Segment::is_valid_index(int i) const {
  if (i < 0) {
    return (-i - 1 < meta_candidates_.size());
  } else {
    return (i < candidates_.size());
  }
}

const Segment::Candidate &Segment::candidate(int i) const {
  if (i < 0) {
    return meta_candidate(-i - 1);
  }
  DCHECK(i < candidates_.size());
  return *candidates_[i];
}

Segment::Candidate *Segment::mutable_candidate(int i) {
  if (i < 0) {
    const size_t meta_index = -i - 1;
    DCHECK_LT(meta_index, meta_candidates_.size());
    return &meta_candidates_[meta_index];
  }
  DCHECK_LT(i, candidates_.size());
  return candidates_[i];
}

size_t Segment::candidates_size() const { return candidates_.size(); }

void Segment::clear_candidates() {
  pool_.clear();
  candidates_.clear();
}

Segment::Candidate *Segment::push_back_candidate() {
  Candidate *candidate =
      pool_.emplace_back(std::make_unique<Candidate>()).get();
  candidate->Init();
  candidates_.push_back(candidate);
  return candidate;
}

Segment::Candidate *Segment::push_front_candidate() {
  Candidate *candidate =
      pool_.emplace_back(std::make_unique<Candidate>()).get();
  candidate->Init();
  candidates_.push_front(candidate);
  return candidate;
}

Segment::Candidate *Segment::add_candidate() { return push_back_candidate(); }

Segment::Candidate *Segment::insert_candidate(int i) {
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
  candidate->Init();
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
    int i, std::vector<std::unique_ptr<Candidate>> &&candidates) {
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
    candidates_.pop_front();
  }
}

void Segment::pop_back_candidate() {
  if (!candidates_.empty()) {
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

size_t Segment::meta_candidates_size() const { return meta_candidates_.size(); }

void Segment::clear_meta_candidates() { meta_candidates_.clear(); }

const std::vector<Segment::Candidate> &Segment::meta_candidates() const {
  return meta_candidates_;
}

std::vector<Segment::Candidate> *Segment::mutable_meta_candidates() {
  return &meta_candidates_;
}

const Segment::Candidate &Segment::meta_candidate(size_t i) const {
  if (i >= meta_candidates_.size()) {
    LOG(ERROR) << "Invalid index number of meta_candidate: " << i;
    i = 0;
  }
  return meta_candidates_[i];
}

Segment::Candidate *Segment::mutable_meta_candidate(size_t i) {
  if (i >= meta_candidates_.size()) {
    LOG(ERROR) << "Invalid index number of meta_candidate: " << i;
    i = 0;
  }
  return &meta_candidates_[i];
}

Segment::Candidate *Segment::add_meta_candidate() {
  Candidate candidate;
  candidate.Init();
  meta_candidates_.push_back(candidate);
  return &meta_candidates_[meta_candidates_size() - 1];
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
    VLOG(1) << "old_idx and new_idx are the same";
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
  meta_candidates_.clear();
  segment_type_ = FREE;
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

Segments::Segments()
    : max_history_segments_size_(0),
      resized_(false),
      pool_(32),
      cached_lattice_(new Lattice()) {}

Segments::Segments(const Segments &x)
    : max_history_segments_size_(x.max_history_segments_size_),
      resized_(x.resized_),
      pool_(32),
      revert_entries_(x.revert_entries_),
      cached_lattice_(new Lattice()) {
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

Segments::~Segments() = default;

const Segment &Segments::segment(size_t i) const { return *segments_[i]; }

Segment *Segments::mutable_segment(size_t i) { return segments_[i]; }

const Segment &Segments::history_segment(size_t i) const {
  return *segments_[i];
}

Segment *Segments::mutable_history_segment(size_t i) { return segments_[i]; }

const Segment &Segments::conversion_segment(size_t i) const {
  return *segments_[i + history_segments_size()];
}

Segment *Segments::mutable_conversion_segment(size_t i) {
  return segments_[i + history_segments_size()];
}

Segment *Segments::add_segment() { return push_back_segment(); }

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

size_t Segments::segments_size() const { return segments_.size(); }

size_t Segments::history_segments_size() const {
  size_t result = 0;
  for (size_t i = 0; i < segments_size(); ++i) {
    if (segment(i).segment_type() != Segment::HISTORY &&
        segment(i).segment_type() != Segment::SUBMITTED) {
      break;
    }
    ++result;
  }
  return result;
}

size_t Segments::conversion_segments_size() const {
  return (segments_size() - history_segments_size());
}

void Segments::erase_segment(size_t i) {
  if (i >= segments_size()) {
    return;
  }
  pool_.Release(mutable_segment(i));
  segments_.erase(segments_.begin() + i);
}

void Segments::erase_segments(size_t i, size_t size) {
  const size_t end = i + size;
  if (i >= segments_size() || end > segments_size()) {
    return;
  }
  for (size_t j = i; j < end; ++j) {
    pool_.Release(mutable_segment(j));
  }
  segments_.erase(segments_.begin() + i, segments_.begin() + end);
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

void Segments::Clear() {
  clear_segments();
  clear_revert_entries();
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
  const size_t size = history_segments_size();
  for (size_t i = size; i < segments_size(); ++i) {
    pool_.Release(mutable_segment(i));
  }
  resized_ = false;
  segments_.resize(size);
}

size_t Segments::max_history_segments_size() const {
  return max_history_segments_size_;
}

void Segments::set_max_history_segments_size(size_t max_history_segments_size) {
  max_history_segments_size_ =
      std::max(static_cast<size_t>(0),
               std::min(max_history_segments_size, kMaxHistorySize));
}

void Segments::set_resized(bool resized) { resized_ = resized; }

bool Segments::resized() const { return resized_; }

void Segments::clear_revert_entries() { revert_entries_.clear(); }

size_t Segments::revert_entries_size() const { return revert_entries_.size(); }

Segments::RevertEntry *Segments::push_back_revert_entry() {
  revert_entries_.resize(revert_entries_.size() + 1);
  Segments::RevertEntry *entry = &revert_entries_.back();
  entry->revert_entry_type = Segments::RevertEntry::CREATE_ENTRY;
  entry->id = 0;
  entry->timestamp = 0;
  entry->key.clear();
  return entry;
}

const Segments::RevertEntry &Segments::revert_entry(size_t i) const {
  return revert_entries_[i];
}

Segments::RevertEntry *Segments::mutable_revert_entry(size_t i) {
  return &revert_entries_[i];
}

Lattice *Segments::mutable_cached_lattice() { return cached_lattice_.get(); }

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
