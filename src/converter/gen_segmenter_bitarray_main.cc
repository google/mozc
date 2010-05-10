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

#include <map>
#include <vector>
#include <string>
#include "base/base.h"
#include "base/bitarray.h"
#include "base/file_stream.h"
#include "converter/converter_compiler.h"
#include "converter/segmenter_inl.h"

DEFINE_string(output, "", "output header filename");
DECLARE_bool(logtostderr);

namespace {
class StateTable {
 public:
  StateTable(const size_t size) : compressed_size_(0) {
    idarray_.resize(size);
  }

  // |str| is an 1-dimentional row (or column) represented in byte array.
  void Add(uint16 id, const string &str) {
    CHECK_LT(id, idarray_.size());
    idarray_[id] = str;
  }

  void Build() {
    compressed_table_.resize(idarray_.size());
    uint16 id = 0;
    map<string, uint16> dup;
    for (size_t i = 0; i < idarray_.size(); ++i) {
      map<string, uint16>::const_iterator it = dup.find(idarray_[i]);
      if (it != dup.end()) {
        compressed_table_[i] = it->second;
      } else {
        compressed_table_[i] = id;
        dup.insert(make_pair(idarray_[i], id));
        ++id;
      }
    }

    compressed_size_ = dup.size();

    // verify
    for (size_t i = 0; i < idarray_.size(); ++i) {
      CHECK_LT(compressed_table_[i], compressed_size_);
      CHECK_EQ(dup[idarray_[i]], compressed_table_[i]);
    }

    CHECK_LT(compressed_size_, idarray_.size());
  }

  uint16 id(uint16 id) const {
    CHECK_LT(id, idarray_.size());
    return compressed_table_[id];
  }

  size_t compressed_size() const {
    return compressed_size_;
  }

  void Output(const string &name, ostream *os) {
    if (compressed_size_ < 256) {
      // trivial compression -- use uint8 if possible
      *os << "const uint8 " << name << "[] = {" << endl;
    } else {
      *os << "const uint16 " << name << "[] = {" << endl;
    }
    for (size_t i = 0; i < compressed_table_.size(); ++i) {
      *os << compressed_table_[i];
      if (i < compressed_table_.size() - 1) {
        *os << ",";
      }
      *os << endl;
    }
    *os << "};" << endl;
  }

 private:
  vector<string> idarray_;
  vector<uint16> compressed_table_;
  size_t compressed_size_;
};
}  // namespace

int main(int argc, char **argv) {
  FLAGS_logtostderr = true;
  InitGoogle(argv[0], &argc, &argv, true);

  // Load the original matrix into an array
  vector<uint8> array((kLSize  + 1) * (kRSize + 1));

  for (size_t rid = 0; rid <= kLSize; ++rid) {
    for (size_t lid = 0; lid <= kRSize; ++lid) {
      const uint32 index = rid + kLSize * lid;
      CHECK_LT(index, array.size());
      if (rid == kLSize || lid == kRSize) {
        array[index] = 1;
        continue;
      }
      if (IsBoundaryInternal(rid, lid)) {
        array[index] = 1;
      } else {
        array[index] = 0;
      }
    }
  }

  // Reduce left states (remove dupliacate rows)
  StateTable ltable(kLSize + 1);
  for (size_t rid = 0; rid <= kLSize; ++rid) {
    string buf;
    for (size_t lid = 0; lid <= kRSize; ++lid) {
      const uint32 index = rid + kLSize * lid;
      buf += array[index];
    }
    ltable.Add(rid, buf);
  }

  // Reduce right states (remove dupliacate columns)
  StateTable rtable(kRSize + 1);
  for (size_t lid = 0; lid <= kRSize; ++lid) {
    string buf;
    for (size_t rid = 0; rid <= kLSize; ++rid) {
      const uint32 index = rid + kLSize * lid;
      buf += array[index];
    }
    rtable.Add(lid, buf);
  }

  // make lookup table
  rtable.Build();
  ltable.Build();

  const size_t kCompressedLSize = ltable.compressed_size();
  const size_t kCompressedRSize = rtable.compressed_size();

  CHECK_GT(kCompressedLSize, 0);
  CHECK_GT(kCompressedRSize, 0);

  // make bitarray
  mozc::BitArray barray(kCompressedLSize * kCompressedRSize);
  for (size_t rid = 0; rid <= kLSize; ++rid) {
    for (size_t lid = 0; lid <= kRSize; ++lid) {
      const int index = rid + kLSize * lid;
      const uint32 cindex = ltable.id(rid) + kCompressedLSize * rtable.id(lid);
      if (array[index] > 0) {
        barray.set(cindex);
      } else {
        barray.clear(cindex);
      }
    }
  }

  // verify the table
  for (size_t rid = 0; rid <= kLSize; ++rid) {
    for (size_t lid = 0; lid <= kRSize; ++lid) {
      const int index= rid + kLSize * lid;
      const uint32 cindex = ltable.id(rid) + kCompressedLSize * rtable.id(lid);
      CHECK_EQ(barray.get(cindex), array[index]);
    }
  }

  CHECK(barray.array());
  CHECK_GT(barray.size(), 0);

  mozc::OutputFileStream ofs(FLAGS_output.c_str());
  CHECK(ofs);

  ofs << "const size_t kCompressedLSize = " << kCompressedLSize << ";" << endl;
  ofs << "const size_t kCompressedRSize = " << kCompressedRSize << ";" << endl;
  ltable.Output("kCompressedLIDTable", &ofs);
  rtable.Output("kCompressedRIDTable", &ofs);

  const char kBitArrayName[] = "SegmenterBitArrayData";
  mozc::ConverterCompiler::MakeHeaderStreamFromArray(
      kBitArrayName,
      reinterpret_cast<const char *>(barray.array()),
      barray.array_size(),
      &ofs);

  return 0;
}
