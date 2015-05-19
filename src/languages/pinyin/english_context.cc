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

#include "languages/pinyin/english_context.h"

#include <cctype>
#include <string>
#include <vector>

#include "base/util.h"
#include "languages/pinyin/english_dictionary_factory.h"
#include "languages/pinyin/session_config.h"

namespace mozc {
namespace pinyin {
namespace english {

namespace {
const char kInputPrefixCharacter = 'v';
const size_t kMaxWordLength = 80;
}  // namespace

EnglishContext::EnglishContext(const SessionConfig &session_config)
    : session_config_(session_config) {}
EnglishContext::~EnglishContext() {}

bool EnglishContext::Insert(char ch) {
  if (!isalpha(ch)) {
    return false;
  }

  // Ignore a too long word to avoid since user dictionary cannot store it.
  if (input_text_.size() >= kMaxWordLength) {
    return false;
  }

  input_text_.append(1, ch);
  DCHECK_EQ(kInputPrefixCharacter, input_text_[0]);
  Suggest();
  UpdateAuxiliaryText();
  return true;
}

void EnglishContext::Commit() {
  CommitPreedit();
}

void EnglishContext::CommitPreedit() {
  if (input_text_.size() <= 1) {
    DCHECK(input_text_.empty() || input_text_[0] == kInputPrefixCharacter);
    Clear();
    return;
  }

  const string result = input_text_.substr(1);
  EnglishDictionaryFactory::GetDictionary()->LearnWord(result);
  Clear();

  // TODO(hsumita): Move this logic to SessionConverter.
  if (session_config_.full_width_word_mode) {
    Util::HalfWidthAsciiToFullWidthAscii(result, &commit_text_);
  } else {
    Util::FullWidthAsciiToHalfWidthAscii(result, &commit_text_);
  }
}

void EnglishContext::Clear() {
  input_text_.clear();
  commit_text_.clear();
  auxiliary_text_.clear();
  focused_candidate_index_ = 0;
  candidates_.clear();
}

void EnglishContext::ClearCommitText() {
  commit_text_.clear();
}

// There is no composition text on English mode.
bool EnglishContext::MoveCursorRight() {
  DLOG(ERROR) << "MoveCursorRight will not be expected to call.";
  return false;
}

bool EnglishContext::MoveCursorLeft() {
  DLOG(ERROR) << "MoveCursorLeft will not be expected to call.";
  return false;
}

bool EnglishContext::MoveCursorRightByWord() {
  DLOG(ERROR) << "MoveCursorRightByWord will not be expected to call.";
  return false;
}

bool EnglishContext::MoveCursorLeftByWord() {
  DLOG(ERROR) << "MoveCursorLeftByWord will not be expected to call.";
  return false;
}

bool EnglishContext::MoveCursorToBeginning() {
  DLOG(ERROR) << "MoveCursorToBeginning will not be expected to call.";
  return false;
}

bool EnglishContext::MoveCursorToEnd() {
  DLOG(ERROR) << "MoveCursorToEnd will not be expected to call.";
  return false;
}

bool EnglishContext::SelectCandidate(size_t index) {
  if (!FocusCandidate(index)) {
    return false;
  }
  DCHECK_EQ(kInputPrefixCharacter, input_text_[0]);

  // Commits selected text.
  const string result = candidates_[focused_candidate_index_];
  EnglishDictionaryFactory::GetDictionary()->LearnWord(result);
  Clear();
  commit_text_.assign(result);

  return true;
}

bool EnglishContext::FocusCandidate(size_t index) {
  if (index >= candidates_.size()) {
    return false;
  }
  focused_candidate_index_ = index;
  return true;
}

bool EnglishContext::ClearCandidateFromHistory(size_t index) {
  if (index >= candidates_.size()) {
    return false;
  }

  // Currently this method does not make sense.
  // TODO(hsumita): Implements this function.

  return true;
}

bool EnglishContext::RemoveCharBefore() {
  if (!input_text_.empty()) {
    input_text_.erase(input_text_.size() - 1);
    Suggest();
    UpdateAuxiliaryText();
  }
  return true;
}

bool EnglishContext::RemoveCharAfter() {
  return false;
}

bool EnglishContext::RemoveWordBefore() {
  Clear();
  return true;
}

bool EnglishContext::RemoveWordAfter() {
  return false;
}

// English mode does NOT use a configuration.
void EnglishContext::ReloadConfig() {}

const string &EnglishContext::commit_text() const {
  return commit_text_;
}

const string &EnglishContext::input_text() const {
  return input_text_;
}

// There is no composition text on English mode.
const string &EnglishContext::selected_text() const { return empty_text_; }
const string &EnglishContext::conversion_text() const { return empty_text_; }
const string &EnglishContext::rest_text() const { return empty_text_; }

const string &EnglishContext::auxiliary_text() const {
  return auxiliary_text_;
}

// There is no composition text on English mode.
size_t EnglishContext::cursor() const {
  return 0;
}

size_t EnglishContext::focused_candidate_index() const {
  return focused_candidate_index_;
}

bool EnglishContext::GetCandidate(size_t index, Candidate *candidate) {
  DCHECK(candidate);
  if (!HasCandidate(index)) {
    return false;
  }

  candidate->text.assign(candidates_[index]);
  return true;
}

bool EnglishContext::HasCandidate(size_t index) {
  return index < candidates_.size();
}

size_t EnglishContext::PrepareCandidates(size_t required_size) {
  return min(required_size, candidates_.size());
}

void EnglishContext::Suggest() {
  candidates_.clear();
  focused_candidate_index_ = 0;
  auxiliary_text_.clear();

  if (input_text_.size() <= 1) {
    return;
  }
  DCHECK_EQ(kInputPrefixCharacter, input_text_[0]);

  string query = input_text_.substr(1);
  EnglishDictionaryFactory::GetDictionary()->
      GetSuggestions(query, &candidates_);
}

void EnglishContext::UpdateAuxiliaryText() {
  auxiliary_text_.clear();

  if (input_text_.empty()) {
    return;
  }
  DCHECK_EQ(kInputPrefixCharacter, input_text_[0]);
  auxiliary_text_.assign(1, input_text_[0]);

  if (input_text_.size() > 1) {
    auxiliary_text_.append(" " + input_text_.substr(1));
  }
}

}  // namespace english
}  // namespace pinyin
}  // namespace mozc
