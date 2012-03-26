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

#include "base/config_file_stream.h"

#ifdef OS_WINDOWS
#include <windows.h>
#endif  // OS_WINDOWS

#include <map>
#include <string.h>
#include <fstream>
#include <sstream>
#include "base/base.h"
#include "base/file_stream.h"
#include "base/singleton.h"

namespace mozc {

namespace {

static const char kSystemPrefix[] = "system://";
static const char kUserPrefix[]   = "user://";
static const char kFilePrefix[]   = "file://";
static const char kMemoryPrefix[] = "memory://";

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

class OnMemoryFileMap {
 public:
  string get(const string &key) const {
    map<string, string>::const_iterator it = map_.find(key);
    if (it != map_.end()) {
      return it->second;
    }
    return string("");
  }

  void set(const string &key, const string &value) {
    map_[key] = value;
  }

  void clear() {
    map_.clear();
  }

 private:
  map<string, string> map_;
};

#include "base/config_file_stream_data.h"
}  // namespace

istream *ConfigFileStream::Open(const string &filename,
                                ios_base::openmode mode) {
  // system://foo.bar.txt
  if (Util::StartsWith(filename, kSystemPrefix)) {
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
  } else if (Util::StartsWith(filename, kUserPrefix)) {
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
  } else if (Util::StartsWith(filename, kFilePrefix)) {
    const string new_filename = RemovePrefix(kFilePrefix, filename);
    InputFileStream *ifs = new InputFileStream(new_filename.c_str(), mode);
    CHECK(ifs);
    if (*ifs) {
      return ifs;
    }
    delete ifs;
    return NULL;
  } else if (Util::StartsWith(filename, kMemoryPrefix)) {
    istringstream *ifs = new istringstream(
        Singleton<OnMemoryFileMap>::get()->get(filename), mode);
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

bool ConfigFileStream::AtomicUpdate(const string &filename,
                                    const string &new_binary_contens) {
  if (Util::StartsWith(filename, kMemoryPrefix)) {
    Singleton<OnMemoryFileMap>::get()->set(filename, new_binary_contens);
    return true;
  } else if (Util::StartsWith(filename, kSystemPrefix)) {
    LOG(ERROR) << "Cannot update system:// files.";
    return false;
  }

  // We should save the new config first,
  // as we may rewrite the original config according to platform.
  // The original config should be platform independent.
  const string real_filename = GetFileName(filename);
  if (real_filename.empty()) {
    return false;
  }

  const string tmp_filename = real_filename + ".tmp";
  {
    OutputFileStream ofs(tmp_filename.c_str(), ios::out | ios::binary);
    if (!ofs) {
      LOG(ERROR) << "cannot open " << tmp_filename;
      return false;
    }
    ofs << new_binary_contens;
  }

  if (!Util::AtomicRename(tmp_filename, real_filename)) {
    LOG(ERROR) << "Util::AtomicRename failed";
    return false;
  }

#ifdef OS_WINDOWS
  // If file name doesn't end with ".db", the file
  // is more likely a temporary file.
  if (!Util::EndsWith(real_filename, ".db")) {
    wstring wfilename;
    Util::UTF8ToWide(real_filename.c_str(), &wfilename);
    // TODO(yukawa): Provide a way to
    // integrate ::SetFileAttributesTransacted with
    // AtomicRename.
    if (!::SetFileAttributes(wfilename.c_str(),
                             FILE_ATTRIBUTE_HIDDEN |
                             FILE_ATTRIBUTE_SYSTEM)) {
      LOG(ERROR) << "Cannot make hidden: " << real_filename
                 << " " << ::GetLastError();
    }
  }
#endif  // OS_WINDOWS

  return true;
}

string ConfigFileStream::GetFileName(const string &filename) {
  if (Util::StartsWith(filename, kSystemPrefix) ||
      Util::StartsWith(filename, kMemoryPrefix)) {
    return "";
  } else if (Util::StartsWith(filename, kUserPrefix)) {
    return Util::JoinPath(Util::GetUserProfileDirectory(),
                          RemovePrefix(kUserPrefix, filename));
  } else if (Util::StartsWith(filename, kFilePrefix)) {
    return RemovePrefix(kUserPrefix, filename);
  } else {
    LOG(WARNING) << filename << " has no prefix. open from localfile";
    return filename;
  }
  return "";
}

void ConfigFileStream::ClearOnMemoryFiles() {
  Singleton<OnMemoryFileMap>::get()->clear();
}
}  // namespace mozc
