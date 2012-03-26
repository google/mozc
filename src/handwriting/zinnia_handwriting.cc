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

// Handwriting module using zinnia.

#include "handwriting/zinnia_handwriting.h"

#include "base/util.h"

#ifdef OS_MACOSX
#include "base/mac_util.h"
#endif

namespace mozc {
namespace handwriting {

namespace {
string GetModelFileName() {
#ifdef OS_MACOSX
  // TODO(komatsu): Fix the file name to "handwriting-ja.model" like the
  // Windows implementation regardless which data file is actually
  // used.  See also gui.gyp:hand_writing_mac.
  const char kModelFile[] = "handwriting-light-ja.model";
  return Util::JoinPath(MacUtil::GetResourcesDirectory(), kModelFile);
#elif defined(USE_LIBZINNIA)
  // On Linux, use the model for tegaki-zinnia.
  const char kModelFile[] =
      "/usr/share/tegaki/models/zinnia/handwriting-ja.model";
  return kModelFile;
#else
  const char kModelFile[] = "handwriting-ja.model";
  return Util::JoinPath(Util::GetServerDirectory(), kModelFile);
#endif  // OS_MACOSX
}

const uint32 kBoxSize = 200;
}  // namespace

ZinniaHandwriting::ZinniaHandwriting()
    : recognizer_(zinnia::Recognizer::create()),
      character_(zinnia::Character::create()),
      mmap_(new Mmap<char>()),
      zinnia_model_error_(false) {
  const string model_file = GetModelFileName();
  DCHECK(recognizer_.get());
  DCHECK(character_.get());
  if (!mmap_->Open(model_file.c_str())) {
    LOG(ERROR) << "Cannot open model file:" << model_file;
    zinnia_model_error_ = true;
    return;
  }
  if (!recognizer_->open(mmap_->begin(), mmap_->GetFileSize())) {
    LOG(ERROR) << "Model file is broken:" << model_file;
    zinnia_model_error_ = true;
    return;
  }
}

HandwritingStatus ZinniaHandwriting::Recognize(
    const Strokes &strokes, vector<string> *candidates) const {
  if (zinnia_model_error_) {
    return HANDWRITING_ERROR;
  }

  character_->clear();
  character_->set_width(kBoxSize);
  character_->set_height(kBoxSize);
  for (size_t i = 0; i < strokes.size(); ++i) {
    for (size_t j = 0; j < strokes[i].size(); ++j) {
      character_->add(i,
                      static_cast<int>(kBoxSize * strokes[i][j].first),
                      static_cast<int>(kBoxSize * strokes[i][j].second));
    }
  }

  const int kMaxResultSize = 100;
  scoped_ptr<zinnia::Result> result(recognizer_->classify(*character_,
                                                          kMaxResultSize));
  if (result.get() == NULL) {
    return HANDWRITING_ERROR;
  }

  candidates->clear();
  for (size_t i = 0; i < result->size(); ++i) {
    candidates->push_back(result->value(i));
  }
  return HANDWRITING_NO_ERROR;
}

HandwritingStatus ZinniaHandwriting::Commit(const Strokes &strokes,
                                            const string &result) {
  // Do nothing so far.
  return HANDWRITING_NO_ERROR;
}

}  // namespace handwriting
}  // namespace mozc
