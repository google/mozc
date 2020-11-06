// Copyright 2010-2020, Google Inc.
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
#include <limits>
#include <sstream>  // For DebugString()
#include <string>

#include "base/logging.h"
#include "base/util.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace {
const size_t kMaxHistorySize = 32;
const size_t kMaxConversionCandidatesSize = 200;
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

void Segment::Candidate::CopyFrom(const Candidate &src) {
  Init();

  key = src.key;
  value = src.value;
  content_key = src.content_key;
  content_value = src.content_value;
  consumed_key_size = src.consumed_key_size;

  prefix = src.prefix;
  suffix = src.suffix;
  usage_id = src.usage_id;
  description = src.description;
  usage_title = src.usage_title;
  usage_description = src.usage_description;

  cost = src.cost;
  wcost = src.wcost;
  structure_cost = src.structure_cost;

  lid = src.lid;
  rid = src.rid;

  attributes = src.attributes;

  style = src.style;
  command = src.command;

  inner_segment_boundary = src.inner_segment_boundary;
}

bool Segment::Candidate::IsValid() const {
  if (inner_segment_boundary.empty()) {
    return true;
  }
  // The sums of the lengths of key, value, content key and content value
  // components must coincide with those of key, value, content_key and
  // content_value, respectively.
  size_t sum_key_len = 0, sum_value_len = 0;
  size_t sum_content_key_len = 0, sum_content_value_len = 0;
  for (InnerSegmentIterator iter(this); !iter.Done(); iter.Next()) {
    sum_key_len += iter.GetKey().size();
    sum_value_len += iter.GetValue().size();
    sum_content_key_len += iter.GetContentKey().size();
    sum_content_value_len += iter.GetContentValue().size();
  }
  if (sum_key_len != key.size() || sum_value_len != value.size() ||
      sum_content_key_len != content_key.size() ||
      sum_content_value_len != content_value.size()) {
    return false;
  }
  return true;
}

bool Segment::Candidate::EncodeLengths(size_t key_len, size_t value_len,
                                       size_t content_key_len,
                                       size_t content_value_len,
                                       uint32 *result) {
  if (key_len > std::numeric_limits<uint8>::max() ||
      value_len > std::numeric_limits<uint8>::max() ||
      content_key_len > std::numeric_limits<uint8>::max() ||
      content_value_len > std::numeric_limits<uint8>::max()) {
    return false;
  }
  *result = (static_cast<uint32>(key_len) << 24) |
            (static_cast<uint32>(value_len) << 16) |
            (static_cast<uint32>(content_key_len) << 8) |
            static_cast<uint32>(content_value_len);
  return true;
}

bool Segment::Candidate::PushBackInnerSegmentBoundary(
    size_t key_len, size_t value_len, size_t content_key_len,
    size_t content_value_len) {
  uint32 encoded;
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
      const uint32 encoded_lengths = inner_segment_boundary[i];
      const int key_len = encoded_lengths >> 24;
      const int value_len = (encoded_lengths >> 16) & 0xff;
      const int content_key_len = (encoded_lengths >> 8) & 0xff;
      const int content_value_len = encoded_lengths & 0xff;
      os << Util::StringPrintf("<%d,%d,%d,%d>", key_len, value_len,
                               content_key_len, content_value_len);
    }
  }
  os << ")" << std::endl;
  return os.str();
}

void Segment::Candidate::InnerSegmentIterator::Next() {
  DCHECK_LT(index_, candidate_->inner_segment_boundary.size());
  const uint32 encoded_lengths = candidate_->inner_segment_boundary[index_++];
  key_offset_ += encoded_lengths >> 24;
  value_offset_ += (encoded_lengths >> 16) & 0xff;
}

absl::string_view Segment::Candidate::InnerSegmentIterator::GetKey() const {
  DCHECK_LT(index_, candidate_->inner_segment_boundary.size());
  const uint32 encoded_lengths = candidate_->inner_segment_boundary[index_];
  return absl::string_view(key_offset_, encoded_lengths >> 24);
}

absl::string_view Segment::Candidate::InnerSegmentIterator::GetValue() const {
  DCHECK_LT(index_, candidate_->inner_segment_boundary.size());
  const uint32 encoded_lengths = candidate_->inner_segment_boundary[index_];
  return absl::string_view(value_offset_, (encoded_lengths >> 16) & 0xff);
}

absl::string_view Segment::Candidate::InnerSegmentIterator::GetContentKey()
    const {
  DCHECK_LT(index_, candidate_->inner_segment_boundary.size());
  const uint32 encoded_lengths = candidate_->inner_segment_boundary[index_];
  return absl::string_view(key_offset_, (encoded_lengths >> 8) & 0xff);
}

absl::string_view Segment::Candidate::InnerSegmentIterator::GetContentValue()
    const {
  DCHECK_LT(index_, candidate_->inner_segment_boundary.size());
  const uint32 encoded_lengths = candidate_->inner_segment_boundary[index_];
  return absl::string_view(value_offset_, encoded_lengths & 0xff);
}

Segment::Segment()
    : segment_type_(FREE), pool_(new ObjectPool<Candidate>(16)) {}

Segment::~Segment() {}

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

int Segment::indexOf(const Segment::Candidate *candidate) {
  if (candidate == nullptr) {
    return static_cast<int>(candidates_size());
  }

  for (int i = 0; i < static_cast<int>(candidates_.size()); ++i) {
    if (candidates_[i] == candidate) {
      return i;
    }
  }

  for (int i = 0; i < static_cast<int>(meta_candidates_.size()); ++i) {
    if (&(meta_candidates_[i]) == candidate) {
      return -i - 1;
    }
  }

  return static_cast<int>(candidates_size());
}

size_t Segment::candidates_size() const { return candidates_.size(); }

void Segment::clear_candidates() {
  pool_->Free();
  candidates_.clear();
}

Segment::Candidate *Segment::push_back_candidate() {
  Candidate *candidate = pool_->Alloc();
  candidate->Init();
  candidates_.push_back(candidate);
  return candidate;
}

Segment::Candidate *Segment::push_front_candidate() {
  Candidate *candidate = pool_->Alloc();
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
  Candidate *candidate = pool_->Alloc();
  candidate->Init();
  candidates_.insert(candidates_.begin() + i, candidate);
  return candidate;
}

void Segment::pop_front_candidate() {
  if (!candidates_.empty()) {
    Candidate *c = candidates_.front();
    pool_->Release(c);
    candidates_.pop_front();
  }
}

void Segment::pop_back_candidate() {
  if (!candidates_.empty()) {
    Candidate *c = candidates_.back();
    pool_->Release(c);
    candidates_.pop_back();
  }
}

void Segment::erase_candidate(int i) {
  if (i < 0 || i >= static_cast<int>(candidates_size())) {
    LOG(WARNING) << "invalid index";
    return;
  }
  pool_->Release(mutable_candidate(i));
  candidates_.erase(candidates_.begin() + i);
}

void Segment::erase_candidates(int i, size_t size) {
  const size_t end = i + size;
  if (i < 0 || i >= static_cast<int>(candidates_size()) ||
      end > candidates_size()) {
    LOG(WARNING) << "invalid index";
    return;
  }
  for (int j = i; j < static_cast<int>(end); ++j) {
    pool_->Release(mutable_candidate(j));
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

void Segment::CopyFrom(const Segment &src) {
  Clear();

  key_ = src.key();
  segment_type_ = src.segment_type();

  for (size_t i = 0; i < src.candidates_size(); ++i) {
    Candidate *candidate = add_candidate();
    candidate->CopyFrom(src.candidate(i));
  }

  for (size_t i = 0; i < src.meta_candidates_size(); ++i) {
    Candidate *meta_candidate = add_meta_candidate();
    meta_candidate->CopyFrom(src.meta_candidate(i));
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

void Segments::RevertEntry::CopyFrom(const RevertEntry &src) {
  revert_entry_type = src.revert_entry_type;
  id = src.id;
  timestamp = src.timestamp;
  key = src.key;
}

Segments::Segments()
    : max_history_segments_size_(0),
      max_prediction_candidates_size_(0),
      max_conversion_candidates_size_(kMaxConversionCandidatesSize),
      resized_(false),
      user_history_enabled_(true),
      request_type_(Segments::CONVERSION),
      pool_(new ObjectPool<Segment>(32)),
      cached_lattice_(new Lattice()) {}

Segments::~Segments() {}

Segments::RequestType Segments::request_type() const { return request_type_; }

void Segments::set_request_type(Segments::RequestType request_type) {
  request_type_ = request_type;
}

void Segments::set_user_history_enabled(bool user_history_enabled) {
  user_history_enabled_ = user_history_enabled;
}

bool Segments::user_history_enabled() const { return user_history_enabled_; }

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
  Segment *segment = pool_->Alloc();
  segment->Clear();
  segments_.insert(segments_.begin() + i, segment);
  return segment;
}

Segment *Segments::push_back_segment() {
  Segment *segment = pool_->Alloc();
  segment->Clear();
  segments_.push_back(segment);
  return segment;
}

Segment *Segments::push_front_segment() {
  Segment *segment = pool_->Alloc();
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
  pool_->Release(mutable_segment(i));
  segments_.erase(segments_.begin() + i);
}

void Segments::erase_segments(size_t i, size_t size) {
  const size_t end = i + size;
  if (i >= segments_size() || end > segments_size()) {
    return;
  }
  for (size_t j = i; j < end; ++j) {
    pool_->Release(mutable_segment(j));
  }
  segments_.erase(segments_.begin() + i, segments_.begin() + end);
}

void Segments::pop_front_segment() {
  if (!segments_.empty()) {
    Segment *seg = segments_.front();
    pool_->Release(seg);
    segments_.pop_front();
  }
}

void Segments::pop_back_segment() {
  if (!segments_.empty()) {
    Segment *seg = segments_.back();
    pool_->Release(seg);
    segments_.pop_back();
  }
}

void Segments::Clear() {
  clear_segments();
  clear_revert_entries();
}

void Segments::CopyFrom(const Segments &src) {
  Clear();
  max_history_segments_size_ = src.max_history_segments_size();
  max_prediction_candidates_size_ = src.max_prediction_candidates_size();
  max_conversion_candidates_size_ = src.max_conversion_candidates_size();
  resized_ = src.resized();
  user_history_enabled_ = src.user_history_enabled();

  request_type_ = src.request_type();

  for (size_t i = 0; i < src.segments_size(); ++i) {
    Segment *segment = add_segment();
    segment->CopyFrom(src.segment(i));
  }

  for (size_t i = 0; i < src.revert_entries_size(); ++i) {
    RevertEntry *revert_entry = push_back_revert_entry();
    revert_entry->CopyFrom(src.revert_entry(i));
  }
}

void Segments::clear_segments() {
  pool_->Free();
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
    pool_->Release(mutable_segment(i));
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

size_t Segments::max_prediction_candidates_size() const {
  return max_prediction_candidates_size_;
}

void Segments::set_max_prediction_candidates_size(size_t size) {
  max_prediction_candidates_size_ = size;
}

size_t Segments::max_conversion_candidates_size() const {
  return max_conversion_candidates_size_;
}

void Segments::set_max_conversion_candidates_size(size_t size) {
  max_conversion_candidates_size_ = size;
}

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
