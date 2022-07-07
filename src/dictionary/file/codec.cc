// Copyright 2010-2021, Google Inc.
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

#include <cstdint>
#include <iterator>
#include <ostream>
#include <string>
#include <vector>

#include "base/hash.h"
#include "base/logging.h"
#include "base/port.h"
#include "base/util.h"
#include "dictionary/file/codec_interface.h"
#include "dictionary/file/codec_util.h"
#include "dictionary/file/section.h"
#include "absl/status/status.h"

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

  filecodec_util::WriteInt32(0, ofs);
}

void DictionaryFileCodec::WriteHeader(std::ostream *ofs) const {
  DCHECK(ofs);
  filecodec_util::WriteInt32(filemagic_, ofs);
  filecodec_util::WriteInt32(seed_, ofs);
}

void DictionaryFileCodec::WriteSection(const DictionaryFileSection &section,
                                       std::ostream *ofs) const {
  DCHECK(ofs);
  const std::string &name = section.name;
  // name should be encoded
  // uint64 needs just 8 bytes.
  DCHECK_EQ(8, name.size());
  if (VLOG_IS_ON(1)) {
    std::string escaped;
    Util::Escape(name, &escaped);
    LOG(INFO) << "section=" << escaped << " length=" << section.len;
  }
  filecodec_util::WriteInt32(section.len, ofs);
  ofs->write(name.data(), name.size());
  ofs->write(section.ptr, section.len);
  filecodec_util::Pad4(section.len, ofs);
}

std::string DictionaryFileCodec::GetSectionName(const std::string &name) const {
  VLOG(1) << "seed\t" << seed_;
  const uint64_t name_fp = Hash::FingerprintWithSeed(name, seed_);
  const std::string fp_string(reinterpret_cast<const char *>(&name_fp),
                              sizeof(name_fp));
  std::string escaped;
  Util::Escape(fp_string, &escaped);
  VLOG(1) << "Section name for " << name << ": " << escaped;
  return fp_string;
}

absl::Status DictionaryFileCodec::ReadSections(
    const char *image, int length,
    std::vector<DictionaryFileSection> *sections) const {
  DCHECK(sections);
  if (image == nullptr) {
    return absl::InvalidArgumentError("codec.cc: Image is nullptr");
  }
  // At least 12 bytes (3 * int32) are required.
  if (length < 12) {
    return absl::FailedPreconditionError(
        absl::StrCat("codec.cc: Insufficient data size: ", length, " bytes"));
  }
  // The image must be aligned at 32-bit boundary.
  if (reinterpret_cast<std::uintptr_t>(image) % 4 != 0) {
    return absl::FailedPreconditionError(
        absl::StrCat("codec.cc: memory block of size ", length,
                     " is not aligned at 32-bit boundary"));
  }
  const char *ptr = image;  // The current position at which data is read.
  const char *const image_end = image + length;

  const int32_t filemagic = filecodec_util::ReadInt32ThenAdvance(&ptr);
  if (filemagic != filemagic_) {
    return absl::FailedPreconditionError(absl::StrCat(
        "codec.cc: Invalid dictionary file magic. Expected: ", filemagic_,
        " Actual: ", filemagic));
  }
  seed_ = filecodec_util::ReadInt32ThenAdvance(&ptr);
  for (int section_index = 0;; ++section_index) {
    // Each section has the following format:
    // +-----------+-------------+-----------------+---------------+
    // |   int32   |   char[8]   | char[data_size] | up to 3 bytes |
    // | data_size | fingerprint |      data       |   padding     |
    // +-----------+-------------+-----------------+---------------+
    // ^                         <- - - - padded_data_size - - - - >
    // ptr points to here now.
    if (std::distance(ptr, image_end) < 4) {
      return absl::OutOfRangeError(absl::StrCat(
          "codec.cc: Section ", section_index,
          ": Insufficient image to read data_size(4 bytes), available size = ",
          std::distance(ptr, image_end)));
    }
    const int32_t data_size = filecodec_util::ReadInt32ThenAdvance(&ptr);
    if (data_size == 0) {  // The end marker written in WriteSections().
      break;
    }
    // Calculate the section end pointer.  Note that |ptr| currently points to
    // the beginning of fingerprint.
    constexpr size_t kFingerprintByteLength = 8;
    const auto padded_data_size = filecodec_util::RoundUp4(data_size);
    const auto *section_end = ptr + kFingerprintByteLength + padded_data_size;
    if (section_end > image_end) {
      return absl::OutOfRangeError(absl::StrCat(
          "codec.cc: Section ", section_index,
          ": Read pointer will pass the end: offset=", section_end - image,
          ", image_size=", length));
    }
    const std::string fingerprint(ptr, kFingerprintByteLength);
    ptr += kFingerprintByteLength;
    if (VLOG_IS_ON(1)) {
      std::string escaped;
      Util::Escape(fingerprint, &escaped);
      LOG(INFO) << "section=" << escaped << " data_size=" << data_size;
    }
    // Add a section with data and fingerprint.  Note that the data size is
    // |data_size| but |ptr| is advanced by |padded_data_size| to skip padding
    // bytes at the end.
    sections->emplace_back(ptr, data_size, fingerprint);
    ptr += padded_data_size;
  }
  if (ptr != image_end) {
    return absl::FailedPreconditionError(absl::StrCat(
        "codec.cc: ", image_end - ptr, " bytes remaining out of ", length));
  }
  return absl::Status();
}

}  // namespace dictionary
}  // namespace mozc
