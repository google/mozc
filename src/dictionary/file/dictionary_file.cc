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

// TODO(tabata): little endian support?

#include "dictionary/file/dictionary_file.h"

#include <stdlib.h>
#include <list>
#include <string>
#include "base/base.h"
#include "base/file_stream.h"
#include "base/mmap.h"

namespace mozc {

DictionaryFile::DictionaryFile() {
}

DictionaryFile::~DictionaryFile() {
  list<DictionaryFileSection *>::iterator it;
  for (it = sections_.begin(); it != sections_.end(); it++) {
    DictionaryFileSection *s = *it;
    if (s->isExternal) {
      delete [] s->ptr;
    }
    delete s;
  }
}

bool DictionaryFile::Open(const char* fn, bool to_write) {
  filename_ = string(fn);
  mapping_.reset(new Mmap<char>());
  if (!to_write) {
    mapping_->Open(filename_.c_str());
    mapping_len_ = mapping_->GetFileSize();
    ptr_ = mapping_->begin();
    return ScanSections();
  }
  return true;
}

bool DictionaryFile::SetPtr(const char *ptr, int len) {
  ptr_ = ptr;
  mapping_len_ = len;
  return ScanSections();
}

void DictionaryFile::AddSection(const char* sec, const char* fn) {
  InputFileStream ifs(fn, ios::binary);
  CHECK(ifs) << "Failed to read" << fn;
  DictionaryFileSection* s = new DictionaryFileSection();
  s->isExternal = true;
  ifs.seekg(0, ios::end);
  s->len = ifs.tellg();
  s->name = string(sec);
  // reads the file
  ifs.seekg(0, ios::beg);
  char *ptr = new char[s->len];
  ifs.read(ptr, s->len);
  s->ptr = ptr;
  // pushes back this section
  sections_.push_back(s);
}

const char *DictionaryFile::GetSection(const char *sec, int *len) {
  list<DictionaryFileSection *>::iterator it;
  for (it = sections_.begin(); it != sections_.end(); it++) {
    if (string(sec) == (*it)->name) {
      *len = (*it)->len;
      return (*it)->ptr;
    }
  }
  return NULL;
}

void DictionaryFile::Write() {
  OutputFileStream ofs(filename_.c_str(), ios::binary);
  WriteInt(ofs, filemagic_);
  list<DictionaryFileSection *>::iterator it;
  for (it = sections_.begin(); it != sections_.end(); it++) {
    WriteSection(ofs, *it);
  }
  WriteInt(ofs, 0);
}

void DictionaryFile::WriteSection(OutputFileStream &ofs,
                                  DictionaryFileSection *s) {
  LOG(INFO) << " section=" << s->name << " length=" << s->len;
  WriteInt(ofs, s->len);
  // Writes s->name including the last '\0'
  int name_len = s->name.size() + 1;
  ofs.write(s->name.c_str(), name_len);
  Pad4(ofs, name_len);

  ofs.write(s->ptr, s->len);
  Pad4(ofs, s->len);
}

void DictionaryFile::Pad4(OutputFileStream &ofs, int n) {
  int s = (n % 4);
  if (!s) {
    return ;
  }
  for (int i = 4 - s; i > 0; i--) {
    ofs << '\0';
  }
}

int DictionaryFile::Rup4(int n) {
  int s = (n % 4);
  if (!s) {
    return 0;
  }
  return (4 - s);
}

void DictionaryFile::WriteInt(OutputFileStream &ofs, int n) {
  ofs.write(reinterpret_cast<const char *>(&n), sizeof(int));
}

int DictionaryFile::ReadInt(const char *p) {
  return *reinterpret_cast<const int *>(p);
}

bool DictionaryFile::ScanSections() {
  VLOG(1) << "ScanSections";
  const char *p = ptr_;
  int size;
  CHECK(ReadInt(p) == filemagic_)
      << "dictionary file magic do not match (recompile dictionary?)";
  p += 4;
  while ((size = ReadInt(p))) {
    p += 4;
    string name(p);
    VLOG(1) << " size=" << size << ":name=" << name;
    int len = static_cast<int>(strlen(p) + 1);
    p += len;
    p += Rup4(len);
    DictionaryFileSection *sec = new DictionaryFileSection;
    sec->len = size;
    sec->ptr = p;
    sec->name = name;
    sec->isExternal = false;
    sections_.push_back(sec);
    p += size;
    p += Rup4(size);
    if (ptr_ + mapping_len_ < p) {
      return false;
    }
  }
  return true;
}
}
