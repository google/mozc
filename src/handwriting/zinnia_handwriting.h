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

// Handwriting module using zinnia.

#ifndef MOZC_HANDWRITING_ZINNIA_HANDWRITING_H_
#define MOZC_HANDWRITING_ZINNIA_HANDWRITING_H_

#include "handwriting/handwriting_manager.h"

#ifdef USE_LIBZINNIA
// Use default zinnia installed in /usr/include
#include <zinnia.h>
#else  // USE_LIBZINNIA
#include "third_party/zinnia/v0_04/zinnia.h"
#endif  // USE_LIBZINNIA

namespace mozc {
class Mmap;

namespace handwriting {

class ZinniaHandwriting : public HandwritingInterface {
 public:
  ZinniaHandwriting();
  virtual ~ZinniaHandwriting();

  HandwritingStatus Recognize(const Strokes &strokes,
                              vector<string> *candidates) const;

  HandwritingStatus Commit(const Strokes &strokes, const string &result);

 private:
  scoped_ptr<zinnia::Recognizer> recognizer_;
  scoped_ptr<zinnia::Character> character_;
  scoped_ptr<Mmap> mmap_;
  size_t width_;
  size_t height_;
  bool zinnia_model_error_;

  DISALLOW_COPY_AND_ASSIGN(ZinniaHandwriting);
};

}  // namespace handwriting
}  // namespace mozc

#endif  // MOZC_HANDWRITING_ZINNIA_HANDWRITING_H_
