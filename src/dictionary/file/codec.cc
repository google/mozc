// Copyright 2010-2018, Google Inc.
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

#include "dictionary/file/codec.h"

#include <climits>

#include "base/logging.h"
#include "base/port.h"
#include "base/util.h"
#include "dictionary/file/codec_interface.h"
#include "dictionary/file/codec_util.h"
#include "dictionary/file/section.h"

namespace mozc {
namespace dictionary {

DictionaryFileCodec::DictionaryFileCodec() : filemagic_(20110701) {}

DictionaryFileCodec::~DictionaryFileCodec() {}

void DictionaryFileCodec::WriteSections(
    const std::vector<DictionaryFileSection> &sections,
    std::ostream *ofs) const {
  DCHECK(ofs);
  WriteHeader(ofs);
  for (size_t i = 0; i < sections.size(); ++i) {
    WriteSection(sections[i], ofs);
  }
  filecodec_util::WriteInt(0, ofs);
}

void DictionaryFileCodec::WriteHeader(std::ostream *ofs) const {
  DCHECK(ofs);
  filecodec_util::WriteInt(filemagic_, ofs);
}

void DictionaryFileCodec::WriteSection(const DictionaryFileSection &section,
                                       std::ostream *ofs) const {
  DCHECK(ofs);
  const string &name = GetSectionName(section.name);
  VLOG(1) << "section=" << name << " length=" << section.len;
  filecodec_util::WriteInt(section.len, ofs);

  const int name_len = name.size() + 1;  // including '\0'
  ofs->write(name.data(), name_len);
  Pad4(name_len, ofs);

  ofs->write(section.ptr, section.len);
  Pad4(section.len, ofs);
}

string DictionaryFileCodec::GetSectionName(const string &name) const {
  // Use the given string as is
  return name;
}

bool DictionaryFileCodec::ReadSections(
    const char *image, int length,
    std::vector<DictionaryFileSection> *sections) const {
  DCHECK(sections);
  const char *ptr = image;
  const int filemagic = filecodec_util::ReadInt(ptr);
  CHECK(filemagic == filemagic_) <<
      "invalid dictionary file magic (recompile dictionary?)";
  ptr += sizeof(filemagic);

  int size;
  while ((size = filecodec_util::ReadInt(ptr))) {
    ptr += sizeof(size);
    const string name(ptr);
    VLOG(1) << "section=" << name << " length=" << size;
    const int name_len = name.size() + 1;
    ptr += name_len;
    ptr += filecodec_util::Rup4(name_len);

    sections->push_back(DictionaryFileSection(ptr, size, name));

    ptr += size;
    ptr += filecodec_util::Rup4(size);
    if (image + length < ptr) {
      return false;
    }
  }
  return true;
}

// Write padding
void DictionaryFileCodec::Pad4(int length, std::ostream *ofs) {
  DCHECK(ofs);
  for (int i = length; (i % 4) != 0; ++i) {
    (*ofs) << static_cast<char>(Util::Random(CHAR_MAX));
  }
}

}  // namespace dictionary
}  // namespace mozc
