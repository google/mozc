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

// Class functions to be used for output by the Session class.

#include "session/internal/session_output.h"

#include "base/text_normalizer.h"
#include "base/util.h"
#include "base/version.h"
#include "converter/segments.h"
#include "composer/composer.h"
#include "session/internal/candidate_list.h"

namespace mozc {
namespace session {

namespace {
bool FillAnnotation(const Segment::Candidate &candidate_value,
                           commands::Annotation *annotation) {
  bool is_modified = false;
  if (!candidate_value.prefix.empty()) {
    annotation->set_prefix(candidate_value.prefix);
    is_modified = true;
  }
  if (!candidate_value.suffix.empty()) {
    annotation->set_suffix(candidate_value.suffix);
    is_modified = true;
  }
  if (!candidate_value.description.empty()) {
    annotation->set_description(
        candidate_value.description);
    is_modified = true;
  }
  return is_modified;
}

void FillAllCandidateWordsInternal(
    const Segment &segment,
    const CandidateList &candidate_list,
    const int focused_id,
    commands::CandidateList *candidate_list_proto) {
  for (size_t i = 0; i < candidate_list.size(); ++i) {
    const Candidate &candidate = candidate_list.candidate(i);
    if (candidate.IsSubcandidateList()) {
      FillAllCandidateWordsInternal(
          segment, candidate.subcandidate_list(), focused_id,
          candidate_list_proto);
      continue;
    }

    commands::CandidateWord *candidate_word_proto =
      candidate_list_proto->add_candidates();
    // id
    const int id = candidate.id();
    candidate_word_proto->set_id(id);

    // index
    const int index = candidate_list_proto->candidates_size() - 1;
    candidate_word_proto->set_index(index);

    // check focused id
    if (id == focused_id && candidate_list.focused()) {
      candidate_list_proto->set_focused_index(index);
    }

    const Segment::Candidate &segment_candidate = segment.candidate(id);
    // key
    if (segment.key() != segment_candidate.content_key) {
      candidate_word_proto->set_key(segment_candidate.content_key);
    }
    // value
    candidate_word_proto->set_value(segment_candidate.value);

    // annotations
    commands::Annotation annotation;
    if (FillAnnotation(segment_candidate, &annotation)) {
      candidate_word_proto->mutable_annotation()->CopyFrom(annotation);
    }
  }
}
}  // namespace

// static
void SessionOutput::FillCandidate(
    const Segment &segment,
    const Candidate &candidate,
    commands::Candidates_Candidate *candidate_proto) {
  if (candidate.IsSubcandidateList()) {
    candidate_proto->set_value(candidate.subcandidate_list().name());
    candidate_proto->set_id(candidate.subcandidate_list().focused_id());
    return;
  }

  const Segment::Candidate &candidate_value = segment.candidate(candidate.id());
  candidate_proto->set_value(candidate_value.value);

  candidate_proto->set_id(candidate.id());
  // Set annotations
  commands::Annotation annotation;
  if (FillAnnotation(candidate_value, &annotation)) {
    candidate_proto->mutable_annotation()->CopyFrom(annotation);
  }

  if (!candidate_value.usage_title.empty()) {
    candidate_proto->set_information_id(candidate_value.usage_id);
  }
}

// static
void SessionOutput::FillCandidates(const Segment &segment,
                                   const CandidateList &candidate_list,
                                   const size_t position,
                                   commands::Candidates *candidates_proto) {
  if (candidate_list.focused()) {
    candidates_proto->set_focused_index(candidate_list.focused_index());
  }
  candidates_proto->set_size(candidate_list.size());
  candidates_proto->set_position(position);

  size_t c_begin = 0;
  size_t c_end = 0;
  candidate_list.GetPageRange(candidate_list.focused_index(),
                              &c_begin, &c_end);

  // Store candidates.
  for (size_t i = c_begin; i <= c_end; ++i) {
    commands::Candidates_Candidate *candidate_proto =
      candidates_proto->add_candidate();
    candidate_proto->set_index(i);
    FillCandidate(segment, candidate_list.candidate(i), candidate_proto);
  }

  // Store subcandidates.
  if (candidate_list.focused_candidate().IsSubcandidateList()) {
    FillCandidates(segment,
                   candidate_list.focused_candidate().subcandidate_list(),
                   candidate_list.focused_index(),
                   candidates_proto->mutable_subcandidates());
  }

  // Store usages.
  FillUsages(segment, candidate_list, candidates_proto);
}

// static
void SessionOutput::FillAllCandidateWords(
    const Segment &segment,
    const CandidateList &candidate_list,
    const commands::Category category,
    commands::CandidateList *candidate_list_proto) {
  candidate_list_proto->set_category(category);
  FillAllCandidateWordsInternal(
      segment, candidate_list, candidate_list.focused_id(),
      candidate_list_proto);
}



// static
bool SessionOutput::ShouldShowUsages(const Segment &segment,
                                     const CandidateList &cand_list) {
  // Check if the shown candidate have the usage data.
  size_t c_begin = 0;
  size_t c_end = 0;
  cand_list.GetPageRange(cand_list.focused_index(), &c_begin, &c_end);
  for (size_t i = c_begin; i <= c_end; ++i) {
    if (cand_list.candidate(i).IsSubcandidateList()) {
      continue;
    }
    const Segment::Candidate &candidate =
      segment.candidate(cand_list.candidate(i).id());
    if (candidate.usage_title.empty()) {
      continue;
    }
    return true;
  }
  return false;
}


// static
void SessionOutput::FillUsages(const Segment &segment,
                               const CandidateList &cand_list,
                               commands::Candidates *candidates_proto) {
  if (!ShouldShowUsages(segment, cand_list)) {
    return;
  }

  commands::InformationList *usages = candidates_proto->mutable_usages();

  size_t c_begin = 0;
  size_t c_end = 0;
  cand_list.GetPageRange(cand_list.focused_index(), &c_begin, &c_end);

  typedef pair<int32, commands::Information *> IndexInfoPair;
  map<int32, IndexInfoPair> usageid_information_map;
  // Store usages.
  for (size_t i = c_begin; i <= c_end; ++i) {
    if (cand_list.candidate(i).IsSubcandidateList()) {
      continue;
    }
    const Segment::Candidate &candidate =
      segment.candidate(cand_list.candidate(i).id());
    if (candidate.usage_title.empty()) {
      continue;
    }

    int index;
    commands::Information *info;
    map<int32, IndexInfoPair>::iterator info_itr =
      usageid_information_map.find(candidate.usage_id);

    if (info_itr == usageid_information_map.end()) {
      index = usages->information_size();
      info = usages->add_information();
      info->set_id(candidate.usage_id);
      info->set_title(candidate.usage_title);
      info->set_description(candidate.usage_description);
      info->add_candidate_id(cand_list.candidate(i).id());
      usageid_information_map.insert(
          make_pair(candidate.usage_id, make_pair(index, info)));
    } else {
      index = info_itr->second.first;
      info = info_itr->second.second;
      info->add_candidate_id(cand_list.candidate(i).id());
    }
    if (cand_list.candidate(i).id() == cand_list.focused_id()) {
      usages->set_focused_index(index);
    }
  }
}


// static
void SessionOutput::FillShortcuts(const string &shortcuts,
                                  commands::Candidates *candidates_proto) {
  const size_t num_loop = min(
      static_cast<size_t>(candidates_proto->candidate_size()),
      shortcuts.size());
  for (size_t i = 0; i < num_loop; ++i) {
    const string shortcut = shortcuts.substr(i, 1);
    candidates_proto->mutable_candidate(i)->mutable_annotation()->
      set_shortcut(shortcut);
  }
}

// static
void SessionOutput::FillSubLabel(commands::Footer *footer) {
  // Delete the label because sub_label will be drawn on the same
  // place for the label.
  footer->clear_label();

  string sub_label("build ");

  // Append third number of the version to sub_label.
  const string version = Version::GetMozcVersion();
  vector<string> version_numbers;
  Util::SplitStringUsing(version, ".", &version_numbers);
  if (version_numbers.size() > 2) {
    sub_label.append(version_numbers[2]);
    footer->set_sub_label(sub_label);
  } else {
    LOG(ERROR) << "Unkonwn version format: " << version;
  }
}

// static
bool SessionOutput::FillFooter(const commands::Category category,
                               commands::Candidates *candidates) {
  if (category != commands::SUGGESTION &&
      category != commands::PREDICTION &&
      category != commands::CONVERSION) {
    return false;
  }

  commands::Footer *footer = candidates->mutable_footer();
  if (category == commands::SUGGESTION) {
    // TODO(komatsu): Enable to localized the message.
    // "Tabキーで選択"
    const char kLabel[] = ("Tab\xE3\x82\xAD\xE3\x83\xBC\xE3\x81\xA7"
                           "\xE9\x81\xB8\xE6\x8A\x9E");
    // TODO(komatsu): Need to check if Tab is not changed to other key binding.
    footer->set_label(kLabel);
  } else {
    // category is commands::PREDICTION or commands::CONVERSION.
    footer->set_index_visible(true);
    footer->set_logo_visible(true);
  }

#ifdef CHANNEL_DEV
  FillSubLabel(footer);
#endif  // CHANNEL_DEV

  return true;
}

// static
bool SessionOutput::AddSegment(const string &key,
                               const string &value,
                               const uint32 segment_type_mask,
                               commands::Preedit *preedit) {
  // Key is always normalized as a preedit text.
  string normalized_key;
  TextNormalizer::NormalizePreeditText(key, &normalized_key);

  string normalized_value;
  if (segment_type_mask & PREEDIT) {
    TextNormalizer::NormalizePreeditText(value, &normalized_value);
  } else if (segment_type_mask & CONVERSION) {
    normalized_value = value;
  } else {
    LOG(WARNING) << "Unknown segment type" << segment_type_mask;
    normalized_value = value;
  }

  if (normalized_value.empty()) {
    return false;
  }

  commands::Preedit::Segment *segment = preedit->add_segment();
  segment->set_key(normalized_key);
  segment->set_value(normalized_value);
  segment->set_value_length(Util::CharsLen(normalized_value));
  segment->set_annotation(commands::Preedit::Segment::UNDERLINE);
  if ((segment_type_mask & CONVERSION) && (segment_type_mask & FOCUSED)) {
    segment->set_annotation(commands::Preedit::Segment::HIGHLIGHT);
  } else {
    segment->set_annotation(commands::Preedit::Segment::UNDERLINE);
  }
  return true;
}

// static
void SessionOutput::FillPreedit(const composer::Composer &composer,
                                commands::Preedit *preedit) {
  string output;
  composer.GetStringForPreedit(&output);

  const uint32 kBaseType = PREEDIT;
  AddSegment(output, output, kBaseType, preedit);
  preedit->set_cursor(static_cast<uint32>(composer.GetCursor()));
}

// static
void SessionOutput::FillConversion(const Segments &segments,
                                   const size_t segment_index,
                                   const int candidate_id,
                                   commands::Preedit *preedit) {
  const uint32 kBaseType = CONVERSION;
  // Cursor position in conversion state should be the end of the preedit.
  size_t cursor = 0;
  for (size_t i = 0; i < segments.conversion_segments_size(); ++i) {
    const Segment &segment = segments.conversion_segment(i);
    if (i == segment_index) {
      const string &value = segment.candidate(candidate_id).value;
      if (AddSegment(segment.key(), value, kBaseType | FOCUSED, preedit) &&
          (!preedit->has_highlighted_position())) {
        preedit->set_highlighted_position(cursor);
      }
      cursor += Util::CharsLen(value);
    } else {
      const string &value = segment.candidate(0).value;
      AddSegment(segment.key(), value, kBaseType, preedit);
      cursor += Util::CharsLen(value);
    }
  }
  preedit->set_cursor(cursor);
}

// static
void SessionOutput::FillConversionResult(const string &key,
                                         const string &result,
                                         commands::Result *result_proto) {
  result_proto->set_type(commands::Result::STRING);

  // Key should be normalized as a preedit text.
  string normalized_key;
  TextNormalizer::NormalizePreeditText(key, &normalized_key);
  result_proto->set_key(normalized_key);

  // value is already normalized by converter.
  result_proto->set_value(result);
}

// static
void SessionOutput::FillPreeditResult(const string &preedit,
                                      commands::Result *result_proto) {
  result_proto->set_type(commands::Result::STRING);


  string normalized_preedit;
  TextNormalizer::NormalizePreeditText(preedit, &normalized_preedit);
  result_proto->set_key(normalized_preedit);
  result_proto->set_value(normalized_preedit);
}

}  // namespace session
}  // namespace mozc
