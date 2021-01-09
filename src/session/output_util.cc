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

#include "session/output_util.h"

#include "base/logging.h"
#include "base/port.h"
#include "protocol/commands.pb.h"

namespace mozc {

bool OutputUtil::GetCandidateIndexById(const commands::Output &output,
                                       int32 mozc_candidate_id,
                                       int32 *candidate_index) {
  if (candidate_index == nullptr) {
    return false;
  }
  if (!output.has_all_candidate_words()) {
    return false;
  }
  const commands::CandidateList &list = output.all_candidate_words();
  for (size_t i = 0; i < list.candidates_size(); ++i) {
    const commands::CandidateWord &word = list.candidates(i);
    if (!word.has_id() || !word.has_index()) {
      // cannot specify this id.
      continue;
    }
    if (word.id() == mozc_candidate_id) {
      DCHECK(nullptr != candidate_index);
      *candidate_index = word.index();
      return true;
    }
  }
  return false;
}

bool OutputUtil::GetCandidateIdByIndex(const commands::Output &output,
                                       int32 candidate_index,
                                       int32 *mozc_candidate_id) {
  if (mozc_candidate_id == nullptr) {
    return false;
  }
  if (!output.has_all_candidate_words()) {
    return false;
  }
  const commands::CandidateList &list = output.all_candidate_words();
  for (size_t i = 0; i < list.candidates_size(); ++i) {
    const commands::CandidateWord &word = list.candidates(i);
    if (!word.has_id() || !word.has_index()) {
      // cannot specify this id.
      continue;
    }
    if (word.index() == candidate_index) {
      DCHECK(nullptr != mozc_candidate_id);
      *mozc_candidate_id = word.id();
      return true;
    }
  }
  return false;
}

bool OutputUtil::GetFocusedCandidateId(const commands::Output &output,
                                       int32 *mozc_candidate_id) {
  if (mozc_candidate_id == nullptr) {
    return false;
  }
  if (!output.has_all_candidate_words()) {
    return false;
  }
  const commands::CandidateList &list = output.all_candidate_words();
  if (!list.has_focused_index()) {
    return false;
  }
  const int32 focused_index = list.focused_index();
  for (size_t i = 0; i < list.candidates_size(); ++i) {
    const commands::CandidateWord &word = list.candidates(i);
    if (!word.has_id() || !word.has_index()) {
      // cannot specify this id.
      continue;
    }
    if (word.index() == focused_index) {
      DCHECK(nullptr != mozc_candidate_id);
      *mozc_candidate_id = word.id();
      return true;
    }
  }
  return false;
}

}  // namespace mozc
