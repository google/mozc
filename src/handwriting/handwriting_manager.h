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

// Handwriting module manager class

#ifndef MOZC_HANDWRITING_HANDWRITING_MANAGER_H_
#define MOZC_HANDWRITING_HANDWRITING_MANAGER_H_

#include <string>
#include <vector>

#include "base/base.h"

namespace mozc {
namespace handwriting {

// A coordinate of a stroke.  If the canvas is a square, the point
// range is supporsed from 0.0 to 1.0.
typedef pair<float, float> Point;
typedef vector<Point> Stroke;
typedef vector<Stroke> Strokes;

class HandwritingInterface {
 public:
  HandwritingInterface() {}
  virtual ~HandwritingInterface() {}

  virtual void Recognize(const Strokes &strokes,
                         vector<string> *candidates) const ABSTRACT;

  virtual void Commit(const Strokes &strokes, const string &result) ABSTRACT;

 private:
  DISALLOW_COPY_AND_ASSIGN(HandwritingInterface);
};

class HandwritingManager {
 public:
  // Add a handwriting module inheriting HandwritingInterface.  The
  // owner of the module should be the caller of this function.
  static void AddHandwritingModule(HandwritingInterface *module);
  static void ClearHandwritingModules();

  static void Recognize(const Strokes &strokes, vector<string> *candidates);
  static void Commit(const Strokes &strokes, const string &result);

 private:
  HandwritingManager() {}
  virtual ~HandwritingManager() {}
};

}  // namespace handwriting
}  // namespace mozc

#endif  // MOZC_HANDWRITING_HANDWRITING_MANAGER_H_
