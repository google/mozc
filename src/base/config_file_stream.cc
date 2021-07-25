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

#include "base/config_file_stream.h"

#ifdef OS_WIN
#include <windows.h>
#endif  // OS_WIN

#include <cstring>
#include <map>
#include <sstream>

#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/port.h"
#include "base/singleton.h"
#include "base/system_util.h"
#include "base/util.h"

namespace mozc {

namespace {

static constexpr char kSystemPrefix[] = "system://";
static constexpr char kUserPrefix[] = "user://";
static constexpr char kFilePrefix[] = "file://";
static constexpr char kMemoryPrefix[] = "memory://";

struct FileData {
  const char *name;
  const char *data;
  size_t size;
};

std::string RemovePrefix(const char *prefix, const std::string &filename) {
  const size_t size = strlen(prefix);
  if (filename.size() < size) {
    return "";
  }
  return filename.substr(size, filename.size() - size);
}

class OnMemoryFileMap {
 public:
  std::string get(const std::string &key) const {
    std::map<std::string, std::string>::const_iterator it = map_.find(key);
    if (it != map_.end()) {
      return it->second;
    }
    return std::string("");
  }

  void set(const std::string &key, const std::string &value) {
    map_[key] = value;
  }

  void clear() { map_.clear(); }

 private:
  std::map<std::string, std::string> map_;
};

#include "base/config_file_stream_data.inc"
}  // namespace

std::istream *ConfigFileStream::Open(const std::string &filename,
                                     std::ios_base::openmode mode) {
  // system://foo.bar.txt
  if (Util::StartsWith(filename, kSystemPrefix)) {
    const std::string new_filename = RemovePrefix(kSystemPrefix, filename);
    for (size_t i = 0; i < arraysize(kFileData); ++i) {
      if (new_filename == kFileData[i].name) {
        std::istringstream *ifs = new std::istringstream(
            std::string(kFileData[i].data, kFileData[i].size), mode);
        CHECK(ifs);
        if (ifs->good()) {
          return ifs;
        }
        delete ifs;
        return nullptr;
      }
    }
    // user://foo.bar.txt
  } else if (Util::StartsWith(filename, kUserPrefix)) {
    const std::string new_filename =
        FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(),
                           RemovePrefix(kUserPrefix, filename));
    InputFileStream *ifs = new InputFileStream(new_filename.c_str(), mode);
    CHECK(ifs);
    if (ifs->good()) {
      return ifs;
    }
    delete ifs;
    return nullptr;
    // file:///foo.map
  } else if (Util::StartsWith(filename, kFilePrefix)) {
    const std::string new_filename = RemovePrefix(kFilePrefix, filename);
    InputFileStream *ifs = new InputFileStream(new_filename.c_str(), mode);
    CHECK(ifs);
    if (ifs->good()) {
      return ifs;
    }
    delete ifs;
    return nullptr;
  } else if (Util::StartsWith(filename, kMemoryPrefix)) {
    std::istringstream *ifs = new std::istringstream(
        Singleton<OnMemoryFileMap>::get()->get(filename), mode);
    CHECK(ifs);
    if (ifs->good()) {
      return ifs;
    }
    delete ifs;
    return nullptr;
  } else {
    LOG(WARNING) << filename << " has no prefix. open from localfile";
    InputFileStream *ifs = new InputFileStream(filename.c_str(), mode);
    CHECK(ifs);
    if (ifs->good()) {
      return ifs;
    }
    delete ifs;
    return nullptr;
  }

  return nullptr;
}

bool ConfigFileStream::AtomicUpdate(const std::string &filename,
                                    const std::string &new_binary_contens) {
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
  const std::string real_filename = GetFileName(filename);
  if (real_filename.empty()) {
    return false;
  }

  const std::string tmp_filename = real_filename + ".tmp";
  {
    OutputFileStream ofs(tmp_filename.c_str(),
                         std::ios::out | std::ios::binary);
    if (!ofs.good()) {
      LOG(ERROR) << "cannot open " << tmp_filename;
      return false;
    }
    ofs << new_binary_contens;
  }

  if (!FileUtil::AtomicRename(tmp_filename, real_filename)) {
    LOG(ERROR) << "FileUtil::AtomicRename failed";
    return false;
  }

#ifdef OS_WIN
  // If file name doesn't end with ".db", the file
  // is more likely a temporary file.
  if (!Util::EndsWith(real_filename, ".db")) {
    // TODO(yukawa): Provide a way to
    // integrate ::SetFileAttributesTransacted with
    // AtomicRename.
    if (!FileUtil::HideFile(real_filename)) {
      LOG(ERROR) << "Cannot make hidden: " << real_filename << " "
                 << ::GetLastError();
    }
  }
#endif  // OS_WIN
  return true;
}

std::string ConfigFileStream::GetFileName(const std::string &filename) {
  if (Util::StartsWith(filename, kSystemPrefix) ||
      Util::StartsWith(filename, kMemoryPrefix)) {
    return "";
  } else if (Util::StartsWith(filename, kUserPrefix)) {
    return FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(),
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
