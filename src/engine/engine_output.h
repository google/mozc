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

// Class functions to be used for output by the Session class.

#ifndef MOZC_ENGINE_SESSION_OUTPUT_H_
#define MOZC_ENGINE_SESSION_OUTPUT_H_

#include <cstddef>
#include <cstdint>
#include <string>

#include "absl/strings/string_view.h"
#include "composer/composer.h"
#include "converter/segments.h"
#include "engine/candidate_list.h"
#include "protocol/candidate_window.pb.h"
#include "protocol/commands.pb.h"

namespace mozc {
namespace engine {
namespace output {

// Fill the CandidateWindow_Candidate protobuf with the contents of candidate.
void FillCandidate(const Segment &segment, const Candidate &candidate,
                   commands::CandidateWindow_Candidate *candidate_proto);

// Fill the CandidateWindow protobuf with the contents of candidate_list.
void FillCandidateWindow(const Segment &segment,
                         const CandidateList &candidate_list, size_t position,
                         commands::CandidateWindow *candidate_window_proto);

// Fill the CandidateList protobuf with the contents of
// candidate_list.  Candidates in the candidate_list are flatten
// even if the candidate_list contains sub-candidate lists.
void FillAllCandidateWords(const Segment &segment,
                           const CandidateList &candidate_list,
                           commands::Category category,
                           commands::CandidateList *candidate_list_proto);

// For debug. Fill the CandidateList protobuf with the
// removed_candidates_for_debug in the segment.
void FillRemovedCandidates(const Segment &segment,
                           commands::CandidateList *candidate_list_proto);

// Check if the usages should be rendered on the current CandidateList status.
bool ShouldShowUsages(const Segment &segment, const CandidateList &cand_list);

// Fill the usages of Candidates protobuf with the contents of candidate_list.
void FillUsages(const Segment &segment, const CandidateList &cand_list,
                commands::CandidateWindow *candidate_window_proto);

// Fill the access key of Candidates protobuf with the sequence of shortcuts.
void FillShortcuts(absl::string_view shortcuts,
                   commands::CandidateWindow *candidate_window_proto);

// Fill the sub_label of footer_proto.  This function should be
// called on dev_channel and unittest.
void FillSubLabel(commands::Footer *footer_proto);

// Fill the footer contents of Candidates protobuf.  If category is
// modified, true is returned.  Otherwise false is returned.
bool FillFooter(commands::Category category,
                commands::CandidateWindow *candidate_window_proto);

// Fill the Preedit protobuf with the contents of composer as a preedit.
void FillPreedit(const composer::Composer &composer,
                 commands::Preedit *preedit);

// Fill the Preedit protobuf with the contents of segments as a conversion.
void FillConversion(const Segments &segments, size_t segment_index,
                    int candidate_id, commands::Preedit *preedit);

enum SegmentType {
  PREEDIT = 1,
  CONVERSION = 2,
  FOCUSED = 4,
};

// Add a Preedit::Segment protobuf to the Preedit protobuf with key
// and value.  Return true iff. new segment is added to preedit.
bool AddSegment(absl::string_view, absl::string_view value,
                uint32_t segment_type_mask, commands::Preedit *preedit);

// Fill the Result protobuf with the key and result strings
// for a conversion result without any text normalization.
void FillConversionResultWithoutNormalization(std::string key,
                                              std::string result,
                                              commands::Result *result_proto);

// Fill the Result protobuf with the key and result strings
// normalizing the string for a conversion result.
void FillConversionResult(absl::string_view key, std::string result,
                          commands::Result *result_proto);

// Fill the Result protobuf with the preedit string normalizing the
// string for a preedit result.
void FillPreeditResult(absl::string_view preedit,
                       commands::Result *result_proto);

// Fill the Result protobuf with cursor offset.
void FillCursorOffsetResult(int32_t cursor_offset,
                            commands::Result *result_proto);

}  // namespace output
}  // namespace engine
}  // namespace mozc

#endif  // MOZC_ENGINE_SESSION_OUTPUT_H_
