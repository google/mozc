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

#ifndef MOZC_SESSION_INTERNAL_CANDIDATE_LIST_H_
#define MOZC_SESSION_INTERNAL_CANDIDATE_LIST_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "base/container/freelist.h"

namespace mozc {
namespace session {

class CandidateList;  // This is fully declared at the bottom.

// Attribute is added to candidates for annotationg additional
// information to the candidates.  This is used for toggleing ASCII
// transliterations at this moment.  Multiple attributes can be used
// to one candidates.
enum Attribute {
  NO_ATTRIBUTES = 0,
  HALF_WIDTH = 1,
  FULL_WIDTH = 2,
  ASCII = 4,
  HIRAGANA = 8,
  KATAKANA = 16,
  UPPER = 32,
  LOWER = 64,
  CAPITALIZED = 128,
};

using Attributes = uint32_t;

class Candidate final {
 public:
  Candidate() = default;
  ~Candidate() = default;

  Candidate(const Candidate &) = delete;
  Candidate &operator=(const Candidate &) = delete;

  void Clear();

  bool IsSubcandidateList() const { return subcandidate_list_ != nullptr; }

  int id() const { return id_; }
  void set_id(int id) { id_ = id; }

  Attributes attributes() const { return attributes_; }
  void add_attributes(Attributes attributes) { attributes_ |= attributes; }
  void set_attributes(Attributes attributes) { attributes_ = attributes; }
  bool has_attributes(Attributes attributes) const {
    return (attributes_ & attributes) == attributes;
  }

  const CandidateList &subcandidate_list() const;
  CandidateList *mutable_subcandidate_list() { return subcandidate_list_; }
  void set_subcandidate_list(CandidateList *subcandidate_list);
  // Allocate a subcandidate list and return it.
  CandidateList *allocate_subcandidate_list(bool rotate);

 private:
  int id_ = 0;
  Attributes attributes_ = NO_ATTRIBUTES;
  // Whether subcandidate_list_ should be released when destructed.
  CandidateList *subcandidate_list_ = nullptr;
  bool subcandidate_list_owner_ = false;
};

class CandidateList final {
 public:
  explicit CandidateList(bool rotate);
  ~CandidateList();

  CandidateList(const CandidateList &) = delete;
  CandidateList &operator=(const CandidateList &) = delete;

  void Clear();

  const Candidate &GetDeepestFocusedCandidate() const;
  void AddCandidate(int id, const std::string &value);
  void AddCandidateWithAttributes(int id, const std::string &value,
                                  Attributes attributes);
  void AddSubCandidateList(CandidateList *subcandidate_list);
  CandidateList *AllocateSubCandidateList(bool rotate);

  void set_name(const std::string &name) { name_ = name; }
  const std::string &name() const { return name_; }

  void set_page_size(size_t page_size) { page_size_ = page_size; }
  size_t page_size() const { return page_size_; }

  // Accessors
  size_t size() const { return candidates_.size(); }
  size_t last_index() const { return size() - 1; }
  const Candidate &candidate(size_t index) const { return *candidates_[index]; }
  const Candidate &focused_candidate() const {
    return candidate(focused_index_);
  }
  int focused_id() const;
  size_t focused_index() const { return focused_index_; }
  int next_available_id() const;
  void GetPageRange(size_t index, size_t *page_begin, size_t *page_end) const;

  bool focused() const { return focused_; }
  void set_focused(bool focused) { focused_ = focused; }

  // Operations
  void MoveFirst() { focused_index_ = 0; }
  void MoveLast() { focused_index_ = last_index(); }
  bool MoveNext();
  bool MovePrev();
  bool MoveNextPage();
  bool MovePrevPage();
  bool MoveNextAttributes(Attributes attributes);
  bool MoveToAttributes(Attributes attributes);
  bool MoveToId(int id);
  // Move the focus to the index from the beginning of the current
  // page.  This is a function for shortcut key operation.
  bool MoveToPageIndex(size_t index);

 private:
  CandidateList *mutable_focused_subcandidate_list();

  static bool IsFirst(size_t index) { return index == 0; }
  bool IsLast(size_t index) const { return index == size() - 1; }
  bool IsFirstPage(size_t index) const { return index < page_size_; }
  bool IsLastPage(size_t index) const;

  size_t page_size_;
  size_t focused_index_;
  std::string name_;
  ObjectPool<Candidate> candidate_pool_;
  std::vector<Candidate *> candidates_;

  // Map marking added candidate values.  The keys are fingerprints of
  // the candidate values, the values of the map are candidate ids.
  std::map<uint64_t, int> added_candidates_;

  // Id-to-id map.  The key and value ids have the same candidate
  // value.  (ex. {id:0, value:"kanji"} and {id:-5, value:"kanji"}).
  // The key ids are not directly stored in candidates, so accessing
  // these ids, they should be converted with this map.
  std::map<int, int> alternative_ids_;

  int next_available_id_;
  bool rotate_;
  bool focused_;
};

}  // namespace session
}  // namespace mozc

#endif  // MOZC_SESSION_INTERNAL_CANDIDATE_LIST_H_
