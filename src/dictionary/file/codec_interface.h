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

#ifndef MOZC_DICTIONARY_FILE_CODEC_INTERFACE_H_
#define MOZC_DICTIONARY_FILE_CODEC_INTERFACE_H_

#include <ostream>
#include <vector>

#include "base/base.h"
#include "dictionary/file/section.h"

namespace mozc {

class DictionaryFileCodecInterface {
 public:
  // Write sections to output file stream
  virtual void WriteSections(const vector<DictionaryFileSection> &sections,
                             ostream *ofs) const = 0;

  // Read sections from memory image
  virtual bool ReadSections(const char *image, int length,
                            vector<DictionaryFileSection> *sections) const = 0;

  // Get section name
  virtual string GetSectionName(const string &name) const = 0;

 protected:
  DictionaryFileCodecInterface() {}
  virtual ~DictionaryFileCodecInterface() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DictionaryFileCodecInterface);
};

class DictionaryFileCodecFactory {
 public:
  // return singleton object
  static DictionaryFileCodecInterface *GetCodec();

  // dependency injectin for unittesting
  static void SetCodec(DictionaryFileCodecInterface *codec);

 private:
  DictionaryFileCodecFactory() {}
  ~DictionaryFileCodecFactory() {}
};

}  // namespace mozc

#endif  // MOZC_DICTIONARY_FILE_CODEC_INTERFACE_H_
