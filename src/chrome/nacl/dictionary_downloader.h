// Copyright 2010-2014, Google Inc.
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

#ifndef MOZC_CHROME_NACL_DICTIONARY_DOWNLOADER_H_
#define MOZC_CHROME_NACL_DICTIONARY_DOWNLOADER_H_

#include <string>

#include "base/port.h"
#include "base/scoped_ptr.h"

namespace mozc {
namespace chrome {
namespace nacl {

class DictionaryDownloader {
 public:
  enum DownloadStatus {
    DOWNLOAD_INITIALIZED,
    DOWNLOAD_PENDING,
    DOWNLOAD_STARTED,
    DOWNLOAD_FINISHED,
    DOWNLOAD_WAITING_FOR_RETRY,
    DOWNLOAD_ERROR
  };
  DictionaryDownloader(const string &url,
                       const string &file_name);
  ~DictionaryDownloader();
  // Sets the options.
  void SetOption(uint32 start_delay,
                 uint32 random_delay,
                 uint32 retry_interval,
                 uint32 retry_backoff_count,
                 uint32 max_retry);
  // Downloading will be started after start_delay + [0 - random_delay] msec.
  // If failed, 1st retry is started after retry_interval + [0 - random_delay].
  // While the retry count is less than retry_backoff_count, retry_interval will
  // be doubled.
  void StartDownload();
  DownloadStatus GetStatus();

 private:
  class Impl;
  scoped_ptr<Impl> impl_;
  DISALLOW_COPY_AND_ASSIGN(DictionaryDownloader);
};

}  // namespace nacl
}  // namespace chrome
}  // namespace mozc

#endif  // MOZC_CHROME_NACL_DICTIONARY_DOWNLOADER_H_
