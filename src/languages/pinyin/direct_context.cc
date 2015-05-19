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

#include "languages/pinyin/direct_context.h"

#include <cctype>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/util.h"
#include "languages/pinyin/session_config.h"

namespace mozc {
namespace pinyin {
namespace direct {

namespace {
const char kInputPrefixCharacter = 'v';
}  // namespace

DirectContext::DirectContext(const SessionConfig &session_config)
    : session_config_(session_config) {
}

DirectContext::~DirectContext() {
}

bool DirectContext::Insert(char ch) {
  const string input(1, ch);

  // TODO(hsumita): Move this logic to SessionConverter.
  if (session_config_.full_width_word_mode) {
    Util::HalfWidthAsciiToFullWidthAscii(input, &commit_text_);
  } else {
    Util::FullWidthAsciiToHalfWidthAscii(input, &commit_text_);
  }

  return true;
}

void DirectContext::Commit() {
  // Does nothing since all character are commited by Insert().
}

void DirectContext::CommitPreedit() {
  // Does nothing since all character are commited by Insert().
}

void DirectContext::Clear() {
  ClearCommitText();
}

void DirectContext::ClearCommitText() {
  commit_text_.clear();
}

bool DirectContext::MoveCursorRight() {
  DLOG(ERROR) << "MoveCursorRight will not be expected to call.";
  return false;
}

bool DirectContext::MoveCursorLeft() {
  DLOG(ERROR) << "MoveCursorLeft will not be expected to call.";
  return false;
}

bool DirectContext::MoveCursorRightByWord() {
  DLOG(ERROR) << "MoveCursorRightByWord will not be expected to call.";
  return false;
}

bool DirectContext::MoveCursorLeftByWord() {
  DLOG(ERROR) << "MoveCursorLeftByWord will not be expected to call.";
  return false;
}

bool DirectContext::MoveCursorToBeginning() {
  DLOG(ERROR) << "MoveCursorToBeginning will not be expected to call.";
  return false;
}

bool DirectContext::MoveCursorToEnd() {
  DLOG(ERROR) << "MoveCursorToEnd will not be expected to call.";
  return false;
}

bool DirectContext::SelectCandidate(size_t index) {
  DLOG(ERROR) << "SelectCandidate will not be expected to call.";
  return false;
}

bool DirectContext::FocusCandidate(size_t index) {
  DLOG(ERROR) << "FocusCandidate will not be expected to call.";
  return false;
}

bool DirectContext::ClearCandidateFromHistory(size_t index) {
  // This context doesn't use history.
  return true;
}

bool DirectContext::RemoveCharBefore() {
  DLOG(ERROR) << "RemoveCharBefore will not be expected to call.";
  return false;
}

bool DirectContext::RemoveCharAfter() {
  DLOG(ERROR) << "RemoveCharAfter will not be expected to call.";
  return false;
}

bool DirectContext::RemoveWordBefore() {
  DLOG(ERROR) << "RemoveWordBefore will not be expected to call.";
  return false;
}

bool DirectContext::RemoveWordAfter() {
  DLOG(ERROR) << "RemoveWordAfter will not be expected to call.";
  return false;
}

void DirectContext::ReloadConfig() {
  // Direct mode does NOT use a configuration.
}

const string &DirectContext::commit_text() const {
  return commit_text_;
}

// There is no composition text on Direct mode.
const string &DirectContext::input_text() const { return empty_text_; }
const string &DirectContext::selected_text() const { return empty_text_; }
const string &DirectContext::conversion_text() const { return empty_text_; }
const string &DirectContext::rest_text() const { return empty_text_; }
const string &DirectContext::auxiliary_text() const { return empty_text_; }
size_t DirectContext::cursor() const { return 0; }

// There is no candidates.
size_t DirectContext::focused_candidate_index() const { return 0; }
bool DirectContext::GetCandidate(size_t index, Candidate *candidate) {
  return false;
}
bool DirectContext::HasCandidate(size_t index) { return false; }
size_t DirectContext::PrepareCandidates(size_t required_size) { return 0; }

}  // namespace direct
}  // namespace pinyin
}  // namespace mozc
