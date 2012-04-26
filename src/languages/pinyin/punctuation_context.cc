// Copyright 2010-2012, Google Inc.
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

#include "languages/pinyin/punctuation_context.h"

#include <cctype>
#include <map>
#include <vector>

#include "base/port.h"
#include "base/singleton.h"
#include "base/util.h"
#include "languages/pinyin/punctuation_table.h"
#include "languages/pinyin/session_config.h"

namespace mozc {
namespace pinyin {
namespace punctuation {

namespace {
// TODO(hsumita): Handling quote and double quote (open and close), and comma
// (especially comma after number) for direct commit.

const char kPunctuationSpecialKey = '`';
}  // namespace

PunctuationContext::PunctuationContext(const SessionConfig &session_config)
    : table_(Singleton<PunctuationTable>::get()),
      session_config_(session_config) {
  Clear();
}

PunctuationContext::~PunctuationContext() {}

bool PunctuationContext::Insert(char ch) {
  if (!isgraph(ch)) {
    return false;
  }

  if (input_text_.empty() && ch != kPunctuationSpecialKey) {
    return DirectCommit(ch);
  }

  if (is_initial_state_ && !input_text_.empty()) {
    is_initial_state_ = false;
    input_text_.clear();
    selected_text_.clear();
    rest_text_.clear();
    cursor_ = 0;
  }

  input_text_ += ch;
  ++cursor_;
  focused_candidate_index_ = 0;

  UpdateCandidates();
  UpdateAuxiliaryText();
  selected_text_.append(candidates_[focused_candidate_index_]);

  return true;
}

void PunctuationContext::Commit() {
  string result;
  result.append(selected_text_);
  result.append(rest_text_);
  Clear();
  commit_text_.assign(result);
}

void PunctuationContext::CommitPreedit() {
  const string result = input_text_;
  Clear();
  commit_text_.assign(result);
}

void PunctuationContext::Clear() {
  ClearCommitText();

  is_initial_state_ = true;
  input_text_.clear();
  selected_text_.clear();
  rest_text_.clear();
  auxiliary_text_.clear();
  cursor_ = 0;
  focused_candidate_index_ = 0;
  candidates_.clear();
}

void PunctuationContext::ClearCommitText() {
  commit_text_.clear();
}

bool PunctuationContext::MoveCursorRight() {
  if (cursor_ == input_text_.size()) {
    return true;
  }
  return MoveCursorInternal(cursor_ + 1);
}

bool PunctuationContext::MoveCursorLeft() {
  if (cursor_ == 0) {
    return true;
  }
  return MoveCursorInternal(cursor_ - 1);
}

bool PunctuationContext::MoveCursorRightByWord() {
  if (cursor_ == input_text_.size()) {
    return true;
  }
  return MoveCursorRight();
}

bool PunctuationContext::MoveCursorLeftByWord() {
  return MoveCursorLeft();
}

bool PunctuationContext::MoveCursorToBeginning() {
  if (cursor_ == 0) {
    return true;
  }
  return MoveCursorInternal(0);
}

bool PunctuationContext::MoveCursorToEnd() {
  if (cursor_ == input_text_.size()) {
    return true;
  }
  return MoveCursorInternal(input_text_.size());
}

bool PunctuationContext::SelectCandidate(size_t index) {
  if (index >= candidates_.size()) {
    return false;
  }

  if (!FocusCandidate(index)) {
    return false;
  }

  Commit();
  return true;
}

bool PunctuationContext::FocusCandidate(size_t index) {
  if (index >= candidates_.size()) {
    return false;
  }

  focused_candidate_index_ = index;
  DCHECK(!selected_text_.empty());
  selected_text_.assign(
      Util::SubString(selected_text_, 0, Util::CharsLen(selected_text_) - 1));
  selected_text_.append(candidates_[focused_candidate_index_]);

  return true;
}

bool PunctuationContext::FocusCandidatePrev() {
  if (focused_candidate_index_ == 0) {
    return true;
  }
  return FocusCandidate(focused_candidate_index_ - 1);
}

bool PunctuationContext::FocusCandidateNext() {
  if (focused_candidate_index_ + 1 >= candidates_.size()) {
    return false;
  }
  return FocusCandidate(focused_candidate_index_ + 1);
}

bool PunctuationContext::ClearCandidateFromHistory(size_t index) {
  // This context doesn't use history.
  return true;
}

bool PunctuationContext::RemoveCharBefore() {
  if (cursor_ == 0) {
    return false;
  }

  input_text_.erase(cursor_ - 1, 1);
  --cursor_;

  if (input_text_.empty()) {
    Clear();
    return true;
  }

  DCHECK(!selected_text_.empty());
  selected_text_ =
      Util::SubString(selected_text_, 0, Util::CharsLen(selected_text_) - 1);

  focused_candidate_index_ = 0;
  UpdateCandidates();
  UpdateAuxiliaryText();
  return true;
}

bool PunctuationContext::RemoveCharAfter() {
  if (cursor_ == input_text_.size()) {
    return false;
  }

  input_text_.erase(cursor_, 1);

  if (input_text_.empty()) {
    Clear();
    return true;
  }

  rest_text_ = Util::SubString(rest_text_, 1, Util::CharsLen(rest_text_) - 1);

  UpdateCandidates();
  UpdateAuxiliaryText();
  return true;
}

bool PunctuationContext::RemoveWordBefore() {
  if (cursor_ == 0) {
    return false;
  }
  return RemoveCharBefore();
}

bool PunctuationContext::RemoveWordAfter() {
  if (cursor_ == input_text_.size()) {
    return false;
  }
  return RemoveCharAfter();
}

void PunctuationContext::ReloadConfig() {
  // This context doesn't use config.
}

const string &PunctuationContext::commit_text() const {
  return commit_text_;
}

const string &PunctuationContext::input_text() const {
  return input_text_;
}

const string &PunctuationContext::selected_text() const {
  return selected_text_;
}

const string &PunctuationContext::conversion_text() const {
  // Conversion text is not used on this context.
  return empty_text_;
}

const string &PunctuationContext::rest_text() const {
  return rest_text_;
}

const string &PunctuationContext::auxiliary_text() const {
  return auxiliary_text_;
}

size_t PunctuationContext::cursor() const {
  return cursor_;
}

size_t PunctuationContext::focused_candidate_index() const {
  return focused_candidate_index_;
}

size_t PunctuationContext::candidates_size() const {
  return candidates_.size();
}

void PunctuationContext::GetCandidates(vector<string> *candidates) const {
  DCHECK(candidates);
  candidates->assign(candidates_.begin(), candidates_.end());
}

bool PunctuationContext::DirectCommit(char ch) {
  string text(1, ch);

  if (session_config_.full_width_punctuation_mode) {
    if (session_config_.simplified_chinese_mode) {
      table_->GetDirectCommitTextForSimplifiedChinese(ch, &text);
    } else {
      table_->GetDirectCommitTextForTraditionalChinese(ch, &text);
    }
    // We use an original character as a commit text if
    // GetDirectCommitTextFor*() is failed.
  } else {
    // TODO(hsumita): Move this logic to SessionConverter.
    if (session_config_.full_width_word_mode) {
      string full_text;
      Util::HalfWidthAsciiToFullWidthAscii(text, &full_text);
      text.assign(full_text);
    } else {
      string half_text;
      Util::FullWidthAsciiToHalfWidthAscii(text, &half_text);
      text.assign(half_text);
    }
  }

  selected_text_.assign(text);
  Commit();
  return true;
}

bool PunctuationContext::MoveCursorInternal(size_t index) {
  if (index > input_text_.size()) {
    return false;
  }

  cursor_ = index;
  focused_candidate_index_ = 0;

  const string text(selected_text_ + rest_text_);
  Util::SubString(text, 0, cursor_, &selected_text_);
  Util::SubString(text, cursor_, Util::CharsLen(text), &rest_text_);

  UpdateCandidates();
  UpdateAuxiliaryText();

  return true;
}

void PunctuationContext::UpdateCandidates() {
  DCHECK(!input_text_.empty());

  candidates_.clear();

  if (cursor_ == 0) {
    return;
  }

  if (is_initial_state_) {
    // Show default candidates.
    table_->GetDefaultCandidates(&candidates_);
    return;
  }

  const char key = input_text_[cursor_ - 1];
  table_->GetCandidates(key, &candidates_);
}

void PunctuationContext::UpdateAuxiliaryText() {
  DCHECK(!input_text_.empty());
  DCHECK_LE(cursor_, input_text_.size());

  auxiliary_text_.assign(input_text_.substr(0, cursor_));
  auxiliary_text_.append("|");
  auxiliary_text_.append(input_text_.substr(cursor_));
}

}  // namespace punctuation
}  // namespace pinyin
}  // namespace mozc
