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

// CandidateList and Candidate classes to be used with Session class.

#include "engine/candidate_list.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "base/hash.h"

namespace mozc {
namespace engine {

void Candidate::Clear() {
  id_ = 0;
  attributes_ = NO_ATTRIBUTES;
  subcandidate_list_.reset();
}

void CandidateList::Clear() {
  candidates_.clear();
  focused_index_ = 0;
  focused_ = false;
  next_available_id_ = 0;
  added_candidates_.clear();
  alternative_ids_.clear();
}

const Candidate &CandidateList::GetDeepestFocusedCandidate() const {
  if (focused_candidate().HasSubcandidateList()) {
    return focused_candidate().subcandidate_list().GetDeepestFocusedCandidate();
  }
  return focused_candidate();
}

void CandidateList::AddCandidateWithAttributes(const int id,
                                               const absl::string_view value,
                                               const Attributes attributes) {
  if (id >= 0) {
    DCHECK(id >= next_available_id_);
    // If id is not for T13N candidate, update |last_added_id_|.
    next_available_id_ = id + 1;
  }

  // If the value has already been stored in the candidate list, reuse it and
  // update the alternative_ids_.
  const uint64_t fp = Fingerprint(value);

  if (const auto [iter, inserted] = added_candidates_.emplace(fp, id);
      !inserted) {  // insertion was failed.
    const int alt_id = iter->second;
    alternative_ids_[id] = alt_id;

    // Add attributes to the existing candidate.
    for (size_t i = 0; i < size(); ++i) {
      if (candidate(i).id() == alt_id) {
        candidates_[i].add_attributes(attributes);
      }
    }
    return;
  }

  Candidate &new_candidate = candidates_.emplace_back();

  new_candidate.set_id(id);
  new_candidate.set_attributes(attributes);
}

CandidateList *CandidateList::AddSubCandidateList() {
  return candidates_.emplace_back().mutable_subcandidate_list();
}

int CandidateList::focused_id() const {
  // If the list does not have any candidate, 0 will be returned.
  if (size() == 0) {
    return 0;
  }
  if (focused_candidate().HasSubcandidateList()) {
    return focused_candidate().subcandidate_list().focused_id();
  }
  return focused_candidate().id();
}

int CandidateList::next_available_id() const {
  int result = next_available_id_;
  for (size_t i = 0; i < candidates_.size(); ++i) {
    if (candidate(i).HasSubcandidateList()) {
      const int sub_available_id =
          candidate(i).subcandidate_list().next_available_id();
      if (result < sub_available_id) {
        result = sub_available_id;
      }
    }
  }
  return result;
}

bool CandidateList::MoveNext() {
  // If the current candidate points to subcandidate list, the focused
  // candidate in the subcandidate list will be operated.
  if (focused_candidate().HasSubcandidateList() &&
      mutable_focused_subcandidate_list()->MoveNext()) {
    return true;
  }

  if (IsLast(focused_index_)) {
    MoveFirst();
    if (!rotate_) {
      // If this candidate list does not rotate, the focused candidate
      // is moved to the parent candidate list.
      return false;
    }
  } else {
    ++focused_index_;
  }

  // If the new current candidate points to subcandidate list, the
  // focused candidate in the subcandidate list should be the first
  // candidate.
  if (focused_candidate().HasSubcandidateList()) {
    mutable_focused_subcandidate_list()->MoveFirst();
  }
  return true;
}

bool CandidateList::MovePrev() {
  // If the current candidate points to subcandidate list, the focused
  // candidate in the subcandidate list will be operated.
  if (focused_candidate().HasSubcandidateList() &&
      mutable_focused_subcandidate_list()->MovePrev()) {
    return true;
  }

  if (IsFirst(focused_index_)) {
    MoveLast();
    if (!rotate_) {
      // If this candidate list does not rotate, the focused candidate
      // is moved to the parent candidate list.
      return false;
    }
  } else {
    --focused_index_;
  }

  // If the new current candidate points to subcandidate list, the
  // focused candidate in the subcandidate list should be the first
  // candidate.
  if (focused_candidate().HasSubcandidateList()) {
    mutable_focused_subcandidate_list()->MoveLast();
  }
  return true;
}

bool CandidateList::MoveNextPage() {
  if (focused_candidate().HasSubcandidateList() &&
      mutable_focused_subcandidate_list()->MoveNextPage()) {
    return true;
  }

  if (IsLastPage(focused_index_)) {
    if (!rotate_) {
      // If the current candidate is the last candidate and rotation
      // should not be performed, this function does nothing and
      // returns false.
      return false;
    }
    MoveFirst();
  } else {
    focused_index_ += page_size_;
  }
  // Move the forcused index to the beginning of the page.
  focused_index_ -= (focused_index_ % page_size_);

  // If the new current candidate points to subcandidate list, the
  // focused candidate in the subcandidate list should be the first
  // candidate.
  if (focused_candidate().HasSubcandidateList()) {
    mutable_focused_subcandidate_list()->MoveFirst();
  }
  return true;
}

bool CandidateList::MovePrevPage() {
  if (focused_candidate().HasSubcandidateList() &&
      mutable_focused_subcandidate_list()->MovePrevPage()) {
    return true;
  }

  if (IsFirstPage(focused_index_)) {
    if (!rotate_) {
      // If the current candidate is on the first page and rotation
      // should not be performed, this function does nothing and
      // returns false.
      return false;
    }
    MoveLast();
  } else {
    // The focused index in not on the first page, so the value of
    // (focused_index_ - page_size_) must be a positive value.
    focused_index_ -= page_size_;
  }
  // Move the forcused index to the beginning of the page.
  focused_index_ -= (focused_index_ % page_size_);

  // If the new current candidate points to subcandidate list, the
  // focused candidate in the subcandidate list should be the first
  // candidate.  Note the forcused index will be moved to the
  // beginning of the page on page moves.
  if (focused_candidate().HasSubcandidateList()) {
    mutable_focused_subcandidate_list()->MoveFirst();
  }
  return true;
}

bool CandidateList::MoveNextAttributes(const Attributes attributes) {
  // Move one candidate.
  MoveNext();
  return MoveToAttributes(attributes);
}

bool CandidateList::MoveToAttributes(const Attributes attributes) {
  const size_t cand_size = size();
  for (size_t i = 0; i < cand_size; ++i) {
    // Shift the index to make the first index focused_index_.
    const size_t index = (focused_index_ + i) % cand_size;

    Candidate &cand = candidates_[index];

    // If the candidate is a subcandidate list, the subcandidate list is
    // traversed recursively.
    if (cand.HasSubcandidateList() &&
        cand.mutable_subcandidate_list()->MoveToAttributes(attributes)) {
      focused_index_ = index;
      return true;
    } else if (cand.has_attributes(attributes)) {
      focused_index_ = index;
      return true;
    }
  }

  return false;
}

bool CandidateList::MoveToId(const int base_id) {
  int id = base_id;

  // If an alternative id for the base_id found, use it to avoid
  // duplicated candidates.
  if (const auto iter = alternative_ids_.find(id);
      iter != alternative_ids_.end()) {
    id = iter->second;
  }

  // NOTE(komatsu): Although this function's order is O(N), The size
  // of N is less than kMaxCandidateSize (= 200).  So it would not be
  // a problem.
  for (size_t i = 0; i < size(); ++i) {
    Candidate &cand = candidates_[i];
    // If the candidate is a subcandidate list, the subcandidate list is
    // traversed recursively.
    if (cand.HasSubcandidateList() &&
        cand.mutable_subcandidate_list()->MoveToId(id)) {
      focused_index_ = i;
      return true;
    } else if (cand.id() == id) {
      focused_index_ = i;
      return true;
    }
  }
  return false;
}

bool CandidateList::MoveToPageIndex(const size_t page_index) {
  const auto [begin, end] = GetPageRange(focused_index_);
  if (begin + page_index > end) {
    return false;
  }
  focused_index_ = begin + page_index;
  if (focused_candidate().HasSubcandidateList()) {
    mutable_focused_subcandidate_list()->MoveFirst();
  }
  return true;
}

CandidateList *CandidateList::mutable_focused_subcandidate_list() {
  CHECK(focused_candidate().HasSubcandidateList());
  return candidates_[focused_index_].mutable_subcandidate_list();
}

}  // namespace engine
}  // namespace mozc
