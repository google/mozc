// Copyright 2010, Google Inc.
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

#ifndef MOZC_CONVERTER_CONVERTER_COMPILER_H_
#define MOZC_CONVERTER_CONVERTER_COMPILER_H_

#include <iomanip>  // ostream
#include <vector>
#include <string>

namespace mozc {

class ConverterCompiler {
 public:
  static void CompileConnectionTable(const string &input,
                                     const string &output);

  static void CompileDictionary(const string &input,
                                const string &output);

  // read binary dictionary/connection table,
  // output C/C++ header file like
  //
  // static const char $(name)_data[] = "...";
  // static const size_t = 1000;
  static void MakeHeaderFile(const string &name,
                             const string &input,
                             const string &output);

  static void MakeHeaderStream(const string &name,
                               const string &input,
                               ostream *os);

  static void MakeHeaderStreamFromArray(const string &name,
                                        const char *image,
                                        size_t image_size,
                                        ostream *os);

};
}  // mozc
#endif  // MOZC_CONVERTER_CONVERTER_COMPILER_H_
