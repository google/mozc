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

#include "sync/logging.h"

#include <string>

#ifdef OS_WINDOWS
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "base/base.h"
#include "base/file_stream.h"
#include "base/logging.h"
#include "base/singleton.h"
#include "base/util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"

DECLARE_string(log_dir);
DEFINE_int32(sync_verbose_level, 1,
             "verbose level of the sync logger");

namespace mozc {
namespace sync {

namespace {
const char kSyncLogFileName[] = "sync.log";
const size_t kMaxSyncLogSize = 1024 * 1024 * 5;  // 5MByte

class LogStreamImpl {
 public:
  LogStreamImpl() {
    Open(sync::Logging::GetLogFileName());
  }

  ostream &stream() {
    return *stream_;
  }

  void Reset() {
    stream_.reset(NULL);
    const string &filename = sync::Logging::GetLogFileName();
    Util::Unlink(filename);
    Open(filename);
  }

  void TruncateStream() {
    scoped_lock l(&mutex_);
    CHECK(stream_.get());
    const size_t size = static_cast<size_t>(stream_->tellp());
    if (size < kMaxSyncLogSize) {
      return;
    }

    LOG(INFO) << "Truncating logging stream";
    stream_.reset(NULL);
    const string filename = sync::Logging::GetLogFileName();
    const string tmp_filename = filename + ".tmp";
    Util::AtomicRename(filename, tmp_filename);
    Open(filename);
    {
      InputFileStream ifs(tmp_filename.c_str());
      // move to 4/5 position.
      ifs.seekg(static_cast<size_t>(0.8 * size), ios::beg);
      string line;
      getline(ifs, line);   // skip first line because this may be broken.
      while (getline(ifs, line)) {
        *(stream_.get()) << line << '\n';
      }
      stream_->flush();
    }
    Util::Unlink(tmp_filename);
  }

 private:
  void Open(const string &filename) {
    stream_.reset(new OutputFileStream(filename.c_str(), ios::app));
    CHECK(stream_.get());
#ifndef OS_WINDOWS
    ::chmod(filename.c_str(), 0600);
#endif
  }

  Mutex mutex_;
  scoped_ptr<ostream> stream_;
};
}

// static
ostream &Logging::GetLogStream() {
  return Singleton<LogStreamImpl>::get()->stream();
}

// static
int Logging::GetVerboseLevel() {
  return FLAGS_sync_verbose_level;
}

// static
string Logging::GetLogFileName() {
  return Util::JoinPath(FLAGS_log_dir.empty() ?
                        Util::GetLoggingDirectory() :
                        FLAGS_log_dir, kSyncLogFileName);
}

// static
void Logging::Reset() {
  Singleton<LogStreamImpl>::get()->Reset();
}

LogFinalizer::~LogFinalizer() {
  Singleton<LogStreamImpl>::get()->stream() << endl;
  Singleton<LogStreamImpl>::get()->TruncateStream();
}
}  // sync
}  // mozc
