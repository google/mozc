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

#include "base/hash.h"
#include "base/logging.h"
#include "base/port.h"
#include "base/util.h"
#include "dictionary/file/codec_interface.h"
#include "dictionary/file/codec_util.h"
#include "dictionary/file/section.h"

namespace mozc {
namespace dictionary {

DictionaryFileCodec::DictionaryFileCodec()
    : seed_(2135654146), filemagic_(20110701) {}

DictionaryFileCodec::~DictionaryFileCodec() = default;

void DictionaryFileCodec::WriteSections(
    const std::vector<DictionaryFileSection> &sections,
    std::ostream *ofs) const {
  DCHECK(ofs);
  WriteHeader(ofs);

  if (sections.size() == 4) {
    // In production, the number of sections equals 4.  In this case, write the
    // sections in the following deterministic order.  This order was determined
    // by random shuffle for engine version 24 but it's now made deterministic
    // to obsolte DictionaryFileCodec.
    for (size_t i : {0, 2, 1, 3}) {
      WriteSection(sections[i], ofs);
    }
  } else {
    // Some tests don't have four sections.  In this case, simply write sections
    // in given order.
    for (const auto &section : sections) {
      WriteSection(section, ofs);
    }
  }

  filecodec_util::WriteInt(0, ofs);
}

void DictionaryFileCodec::WriteHeader(std::ostream *ofs) const {
  DCHECK(ofs);
  filecodec_util::WriteInt(filemagic_, ofs);
  filecodec_util::WriteInt(seed_, ofs);
}

void DictionaryFileCodec::WriteSection(const DictionaryFileSection &section,
                                       std::ostream *ofs) const {
  DCHECK(ofs);
  const string &name = section.name;
  // name should be encoded
  // uint64 needs just 8 bytes.
  DCHECK_EQ(8, name.size());
  string escaped;
  Util::Escape(name, &escaped);
  VLOG(1) << "section=" << escaped << " length=" << section.len;
  filecodec_util::WriteInt(section.len, ofs);
  ofs->write(name.data(), name.size());

  ofs->write(section.ptr, section.len);
  Pad4(section.len, ofs);
}

void DictionaryFileCodec::Pad4(int length, std::ostream *ofs) {
  DCHECK(ofs);
  for (int i = length; (i % 4) != 0; ++i) {
    (*ofs) << '\0';
  }
}

string DictionaryFileCodec::GetSectionName(const string &name) const {
  VLOG(1) << "seed\t" << seed_;
  const uint64 name_fp = Hash::FingerprintWithSeed(name, seed_);
  const string fp_string(reinterpret_cast<const char *>(&name_fp),
                         sizeof(name_fp));
  string escaped;
  Util::Escape(fp_string, &escaped);
  VLOG(1) << "Section name for " << name << ": " << escaped;
  return fp_string;
}

bool DictionaryFileCodec::ReadSections(
    const char *image, int length,
    std::vector<DictionaryFileSection> *sections) const {
  DCHECK(sections);
  const char *ptr = image;
  const int filemagic = filecodec_util::ReadInt(ptr);
  CHECK(filemagic == filemagic_)
      << "invalid dictionary file magic (recompile dictionary?)";
  ptr += sizeof(filemagic);
  seed_ = filecodec_util::ReadInt(ptr);
  ptr += sizeof(seed_);
  int size;
  while ((size = filecodec_util::ReadInt(ptr))) {
    ptr += sizeof(size);
    // finger print name
    const string name(ptr, sizeof(uint64));
    ptr += sizeof(uint64);

    string escaped;
    Util::Escape(name, &escaped);
    VLOG(1) << "section=" << escaped << " length=" << size;

    sections->push_back(DictionaryFileSection(ptr, size, name));

    ptr += size;
    ptr += filecodec_util::Rup4(size);
    if (image + length < ptr) {
      return false;
    }
  }
  return true;
}

}  // namespace dictionary
}  // namespace mozc
