// Copyright 2010-2013, Google Inc.
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

#include "languages/pinyin/pinyin_context_mock.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"

namespace mozc {
namespace pinyin {

namespace {
// Used to determine word boundaries.
const size_t kWordSize = 3;
const char *kAuxiliaryTextPrefix = "auxiliary_text_";
}  // namespace

PinyinContextMock::PinyinContextMock() {
  double_pinyin_ = GET_CONFIG(pinyin_config).double_pinyin();
  Clear();
}

PinyinContextMock::~PinyinContextMock() {
}

bool PinyinContextMock::Insert(char ch) {
  if (!islower(ch)) {
    return false;
  }

  input_text_ += ch;
  ++cursor_;
  focused_candidate_index_ = 0;
  Update();
  return true;
}

void PinyinContextMock::Commit() {
  string result;
  result.append(selected_text_);
  result.append(input_text_.substr(Util::CharsLen(selected_text_)));
  Clear();
  commit_text_ = result;
}

void PinyinContextMock::CommitPreedit() {
  const string result = input_text_;
  Clear();
  commit_text_ = result;
}

void PinyinContextMock::Clear() {
  ClearCommitText();

  input_text_.clear();
  selected_text_.clear();
  conversion_text_.clear();
  rest_text_.clear();
  auxiliary_text_.clear();
  cursor_ = 0;
  focused_candidate_index_ = 0;
  candidates_.clear();
}

void PinyinContextMock::ClearCommitText() {
  commit_text_.clear();
}

bool PinyinContextMock::MoveCursorRight() {
  const size_t pos = min(input_text_.size(), cursor_ + 1);
  return MoveCursorInternal(pos);
}

bool PinyinContextMock::MoveCursorLeft() {
  const size_t pos = (cursor_ == 0) ? 0 : cursor_ - 1;
  return MoveCursorInternal(pos);
}

bool PinyinContextMock::MoveCursorRightByWord() {
  return MoveCursorInternal(BoundaryNext());
}

bool PinyinContextMock::MoveCursorLeftByWord() {
  return MoveCursorInternal(BoundaryPrev());
}

bool PinyinContextMock::MoveCursorToBeginning() {
  return MoveCursorInternal(0);
}

bool PinyinContextMock::MoveCursorToEnd() {
  return MoveCursorInternal(input_text_.size());
}

bool PinyinContextMock::SelectCandidate(size_t index) {
  if (index >= candidates_.size()) {
    return false;
  }

  selected_text_.append(candidates_[index]);
  conversion_text_.clear();
  focused_candidate_index_ = 0;

  if (Util::CharsLen(selected_text_) == input_text_.size()) {
    Commit();
  } else {
    Update();
  }
  return true;
}

bool PinyinContextMock::FocusCandidate(size_t index) {
  if (index >= candidates_.size()) {
    return false;
  }

  if (input_text_.size() == cursor_) {
    conversion_text_ = candidates_[index];
  } else {
    conversion_text_ = input_text_.substr(Util::CharsLen(selected_text_),
                                          Util::CharsLen(candidates_[index]));
  }
  rest_text_ = input_text_.substr(
      Util::CharsLen(selected_text_) + Util::CharsLen(conversion_text_));
  focused_candidate_index_ = index;
  return true;
}

bool PinyinContextMock::ClearCandidateFromHistory(size_t index) {
  if (index >= candidates_.size()) {
    return false;
  }

  candidates_.erase(candidates_.begin() + index);
  focused_candidate_index_ = 0;
  UpdateConversion();
  return true;
}

bool PinyinContextMock::RemoveCharBefore() {
  if (cursor_ == 0) {
    return false;
  }

  input_text_.erase(cursor_ - 1, 1);
  --cursor_;
  focused_candidate_index_ = 0;
  Update();
  return true;
}

bool PinyinContextMock::RemoveCharAfter() {
  if (cursor_ == input_text_.size()) {
    return false;
  }

  input_text_.erase(cursor_, 1);
  rest_text_ = input_text_.substr(cursor_);
  return true;
}

bool PinyinContextMock::RemoveWordBefore() {
  if (cursor_ == 0) {
    return false;
  }

  const size_t boundary = BoundaryPrev();
  input_text_.erase(boundary, cursor_ - boundary);
  cursor_ = boundary;
  focused_candidate_index_ = 0;
  Update();
  return true;
}

bool PinyinContextMock::RemoveWordAfter() {
  if (cursor_ == input_text_.size()) {
    return false;
  }

  const size_t boundary = BoundaryNext();
  input_text_.erase(cursor_, boundary - cursor_);
  rest_text_ = input_text_.substr(cursor_);
  return true;
}

void PinyinContextMock::ReloadConfig() {
  bool new_mode = GET_CONFIG(pinyin_config).double_pinyin();
  if (new_mode != double_pinyin_) {
    double_pinyin_ = new_mode;
    Clear();
  }
}

const string &PinyinContextMock::commit_text() const {
  return commit_text_;
}

const string &PinyinContextMock::input_text() const {
  return input_text_;
}

const string &PinyinContextMock::selected_text() const {
  return selected_text_;
}

const string &PinyinContextMock::conversion_text() const {
  return conversion_text_;
}

const string &PinyinContextMock::rest_text() const {
  return rest_text_;
}

const string &PinyinContextMock::auxiliary_text() const {
  return auxiliary_text_;
}

size_t PinyinContextMock::cursor() const {
  return cursor_;
}

size_t PinyinContextMock::focused_candidate_index() const {
  return focused_candidate_index_;
}

size_t PinyinContextMock::candidates_size() const {
  return candidates_.size();
}

bool PinyinContextMock::HasCandidate(size_t index) {
  return index < candidates_.size();
}

bool PinyinContextMock::GetCandidate(size_t index, Candidate *candidate) {
  DCHECK(candidate);
  if (!HasCandidate(index)) {
    return false;
  }
  candidate->text.assign(candidates_[index]);
  return true;
}

size_t PinyinContextMock::PrepareCandidates(size_t index) {
  return min(index, candidates_.size());
}

size_t PinyinContextMock::BoundaryNext() const {
  return min(input_text_.size(),
             (cursor_ + kWordSize) / kWordSize * kWordSize);
}

size_t PinyinContextMock::BoundaryPrev() const {
  if (cursor_ == 0) {
    return 0;
  }
  return (cursor_ - 1) / kWordSize * kWordSize;
}

void PinyinContextMock::Update() {
  UpdateCandidates();
  UpdateConversion();
}

void PinyinContextMock::UpdateCandidates() {
  const size_t selected_length = Util::CharsLen(selected_text_);
  const string converting_string = input_text_.substr(
      selected_length, cursor_ - selected_length);

  string base = converting_string;
  Util::UpperString(&base);

  candidates_.clear();
  for (size_t i = 0; i < base.size(); ++i) {
    const string &sub_text = base.substr(0, base.size() - i);
    string candidate;
    Util::HalfWidthAsciiToFullWidthAscii(sub_text, &candidate);
    candidates_.push_back(candidate);
  }
}

void PinyinContextMock::UpdateConversion() {
  conversion_text_.clear();
  rest_text_.clear();
  auxiliary_text_.clear();

  const size_t selected_length = Util::CharsLen(selected_text_);

  if (candidates_.empty()) {
    rest_text_ = input_text_.substr(selected_length);
    return;
  }

  if (cursor_ == input_text_.size()) {
    conversion_text_ = candidates_[focused_candidate_index_];
    const size_t consumed =
        Util::CharsLen(selected_text_) + Util::CharsLen(conversion_text_);
    rest_text_ = input_text_.substr(consumed);
  } else {
    conversion_text_ = input_text_.substr(
        selected_length, cursor_ - selected_length);
    rest_text_ = input_text_.substr(cursor_);
  }

  auxiliary_text_ = kAuxiliaryTextPrefix + input_text_.substr(
      selected_length, cursor_ - selected_length);
}

bool PinyinContextMock::MoveCursorInternal(size_t pos) {
  if (pos > input_text_.size()) {
    LOG(ERROR) << "Too big cursor index!";
    return false;
  }

  if (pos == cursor_) {
    return true;
  }

  cursor_ = pos;

  selected_text_.clear();
  conversion_text_.clear();
  rest_text_.clear();
  focused_candidate_index_ = 0;
  candidates_.clear();

  Update();

  return true;
}

}  // namespace pinyin
}  // namespace mozc
