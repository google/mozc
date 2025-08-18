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

#include "base/log_file.h"

#include <string>

#include "absl/base/thread_annotations.h"
#include "absl/log/initialize.h"
#include "absl/log/log_entry.h"
#include "absl/log/log_sink.h"
#include "absl/log/log_sink_registry.h"
#include "absl/synchronization/mutex.h"
#include "base/file_stream.h"

namespace mozc {
namespace {

class LogFileSink : public absl::LogSink {
 public:
  explicit LogFileSink(const std::string& path) : file_(path) {}

  void Send(const absl::LogEntry& entry) override {
    absl::MutexLock lock(&mutex_);
    file_ << entry.text_message_with_prefix_and_newline();
  }

  void Flush() override {
    absl::MutexLock lock(&mutex_);
    file_.flush();
  }

 private:
  absl::Mutex mutex_;
  OutputFileStream file_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace

void RegisterLogFileSink(const std::string& path) {
#if !defined(NDEBUG) && !defined(__ANDROID__)
  absl::InitializeLog();
  absl::AddLogSink(new LogFileSink(path));
#endif  // !NDEBUG && !__ANDROID__
}

}  // namespace mozc
