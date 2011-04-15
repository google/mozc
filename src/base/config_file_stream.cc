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

#include "base/config_file_stream.h"

#include <string.h>
#include <fstream>
#include <sstream>
#include "base/base.h"
#include "base/file_stream.h"

namespace mozc {

namespace {

static const char kSystemPrefix[] = "system://";
static const char kUserPrefix[]   = "user://";
static const char kFilePrefix[]   = "file://";

struct FileData {
  const char *name;
  const char *data;
  size_t size;
};

string RemovePrefix(const char *prefix, const string &filename) {
  const size_t size = strlen(prefix);
  if (filename.size() < size) {
    return "";
  }
  return filename.substr(size, filename.size() - size);
}

#include "base/config_file_stream_data.h"
}  // namespace

istream *ConfigFileStream::Open(const string &filename,
                                ios_base::openmode mode) {
  // system://foo.bar.txt
  if (filename.find(kSystemPrefix) == 0) {
    const string new_filename = RemovePrefix(kSystemPrefix, filename);
    for (size_t i = 0; i < arraysize(kFileData); ++i) {
      if (new_filename == kFileData[i].name) {
        istringstream *ifs = new istringstream(
            string(kFileData[i].data, kFileData[i].size), mode);
        CHECK(ifs);
        if (*ifs) {
          return ifs;
        }
        delete ifs;
        return NULL;
      }
    }
  // user://foo.bar.txt
  } else if (filename.find(kUserPrefix) == 0) {
    const string new_filename =
        Util::JoinPath(Util::GetUserProfileDirectory(),
                       RemovePrefix(kUserPrefix, filename));
    InputFileStream *ifs = new InputFileStream(new_filename.c_str(), mode);
    CHECK(ifs);
    if (*ifs) {
      return ifs;
    }
    delete ifs;
    return NULL;
  // file:///foo.map
  } else if (filename.find(kFilePrefix) == 0) {
    const string new_filename = RemovePrefix(kFilePrefix, filename);
    InputFileStream *ifs = new InputFileStream(new_filename.c_str(), mode);
    CHECK(ifs);
    if (*ifs) {
      return ifs;
    }
    delete ifs;
    return NULL;
  } else {
    LOG(WARNING) << filename << " has no prefix. open from localfile";
    InputFileStream *ifs = new InputFileStream(filename.c_str(), mode);
    CHECK(ifs);
    if (*ifs) {
      return ifs;
    }
    delete ifs;
    return NULL;
  }

  return NULL;
}

string ConfigFileStream::GetFileName(const string &filename) {
  if (filename.find(kSystemPrefix) == 0) {
    return "";
  } else if (filename.find(kUserPrefix) == 0) {
    return Util::JoinPath(Util::GetUserProfileDirectory(),
                          RemovePrefix(kUserPrefix, filename));
  } else if (filename.find(kFilePrefix) == 0) {
    return RemovePrefix(kUserPrefix, filename);
  } else {
    LOG(WARNING) << filename << " has no prefix. open from localfile";
    return filename;
  }
  return "";
}
}  // namespace mozc
