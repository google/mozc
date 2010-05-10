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

#include <string>
#include <vector>
#include "base/base.h"
#include "base/mmap.h"
#include "base/file_stream.h"
#include "converter/connector.h"
#include "converter/converter_compiler.h"
#include "dictionary/dictionary.h"

namespace mozc {

void ConverterCompiler::CompileConnectionTable(const string &input,
                                               const string &output) {
  ConnectorInterface::Compile(input.c_str(), output.c_str());
}

void ConverterCompiler::CompileDictionary(const string &input,
                                          const string &output) {
  Dictionary::Compile(Dictionary::SYSTEM, input.c_str(), output.c_str());
}

void ConverterCompiler::MakeHeaderFile(const string &name,
                                       const string &input,
                                       const string &output) {
  OutputFileStream ofs(output.c_str());
  CHECK(ofs);
  ConverterCompiler::MakeHeaderStream(name, input, &ofs);
}

void ConverterCompiler::MakeHeaderStream(const string &name,
                                         const string &input,
                                         ostream *os) {
  Mmap<char> mmap;
  CHECK(mmap.Open(input.c_str()));
  MakeHeaderStreamFromArray(name, mmap.begin(), mmap.GetFileSize(), os);
}

void ConverterCompiler::MakeHeaderStreamFromArray(const string &name,
                                                  const char *image,
                                                  size_t image_size,
                                                  ostream *os) {
  const char *begin = image;
  const char *end = image + image_size;

  *os << "static const size_t k" << name << "_size = "
      << image_size << ";" << endl;

  // WINDOWS dose not accept static string of size >= 65536,
  // so we represent string in an array of uint64
#ifdef OS_WINDOWS
  *os << "static const uint64 k" << name << "_data[] = {"  << endl;
  os->setf(ios::hex, ios::basefield);   // in hex
  os->setf(ios::showbase);              // add 0x
  int num = 0;
  while (begin < end) {
    uint64 n = 0;
    uint8 *buf = reinterpret_cast<uint8 *>(&n);
    const size_t size = min(static_cast<size_t>(end - begin),
                            static_cast<size_t>(8));
    for (size_t i = 0; i < size; ++i) {
      buf[i] = static_cast<uint8>(begin[i]);
    }
    begin += 8;
    *os << n << ", ";
    if (++num % 8 == 0) {
      *os << endl;
    }
  }
  *os << "};" << endl;
#else
  *os << "static const char k" << name << "_data[] ="  << endl;
  static const size_t kBucketSize = 20;
  while (begin < end) {
    const size_t size = min(static_cast<size_t>(end - begin), kBucketSize);
    string buf;
    Util::Escape(string(begin, size), &buf);
    *os << "\"" << buf << "\"";
    *os << endl;
    begin += kBucketSize;
  }
  *os << ";" << endl;
#endif

  return;
}
}  // mozc
