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

// This code manages ar/cpio/tar like file structure that contains
// multiple sections in a file. Each section has a name and size.

#ifndef MOZC_DICTIONARY_DICTIONARY_FILE_H_
#define MOZC_DICTIONARY_DICTIONARY_FILE_H_

#include <string>
#include <list>
#include "base/base.h"
#include "base/file_stream.h"
#include "base/mmap.h"

namespace mozc {

class OutputFileStream;

struct DictionaryFileSection {
  const char *ptr;
  int len;
  string name;
  bool isExternal;
};

class DictionaryFile {
 public:
  DictionaryFile();
  virtual ~DictionaryFile();

  bool Open(const char* fn, bool to_write);
  bool SetPtr(const char* ptr, int len);

  void AddSection(const char* sec, const char* fn);
  void Write();

  const char* GetSection(const char* sec, int *len);
 private:
  bool ScanSections();

  void WriteSection(OutputFileStream &ofs, DictionaryFileSection *s);
  void WriteInt(OutputFileStream &ofs, int n);
  int ReadInt(const char *ptr);
  // Write padding bytes.
  void Pad4(OutputFileStream &ofs, int len);
  // Round up by 4 bytes.
  int Rup4(int len);

  // This will be NULL if the mapping source is given as a pointer.
  scoped_ptr<Mmap<char> > mapping_;

  const char *ptr_;
  int mapping_len_;

  list<DictionaryFileSection *> sections_;
  string filename_;
  static const int filemagic_ = 20080808;
};
}

#endif  // MOZC_DICTIONARY_DICTIONARY_FILE_H_
