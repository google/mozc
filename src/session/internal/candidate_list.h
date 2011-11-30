// Copyright 2010-2011, Google Inc.
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

#include <map>
#include <string>
#include <vector>

#include "base/base.h"


namespace mozc {

template <class T> class ObjectPool;

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
typedef uint32 Attributes;

class Candidate {
 public:
  Candidate();
  virtual ~Candidate();

  void Clear();

  bool IsSubcandidateList() const;
  int id() const;
  void set_id(int id);

  Attributes attributes() const;
  void add_attributes(Attributes attributes);
  void set_attributes(Attributes attributes);
  bool has_attributes(Attributes attributes) const;

  const CandidateList &subcandidate_list() const;
  CandidateList *mutable_subcandidate_list();
  void set_subcandidate_list(CandidateList *subcandidate_list);
  // Allocate a subcandidate list and return it.
  CandidateList *allocate_subcandidate_list(bool rotate);

 private:
  int id_;
  Attributes attributes_;
  // Whether subcandidate_list_ should be released when destructed.
  CandidateList *subcandidate_list_;
  bool subcandidate_list_owner_;

  DISALLOW_COPY_AND_ASSIGN(Candidate);
};

class CandidateList {
 public:
  explicit CandidateList(bool rotate);
  virtual ~CandidateList();

  void Clear();

  const Candidate &GetDeepestFocusedCandidate() const;
  void AddCandidate(int id, const string &value);
  void AddCandidateWithAttributes(int id,
                                  const string &value,
                                  Attributes attributes);
  void AddSubCandidateList(CandidateList *subcandidate_list);
  CandidateList *AllocateSubCandidateList(bool rotate);

  void set_name(const string &name);
  const string &name() const;

  void set_page_size(size_t page_size);
  size_t page_size() const;

  // Accessors
  size_t size() const;
  size_t last_index() const;
  const Candidate &focused_candidate() const;
  const Candidate &candidate(size_t index) const;
  int focused_id() const;
  size_t focused_index() const;
  int next_available_id() const;
  void GetPageRange(size_t index, size_t *cand_begin, size_t *cand_end) const;

  bool focused() const;
  void set_focused(bool focused);

  // Operations
  void MoveFirst();
  void MoveLast();
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

  bool IsFirst(size_t index) const;
  bool IsLast(size_t index) const;
  bool IsFirstPage(size_t index) const;
  bool IsLastPage(size_t index) const;

  bool rotate_;
  size_t page_size_;
  size_t focused_index_;
  bool focused_;
  string name_;
  scoped_ptr<ObjectPool<Candidate> > candidate_pool_;
  scoped_ptr<vector<Candidate *> > candidates_;
  int next_available_id_;

  // Map marking added candidate values.  The keys are fingerprints of
  // the candidate values, the values of the map are candidate ids.
  scoped_ptr<map<uint64, int> > added_candidates_;

  // Id-to-id map.  The key and value ids have the same candidate
  // value.  (ex. {id:0, value:"kanji"} and {id:-5, value:"kanji"}).
  // The key ids are not directly stored in candidates, so accessing
  // these ids, they should be converted with this map.
  scoped_ptr<map<int, int> > alternative_ids_;

  DISALLOW_COPY_AND_ASSIGN(CandidateList);
};

}  // namespace session
}  // namespace mozc
#endif  // MOZC_SESSION_INTERNAL_CANDIDATE_LIST_H_
