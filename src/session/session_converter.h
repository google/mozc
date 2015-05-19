// Copyright 2010-2014, Google Inc.
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

// A class handling the converter on the session layer.

#ifndef MOZC_SESSION_SESSION_CONVERTER_H_
#define MOZC_SESSION_SESSION_CONVERTER_H_

#include <string>
#include <vector>

#include "base/port.h"
#include "base/scoped_ptr.h"
#include "session/session_converter_interface.h"

namespace mozc {
namespace commands {
class CandidateList;
class Candidates;
class Context;
class Output;
class Preedit;
class Request;
class Result;
}  // namespace commands

namespace config {
class Config;
}  // namespace config

namespace session {
class CandidateList;

// Class handling ConverterInterface with a session state.  This class
// support stateful operations related with the converter.
class SessionConverter : public SessionConverterInterface {
 public:
  SessionConverter(const ConverterInterface *converter,
                   const commands::Request *request);
  virtual ~SessionConverter();

  // Updates OperationPreferences.
  virtual void SetOperationPreferences(const OperationPreferences &preferences);

  // Checks if the current state is in the state bitmap.
  virtual bool CheckState(States) const;

  // Indicates if the conversion session is active or not.  In general,
  // Convert functions make it active and Cancel, Reset and Commit
  // functions make it deactive.
  virtual bool IsActive() const;

  // Returns the default conversion preferences to be used for custom
  // conversion.
  virtual const ConversionPreferences &conversion_preferences() const;

  // Gets the selected candidate. If no candidate is selected, returns NULL.
  virtual const Segment::Candidate *
  GetSelectedCandidateOfFocusedSegment() const;

  // Sends a conversion request to the converter.
  virtual bool Convert(const composer::Composer &composer);
  virtual bool ConvertWithPreferences(const composer::Composer &composer,
                                      const ConversionPreferences &preferences);

  // Gets reading text (e.g. from "猫" to "ねこ").
  virtual bool GetReadingText(const string &source_text, string *reading);

  // Sends a transliteration request to the converter.
  virtual bool ConvertToTransliteration(
      const composer::Composer &composer,
      transliteration::TransliterationType type);

  // Converts the current composition to half-width characters.
  // NOTE(komatsu): This function might be merged to ConvertToTransliteration.
  virtual bool ConvertToHalfWidth(const composer::Composer &composer);

  // Switches the composition to Hiragana, full-width Katakana or
  // half-width Katakana by rotation.
  virtual bool SwitchKanaType(const composer::Composer &composer);

  // Sends a suggestion request to the converter.
  virtual bool Suggest(const composer::Composer &composer);
  virtual bool SuggestWithPreferences(const composer::Composer &composer,
                                      const ConversionPreferences &preferences);

  // Sends a prediction request to the converter.
  virtual bool Predict(const composer::Composer &composer);
  virtual bool PredictWithPreferences(const composer::Composer &composer,
                                      const ConversionPreferences &preferences);

  // Sends a prediction request to the converter.
  // The result is added at the tail of existing candidate list as "suggestion"
  // candidates.
  virtual bool ExpandSuggestion(const composer::Composer &composer);
  virtual bool ExpandSuggestionWithPreferences(
      const composer::Composer &composer,
      const ConversionPreferences &preferences);

  // Clears conversion segments, but keep the context.
  virtual void Cancel();

  // Clears conversion segments and the context.
  virtual void Reset();

  // Fixes the conversion with the current status.
  virtual void Commit(const composer::Composer &composer,
                      const commands::Context &context);

  // Fixes the suggestion candidate. Stores the number of characters in the key
  // of the committed candidate to committed_key_size.
  // For example, assume that "日本語" was suggested as a candidate for "にほ".
  // The key of the candidate is "にほんご". Therefore, when the candidate is
  // committed by this function, consumed_key_size will be set to 4.
  // |consumed_key_size| can be very large value
  // (i.e. SessionConverter::kConsumedAllCharacters).
  // In this case all the composition characters are consumed.
  // Examples:
  // - {composition: "にほんご|", value: "日本語", consumed_key_size: 4}
  //   This is usual case of suggestion/composition.
  // - {composition: "にほんg|", value: "日本語",
  //    consumed_key_size: kConsumedAllCharacters}
  //   kConsumedAllCharacters can be very large value.
  // - {composition: "わた|しのなまえ", value: "綿",
  //    consumed_key_size: 2}
  //   (Non-Auto) Partial suggestion.
  //   New composition is "しのなまえ".
  // - {composition: "わたしのなまえ|", value: "私の",
  //    consumed_key_size: 4}
  //   Auto partial suggestion.
  //   Consumed the composition partially and "なまえ" becomes composition.
  // - {composition: "じゅえり", value: "クエリ",
  //    consumed_key_size: 4}
  //   Typing correction.
  //   The value クエリ corresponds to raw composition "じゅえり".
  // True is returned if the suggestion is successfully committed.
  virtual bool CommitSuggestionByIndex(size_t index,
                                       const composer::Composer &composer,
                                       const commands::Context &context,
                                       size_t *consumed_key_size);

  // Selects a candidate and commit the selected candidate.  True is
  // returned if the suggestion is successfully committed.
  // c.f. CommitSuggestionInternal
  virtual bool CommitSuggestionById(int id,
                                    const composer::Composer &composer,
                                    const commands::Context &context,
                                    size_t *consumed_key_size);

  // Fixes only the conversion of the first segment, and keep the rest.
  // The caller should delete characters from composer based on returned
  // |committed_key_size|.
  // If |consumed_key_size| is 0, this means that there is only a segment
  // so Commit() method is called instead. In this case, the caller
  // should not delete any characters.
  // c.f. CommitSuggestionInternal
  virtual void CommitFirstSegment(const composer::Composer &composer,
                                  const commands::Context &context,
                                  size_t *consumed_key_size);

  // Commits the preedit string represented by Composer.
  virtual void CommitPreedit(const composer::Composer &composer,
                             const commands::Context &context);

  // Commits the specified number of characters at the head of the preedit
  // string represented by Composer.
  // The caller should delete characters from composer based on returned
  // |consumed_key_size|.
  // c.f. CommitSuggestionInternal
  // TODO(yamaguchi): Enhance to support the conversion mode.
  virtual void CommitHead(size_t count,
                          const composer::Composer &composer,
                          size_t *consumed_key_size);

  // Reverts the last "Commit" operation
  virtual void Revert();

  // Moves the focus of segments.
  virtual void SegmentFocusRight();
  virtual void SegmentFocusLast();
  virtual void SegmentFocusLeft();
  virtual void SegmentFocusLeftEdge();

  // Resizes the focused segment.
  virtual void SegmentWidthExpand(const composer::Composer &composer);
  virtual void SegmentWidthShrink(const composer::Composer &composer);

  // Moves the focus of candidates.
  virtual void CandidateNext(const composer::Composer &composer);
  virtual void CandidateNextPage();
  virtual void CandidatePrev();
  virtual void CandidatePrevPage();
  // Moves the focus to the candidate represented by the id.
  virtual void CandidateMoveToId(int id, const composer::Composer &composer);
  // Moves the focus to the index from the beginning of the current page.
  virtual void CandidateMoveToPageIndex(size_t index);
  // Moves the focus to the candidate represented by the shortcut.  If
  // the shortcut is not bound with any candidate, false is returned.
  virtual bool CandidateMoveToShortcut(char shortcut);

  // Operation for the candidate list.
  virtual void SetCandidateListVisible(bool visible);

  // Fills protocol buffers and update the internal status.
  virtual void PopOutput(const composer::Composer &composer,
                         commands::Output *output);

  // Fills protocol buffers
  virtual void FillOutput(const composer::Composer &composer,
                          commands::Output *output) const;

  // Sets setting by the request;
  virtual void SetRequest(const commands::Request *request);

  // Set setting by the context.
  virtual void OnStartComposition(const commands::Context &context);

  // Fills segments with the conversion preferences.
  static void SetConversionPreferences(
      const ConversionPreferences &preferences,
      Segments *segments);

  // Copies SessionConverter
  // TODO(hsumita): Copy all member variables.
  // Currently, converter_ is not copied.
  virtual SessionConverter *Clone() const;

  // Fills protocol buffers with all flatten candidate words.
  void FillAllCandidateWords(commands::CandidateList *candidates) const;

  // Meaning that all the composition characters are consumed.
  // c.f. CommitSuggestionInternal
  static const size_t kConsumedAllCharacters;

 private:
  friend class SessionConverterTest;

  // Resets the result value stored at the previous command.
  void ResetResult();

  // Resets the session state variables.
  void ResetState();

  // Notifies the converter that the current segment is focused.
  void SegmentFocus();

  // Notifies the converter that the current segment is fixed.
  void SegmentFix();

  // Gets preedit from segment(index) to segment(index + size).
  void GetPreedit(size_t index, size_t size, string *preedit) const;
  // Gets conversion from segment(index) to segment(index + size).
  void GetConversion(size_t index, size_t size, string *conversion) const;
  // Gets consumed size of the preedit characters.
  // c.f. CommitSuggestionInternal
  size_t GetConsumedPreeditSize(const size_t index, size_t size) const;

  // Performs the command if the command candidate is selected.  True
  // is returned if a command is performed.
  bool MaybePerformCommandCandidate(size_t index, size_t size) const;

  // Updates internal states
  bool UpdateResult(size_t index, size_t size, size_t *consumed_key_size);

  // Fills the candidate list with the focused segment's candidates.
  // This method does not clear the candidate list before processing.
  // Only the candidates of which id is not existent in the candidate list
  // are appended. Other candidates are ignored.
  void AppendCandidateList();
  // Clears the candidate list and fill it with the focused segment's
  // candidates.
  void UpdateCandidateList();

  // Returns the candidate index to be used by the converter.
  int GetCandidateIndexForConverter(const size_t segment_index) const;

  // If focus_id is pointing to the last of suggestions,
  // call StartPrediction().
  void MaybeExpandPrediction(const composer::Composer &composer);

  // Returns the value of candidate to be used by the converter.
  string GetSelectedCandidateValue(size_t segment_index) const;

  // Returns the candidate to be used by the converter.
  const Segment::Candidate &GetSelectedCandidate(size_t segment_index) const;

  // Returns the length of committed candidate's key in characters.
  // True is returned if the selected candidate is successfully committed.
  bool CommitSuggestionInternal(const composer::Composer &composer,
                                const commands::Context &context,
                                size_t *committed_length);

  void SegmentFocusInternal(size_t segment_index);
  void ResizeSegmentWidth(const composer::Composer &composer, int delta);

  void FillConversion(commands::Preedit *preedit) const;
  void FillResult(commands::Result *result) const;
  void FillCandidates(commands::Candidates *candidates) const;

  bool IsEmptySegment(const Segment &segment) const;

  // Handles selected_indices for usage stats.
  void InitializeSelectedCandidateIndices();
  void UpdateSelectedCandidateIndex();
  void UpdateCandidateStats(const string &base_name, int32 index);
  void CommitUsageStats(
      SessionConverterInterface::State commit_state,
      const commands::Context &context);
  void CommitUsageStatsWithSegmentsSize(
      SessionConverterInterface::State commit_state,
      const commands::Context &context,
      size_t submit_segment_size);

  SessionConverterInterface::State state_;

  const ConverterInterface *converter_;
  scoped_ptr<Segments> segments_;
  size_t segment_index_;

  // Previous suggestions to be merged with the current predictions.
  Segment previous_suggestions_;

  // Default conversion preferences.
  ConversionPreferences conversion_preferences_;

  // Preferences for user's operation.
  OperationPreferences operation_preferences_;

  scoped_ptr<commands::Result> result_;

  scoped_ptr<CandidateList> candidate_list_;
  bool candidate_list_visible_;

  const commands::Request *request_;

  // Selected index data of each segments for usage stats.
  vector<int> selected_candidate_indices_;

  // Revision number of client context with which the converter determines when
  // the history segments should be invalidated. See the implemenation of
  // OnStartComposition for details.
  int32 client_revision_;

  DISALLOW_COPY_AND_ASSIGN(SessionConverter);
};

}  // namespace session
}  // namespace mozc

#endif  // MOZC_SESSION_SESSION_CONVERTER_H_
