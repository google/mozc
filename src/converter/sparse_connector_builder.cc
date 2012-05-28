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

#include "converter/sparse_connector_builder.h"

#include <vector>

#include "base/base.h"
#include "base/file_stream.h"
#include "base/mmap.h"
#include "converter/sparse_connector.h"
#include "storage/sparse_array_image.h"

DEFINE_bool(use_1byte_cost, false,
            "Cost is encoded int a byte, instead of 2 bytes.");
DEFINE_int32(cost_resolution, 64,
             "Cost value is calculated by the value in SparseArray * "
             "cost_resolution. So every cost value should be smaller than "
             "resolution*256.");

namespace mozc {
namespace converter {
namespace {

size_t LoadIDSize(const string &filename) {
  InputFileStream ifs(filename.c_str());
  CHECK(ifs);
  string line;
  vector<string> fields;
  int max_id = 0;
  int line_num = 0;
  while (getline(ifs, line)) {
    fields.clear();
    Util::SplitStringUsing(line, "\t ", &fields);
    CHECK_GE(fields.size(), 2);
    const int id = atoi32(fields[0].c_str());
    max_id = max(id, max_id);
    ++line_num;
  }
  ++max_id;

  CHECK_EQ(line_num, max_id);

  return max_id;
}

size_t LoadSpecialPOSSize(const string &filename) {
  InputFileStream ifs(filename.c_str());
  CHECK(ifs);
  string line;
  int line_num = 0;
  while (getline(ifs, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    ++line_num;
  }

  return line_num;
}
}  // namespace

SparseConnectorBuilder::SparseConnectorBuilder() {
}
SparseConnectorBuilder::~SparseConnectorBuilder() {
}

void SparseConnectorBuilder::SetIdFile(const string &id_file) {
  id_size_ = LoadIDSize(id_file);
}
void SparseConnectorBuilder::SetSpecialPosFile(
    const string &special_pos_file) {
  special_pos_size_ = LoadSpecialPOSSize(special_pos_file);
}
void SparseConnectorBuilder::ParseTextConnectionFile(
    const string &text_connection_file,
    vector<int16> *matrix) const {
  InputFileStream ifs(text_connection_file.c_str());
  CHECK(ifs);

  string line;
  vector<string> fields;
  getline(ifs, line);
  Util::SplitStringUsing(line, "\t ", &fields);
  CHECK_GE(fields.size(), 2);

  CHECK_EQ(id_size_, atoi32(fields[0].c_str()));
  CHECK_EQ(id_size_, atoi32(fields[1].c_str()));
  const uint16 matrix_size = MatrixSize();

  LOG(INFO) << "Making " << matrix_size << "x" << matrix_size << " matrix.";
  matrix->assign(matrix_size * matrix_size, 0);

  while (getline(ifs, line)) {
    fields.clear();
    Util::SplitStringUsing(line, "\t ", &fields);
    CHECK_GE(fields.size(), 3);
    const int lid = atoi32(fields[0].c_str());
    const int rid = atoi32(fields[1].c_str());
    int cost = atoi32(fields[2].c_str());
    CHECK(lid < matrix_size && rid < matrix_size)
        << "index values are out of range: "
        << lid << ", " << rid << ", " << matrix_size;
    // BOS->EOS connection cost is always 0
    if (lid == 0 && rid == 0) {
      cost = 0;
    }
    (*matrix)[lid + matrix_size * rid] = static_cast<int16>(cost);
  }

  for (int lid = id_size_; lid < matrix_size; ++lid) {
    for (int rid = 1; rid < matrix_size; ++rid) {   // SKIP EOS (rid == 0)
      CHECK(lid < matrix_size && rid < matrix_size)
          << "index values are out of range: "
          << lid << ", " << rid << ", " << matrix_size;
      (*matrix)[lid + matrix_size * rid] = ConnectorInterface::kInvalidCost;
    }
  }

  for (int rid = id_size_; rid < matrix_size; ++rid) {
    for (int lid = 1; lid < matrix_size; ++lid) {   // SKIP BOS (lid == 0)
      CHECK(lid < matrix_size && rid < matrix_size)
          << "index values are out of range: "
          << lid << ", " << rid << ", " << matrix_size;
      (*matrix)[lid + matrix_size * rid] = ConnectorInterface::kInvalidCost;
    }
  }
}

void SparseConnectorBuilder::FillDefaultCost(const vector<int16> &matrix) {
  size_t matrix_size = MatrixSize();
  CHECK_EQ(matrix_size * matrix_size, matrix.size());

  // Initialized by filling 0
  default_cost_.assign(matrix_size, 0);
  for (int lid = 0; lid < matrix_size; ++lid) {
    for (int rid = 0; rid < matrix_size; ++rid) {
      const int16 c = matrix[lid + matrix_size * rid];
      if (ConnectorInterface::kInvalidCost != c) {
        default_cost_[lid] = max(default_cost_[lid], c);  // save default cost
      }
    }
  }
}

void SparseConnectorBuilder::Build(const string &text_connection_file) {
  BuildInternal(text_connection_file, NULL);
}

void SparseConnectorBuilder::BuildInternal(
    const string &text_connection_file,
    vector<int16> *output_matrix) {
  vector<int16> matrix;
  ParseTextConnectionFile(text_connection_file, &matrix);
  FillDefaultCost(matrix);

  LOG(INFO) << "compiling matrix with " << matrix.size();

  builder_.reset(new SparseArrayBuilder);
  if (FLAGS_use_1byte_cost) {
    builder_->SetUse1ByteValue(true);
  }

  size_t matrix_size = MatrixSize();
  for (int lid = 0; lid < matrix_size; ++lid) {
    for (int rid = 0; rid < matrix_size; ++rid) {
      int16 c = matrix[lid + matrix_size * rid];
      if (c != default_cost_[lid]) {
        if (FLAGS_use_1byte_cost) {
          if (c == ConnectorInterface::kInvalidCost) {
            c = SparseConnector::kInvalid1ByteCostValue;
          } else {
            c = c / FLAGS_cost_resolution;
            CHECK(c < 256 && c != SparseConnector::kInvalid1ByteCostValue);
          }
        }
        builder_->AddValue(SparseConnector::EncodeKey(lid, rid), c);
      }
    }
  }

  builder_->Build();
  if (output_matrix != NULL) {
    output_matrix->swap(matrix);
  }
}

void SparseConnectorBuilder::WriteTo(const string& output_file) const {
  CHECK(builder_.get() != NULL);

  OutputFileStream ofs(output_file.c_str(), ios::binary|ios::out);
  CHECK(ofs) << "permission denied: " << output_file;

  const uint16 magic = SparseConnector::kSparseConnectorMagic;
  ofs.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
  uint16 resolution = 1;
  if (FLAGS_use_1byte_cost) {
    resolution = FLAGS_cost_resolution;
  }

  const uint16 matrix_size = MatrixSize();
  CHECK_EQ(matrix_size, default_cost_.size());

  ofs.write(reinterpret_cast<const char*>(&resolution), sizeof(resolution));
  ofs.write(reinterpret_cast<const char*>(&matrix_size), sizeof(matrix_size));
  ofs.write(reinterpret_cast<const char*>(&matrix_size), sizeof(matrix_size));
  ofs.write(reinterpret_cast<const char*>(&default_cost_[0]),
            sizeof(default_cost_[0]) * default_cost_.size());
  // 4byte alignment
  while (ofs.tellp() % 4) {
    ofs.put('\0');
  }
  ofs.write(builder_->GetImage(), builder_->GetSize());
  ofs.close();
}

void SparseConnectorBuilder::Compile(
    const string &text_connection_file,
    const string &id_file,
    const string &special_pos_file,
    const string &output_file) {
  vector<int16> matrix;
  SparseConnectorBuilder builder;
  builder.SetIdFile(id_file);
  builder.SetSpecialPosFile(special_pos_file);
  builder.BuildInternal(text_connection_file, &matrix);
  builder.WriteTo(output_file);

  // verify connector
  {
    Mmap<char> mmap;
    CHECK(mmap.Open(output_file.c_str(), "r"));

    scoped_ptr<SparseConnector> connector(
        new SparseConnector(mmap.begin(), mmap.GetFileSize()));
    CHECK(connector.get());

    const int resolution = connector->GetResolution();
    const size_t matrix_size = builder.MatrixSize();
    for (int rid = 0; rid < matrix_size; ++rid) {
      for (int lid = 0; lid < matrix_size; ++lid) {
        const int diff = abs(connector->GetTransitionCost(lid, rid) -
                             matrix[lid + matrix_size * rid]);
        CHECK_LT(diff, resolution);
      }
    }
  }
}
}  // namespace converter
}  // namespace mozc
