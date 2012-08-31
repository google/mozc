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

#include "languages/pinyin/pinyin_context.h"

#include <PyZy/Const.h>
#include <PyZy/InputContext.h>
#include <PyZy/Variant.h>
#include <string>
#include "base/util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "languages/pinyin/session_config.h"

namespace mozc {
namespace pinyin {

class ContextObserver : public ContextObserverInterface {
 public:
  explicit ContextObserver(const SessionConfig &session_config)
      : session_config_(session_config) {
  }

  virtual ~ContextObserver() {}

  virtual const string &commit_text() const {
    return commit_text_;
  }

  virtual void SetCommitText(const string &commit_text) {
    // TODO(hsumita): Move this logic to SessionConverter.
    if (session_config_.full_width_word_mode) {
      Util::HalfWidthAsciiToFullWidthAscii(commit_text, &commit_text_);
    } else {
      commit_text_.assign(commit_text);
    }
  }

  virtual void ClearCommitText() {
    commit_text_.clear();
  }

  // Callback interfaces which are called by libpyzy.
  virtual void commitText(PyZy::InputContext *context,
                  const string &commit_text) {
    SetCommitText(commit_text);
  }

  // We don't use these function. Do nothings.
  virtual void inputTextChanged(PyZy::InputContext *context) {}
  virtual void cursorChanged(PyZy::InputContext *context) {}
  virtual void preeditTextChanged(PyZy::InputContext *context) {}
  virtual void auxiliaryTextChanged(PyZy::InputContext *context) {}
  virtual void candidatesChanged(PyZy::InputContext *context) {}

 private:
  string commit_text_;
  const SessionConfig &session_config_;
};

// Apply this change to the header file.
PinyinContext::PinyinContext(const SessionConfig &session_config)
    : session_config_(session_config),
      observer_(new ContextObserver(session_config)) {
  ResetContext();
  ReloadConfig();
}

PinyinContext::~PinyinContext() {
}

bool PinyinContext::Insert(char ch) {
  if (isdigit(ch) && input_text().empty()) {
    observer_->SetCommitText(string(1, ch));
    return true;
  }
  return context_->insert(ch);
}

void PinyinContext::Commit() {
  context_->commit(PyZy::InputContext::TYPE_CONVERTED);
}

void PinyinContext::CommitPreedit() {
  context_->commit(PyZy::InputContext::TYPE_RAW);
}

void PinyinContext::Clear() {
  context_->reset();
  ClearCommitText();
}

void PinyinContext::ClearCommitText() {
  observer_->ClearCommitText();
}

bool PinyinContext::MoveCursorRight() {
  if (context_->unselectCandidates()) {
    return true;
  }
  return context_->moveCursorRight();
}

bool PinyinContext::MoveCursorLeft() {
  if (context_->unselectCandidates()) {
    return true;
  }
  return context_->moveCursorLeft();
}

bool PinyinContext::MoveCursorRightByWord() {
  if (context_->unselectCandidates()) {
    return true;
  }
  return context_->moveCursorRightByWord();
}

bool PinyinContext::MoveCursorLeftByWord() {
  if (context_->unselectCandidates()) {
    return true;
  }
  return context_->moveCursorLeftByWord();
}

bool PinyinContext::MoveCursorToBeginning() {
  if (context_->unselectCandidates()) {
    return true;
  }
  return context_->moveCursorToBegin();
}

bool PinyinContext::MoveCursorToEnd() {
  if (context_->unselectCandidates()) {
    return true;
  }
  return context_->moveCursorToEnd();
}

bool PinyinContext::SelectCandidate(size_t index) {
  return context_->selectCandidate(index);
}

bool PinyinContext::FocusCandidate(size_t index) {
  return context_->focusCandidate(index);
}

bool PinyinContext::ClearCandidateFromHistory(size_t index) {
  return context_->resetCandidate(index);
}

bool PinyinContext::RemoveCharBefore() {
  return context_->removeCharBefore();
}

bool PinyinContext::RemoveCharAfter() {
  return context_->removeCharAfter();
}

bool PinyinContext::RemoveWordBefore() {
  return context_->removeWordBefore();
}

bool PinyinContext::RemoveWordAfter() {
  return context_->removeWordAfter();
}

namespace {
const uint32 kIncompletePinyinOption = PINYIN_INCOMPLETE_PINYIN;
const uint32 kCorrectPinyinOption = PINYIN_CORRECT_ALL;
const uint32 kFuzzyPinyinOption =
    PINYIN_FUZZY_C_CH |
    PINYIN_FUZZY_Z_ZH |
    PINYIN_FUZZY_S_SH |
    PINYIN_FUZZY_L_N |
    PINYIN_FUZZY_F_H |
    PINYIN_FUZZY_K_G |
    PINYIN_FUZZY_G_K |
    PINYIN_FUZZY_AN_ANG |
    PINYIN_FUZZY_ANG_AN |
    PINYIN_FUZZY_EN_ENG |
    PINYIN_FUZZY_ENG_EN |
    PINYIN_FUZZY_IN_ING |
    PINYIN_FUZZY_ING_IN;
}  // namespace

void PinyinContext::ReloadConfig() {
  const config::PinyinConfig &config = GET_CONFIG(pinyin_config);

  // Resets a context if input method is changed.
  if (config.double_pinyin() != double_pinyin_) {
    ResetContext();
  }

  uint32 conversion_option = kIncompletePinyinOption;
  if (config.correct_pinyin()) {
    conversion_option |= kCorrectPinyinOption;
  }
  if (config.fuzzy_pinyin()) {
    conversion_option |= kFuzzyPinyinOption;
  }
  context_->setProperty(PyZy::InputContext::PROPERTY_CONVERSION_OPTION,
                        PyZy::Variant::fromUnsignedInt(conversion_option));

  context_->setProperty(
      PyZy::InputContext::PROPERTY_DOUBLE_PINYIN_SCHEMA,
      PyZy::Variant::fromUnsignedInt(config.double_pinyin_schema()));

  context_->setProperty(
      PyZy::InputContext::PROPERTY_MODE_SIMP,
      PyZy::Variant::fromBool(session_config_.simplified_chinese_mode));
}

const string &PinyinContext::commit_text() const {
  return observer_->commit_text();
}

const string &PinyinContext::input_text() const {
  return context_->inputText();
}

const string &PinyinContext::selected_text() const {
  return context_->selectedText();
}

const string &PinyinContext::conversion_text() const {
  return context_->conversionText();
}

const string &PinyinContext::rest_text() const {
  return context_->restText();
}

const string &PinyinContext::auxiliary_text() const {
  return context_->auxiliaryText();
}

size_t PinyinContext::cursor() const {
  return context_->cursor();
}

size_t PinyinContext::focused_candidate_index() const {
  return context_->focusedCandidate();
}

bool PinyinContext::GetCandidate(size_t index, Candidate *candidate) {
  DCHECK(candidate);

  PyZy::Candidate pyzy_candidate;
  if (context_->getCandidate(index, pyzy_candidate)) {
    candidate->text.assign(pyzy_candidate.text);
    return true;
  }
  return false;
}

bool PinyinContext::HasCandidate(size_t index) {
  return context_->hasCandidate(index);
}

size_t PinyinContext::PrepareCandidates(size_t required_size) {
  DCHECK_NE(0, required_size);
  if (context_->hasCandidate(required_size - 1)) {
    return required_size;
  }
  return context_->getPreparedCandidatesSize();
}

void PinyinContext::ResetContext() {
  double_pinyin_ = GET_CONFIG(pinyin_config).double_pinyin();

  PyZy::InputContext::InputType type = double_pinyin_
      ? PyZy::InputContext::DOUBLE_PINYIN
      : PyZy::InputContext::FULL_PINYIN;

  context_.reset(PyZy::InputContext::create(type, observer_.get()));
  Clear();
}

}  // namespace pinyin
}  // namespace mozc
