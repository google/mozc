// Copyright 2010-2016, Google Inc.
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

#include "data_manager/packed/system_dictionary_data_packer.h"

#include <memory>
#include <string>

#include "base/codegen_bytearray_stream.h"
#include "base/file_stream.h"
#include "base/logging.h"
#include "base/protobuf/gzip_stream.h"
#include "base/protobuf/zero_copy_stream_impl.h"
#include "base/version.h"
#include "data_manager/packed/system_dictionary_data.pb.h"
#include "data_manager/packed/system_dictionary_format_version.h"

namespace mozc {
namespace packed {

SystemDictionaryDataPacker::SystemDictionaryDataPacker(const string &version) {
  system_dictionary_.reset(new SystemDictionaryData());
  system_dictionary_->set_product_version(version);
  system_dictionary_->set_format_version(kSystemDictionaryFormatVersion);
}

SystemDictionaryDataPacker::~SystemDictionaryDataPacker() {
}

void SystemDictionaryDataPacker::SetMozcData(const string &data,
                                             const string &magic) {
  system_dictionary_->set_mozc_data(data);
  system_dictionary_->set_mozc_data_magic(magic);
}

bool SystemDictionaryDataPacker::Output(const string &file_path,
                                        bool use_gzip) {
  OutputFileStream output(file_path.c_str(),
                          ios::out | ios::binary | ios::trunc);
  if (use_gzip) {
    protobuf::io::OstreamOutputStream zero_copy_output(&output);
    protobuf::io::GzipOutputStream gzip_stream(&zero_copy_output);
    if (!system_dictionary_->SerializeToZeroCopyStream(&gzip_stream) ||
        !gzip_stream.Close()) {
      LOG(ERROR) << "Serialize to gzip stream failed.";
    }
  } else {
    if (!system_dictionary_->SerializeToOstream(&output)) {
      LOG(ERROR) << "Failed to write data to " << file_path;
      return false;
    }
  }
  return true;
}

bool SystemDictionaryDataPacker::OutputHeader(
    const string &file_path,
    bool use_gzip) {
  std::unique_ptr<ostream> output_stream(
      new mozc::OutputFileStream(file_path.c_str(), ios::out | ios::trunc));
  CodeGenByteArrayOutputStream *codegen_stream;
  output_stream.reset(
      codegen_stream = new mozc::CodeGenByteArrayOutputStream(
          output_stream.release(),
          mozc::codegenstream::OWN_STREAM));
  codegen_stream->OpenVarDef("PackedSystemDictionary");
  if (use_gzip) {
    protobuf::io::OstreamOutputStream zero_copy_output(output_stream.get());
    protobuf::io::GzipOutputStream gzip_stream(&zero_copy_output);
    if (!system_dictionary_->SerializeToZeroCopyStream(&gzip_stream) ||
        !gzip_stream.Close()) {
      LOG(ERROR) << "Serialize to gzip stream failed.";
    }
  } else {
    if (!system_dictionary_->SerializeToOstream(output_stream.get())) {
      LOG(ERROR) << "Failed to write data to " << file_path;
      return false;
    }
  }
  return true;
}

}  // namespace packed
}  // namespace mozc
