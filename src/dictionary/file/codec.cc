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

#include "dictionary/file/codec.h"

#include <algorithm>
#include <climits>

#include "base/base.h"
#include "base/file_stream.h"
#include "base/singleton.h"
#include "base/util.h"
#include "dictionary/file/codec_interface.h"
#include "dictionary/file/section.h"


namespace mozc {
namespace {
void WriteInt(int value, OutputFileStream *ofs) {
  DCHECK(ofs);
  ofs->write(reinterpret_cast<const char *>(&value), sizeof(value));
}

// Write padding
void Pad4(int length, OutputFileStream *ofs) {
  DCHECK(ofs);
  for (int i = length; (i % 4) != 0; ++i) {
    (*ofs) << static_cast<char>(Util::Random(CHAR_MAX));
  }
}

int ReadInt(const char *ptr) {
  DCHECK(ptr);
  return *reinterpret_cast<const int *>(ptr);
}

// Round up
int Rup4(int length) {
  const int rem = (length % 4);
  return ((4 - rem) % 4);
}
}  // namespace


DictionaryFileCodec::DictionaryFileCodec() : filemagic_(20110701) {}

DictionaryFileCodec::~DictionaryFileCodec() {}

void DictionaryFileCodec::WriteSections(
    const vector<DictionaryFileSection> &sections,
    OutputFileStream *ofs) const {
  DCHECK(ofs);
  WriteHeader(ofs);
  for (size_t i = 0; i < sections.size(); ++i) {
    WriteSection(sections[i], ofs);
  }
  WriteInt(0, ofs);
}

void DictionaryFileCodec::WriteHeader(OutputFileStream *ofs) const {
  DCHECK(ofs);
  WriteInt(filemagic_, ofs);
}

void DictionaryFileCodec::WriteSection(
    const DictionaryFileSection &section, OutputFileStream *ofs) const {
  DCHECK(ofs);
  const string &name = GetSectionName(section.name);
  VLOG(1) << "section=" << name << " length=" << section.len;
  WriteInt(section.len, ofs);

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
    vector<DictionaryFileSection> *sections) const {
  DCHECK(sections);
  const char *ptr = image;
  const int filemagic = ReadInt(ptr);
  CHECK(filemagic == filemagic_) <<
      "invalid dictionary file magic (recompile dictionary?)";
  ptr += sizeof(filemagic);

  int size;
  while ((size = ReadInt(ptr))) {
    ptr += sizeof(size);
    string name(ptr);
    VLOG(1) << "section=" << name << " length=" << size;
    const int name_len = name.size() + 1;
    ptr += name_len;
    ptr += Rup4(name_len);

    sections->resize(sections->size() + 1);
    sections->back().len = size;
    sections->back().ptr = ptr;
    sections->back().name = name;

    ptr += size;
    ptr += Rup4(size);
    if (image + length < ptr) {
      return false;
    }
  }
  return true;
}

namespace {
DictionaryFileCodecInterface *g_dictionary_file_codec = NULL;
}  // namespace

typedef DictionaryFileCodec DefaultCodec;

DictionaryFileCodecInterface *DictionaryFileCodecFactory::GetCodec() {
  if (g_dictionary_file_codec == NULL) {
    return Singleton<DefaultCodec>::get();
  } else {
    return g_dictionary_file_codec;
  }
}

void DictionaryFileCodecFactory::SetCodec(DictionaryFileCodecInterface *codec) {
  g_dictionary_file_codec = codec;
}

}  // namespace mozc
