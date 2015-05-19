// Copyright 2010-2013, Google Inc.
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

#include "chrome/nacl/dictionary_downloader.h"

#include <ppapi/utility/completion_callback_factory.h>

#include "base/logging.h"
#include "base/mutex.h"
#include "base/port.h"
#include "base/pepper_file_util.h"
#include "base/scoped_ptr.h"
#include "base/util.h"
#include "chrome/nacl/url_loader_util.h"
#include "net/http_client_pepper.h"

namespace mozc {
namespace chrome {
namespace nacl {

class DictionaryDownloader::Impl {
 public:
  Impl(const string &url,
       const string &file_name,
       pp::Instance *instance);
  ~Impl();
  void SetOption(uint32 start_delay,
                 uint32 random_delay,
                 uint32 retry_interval,
                 uint32 retry_backoff_count,
                 uint32 max_retry);
  void StartDownload();
  DownloadStatus GetStatus();

 private:
  void SetStatus(DownloadStatus status);
  void StartDownloadCallback(int32 result);
  void OnDownloaded(int32 result);
  const string url_;
  const string file_name_;
  pp::Instance *instance_;
  DownloadStatus status_;
  uint32 retry_count_;
  uint32 start_delay_;
  uint32 random_delay_;
  uint32 retry_interval_;
  uint32 retry_backoff_count_;
  uint32 max_retry_;
  pp::CompletionCallbackFactory<Impl> callback_factory_;
  Mutex mutex_;
  bool shut_down_;
  DISALLOW_COPY_AND_ASSIGN(Impl);
};


DictionaryDownloader::Impl::Impl(
    const string &url,
    const string &file_name,
    pp::Instance *instance)
    : url_(url),
      file_name_(file_name),
      instance_(instance),
      status_(DOWNLOAD_INITIALIZED),
      retry_count_(0),
      start_delay_(0),
      random_delay_(0),
      retry_interval_(0),
      retry_backoff_count_(0),
      max_retry_(0),
      shut_down_(false) {
  callback_factory_.Initialize(this);
}

DictionaryDownloader::Impl::~Impl() {
}

void DictionaryDownloader::Impl::SetOption(uint32 start_delay,
                                           uint32 random_delay,
                                           uint32 retry_interval,
                                           uint32 retry_backoff_count,
                                           uint32 max_retry) {
  start_delay_ = start_delay;
  random_delay_ = random_delay;
  retry_interval_ = retry_interval;
  retry_backoff_count_ = retry_backoff_count;
  max_retry_ = max_retry;
}

void DictionaryDownloader::Impl::StartDownload() {
  SetStatus(DOWNLOAD_PENDING);
  const int32 delay = start_delay_ + Util::Random(random_delay_);
  pp::Module::Get()->core()->CallOnMainThread(
      delay,
      callback_factory_.NewCallback(
          &DictionaryDownloader::Impl::StartDownloadCallback));
}

void DictionaryDownloader::Impl::StartDownloadCallback(int32 result) {
  if (GetStatus() == DOWNLOAD_STARTED) {
    VLOG(2) << "Currently downloading";
  }
  VLOG(2) << " url_:" << url_;
  VLOG(2) << " file_name_:" << file_name_;
  SetStatus(DOWNLOAD_STARTED);
  URLLoaderUtil::StartDownloadToFile(
      instance_,
      url_,
      file_name_,
      callback_factory_.NewCallback(
          &DictionaryDownloader::Impl::OnDownloaded));
}

DictionaryDownloader::DownloadStatus DictionaryDownloader::Impl::GetStatus() {
  scoped_lock l(&mutex_);
  return status_;
}

void DictionaryDownloader::Impl::SetStatus(
    DictionaryDownloader::DownloadStatus status) {
  scoped_lock l(&mutex_);
  status_ = status;
}

void DictionaryDownloader::Impl::OnDownloaded(int32 result) {
  if (result == PP_OK) {
    SetStatus(DOWNLOAD_FINISHED);
    return;
  }
  if (retry_count_ >= max_retry_) {
    SetStatus(DOWNLOAD_ERROR);
    return;
  }
  ++retry_count_;
  int32 back_off = retry_count_ - 1;
  if (back_off > retry_backoff_count_) {
    back_off = retry_backoff_count_;
  }
  int32 next_delay = retry_interval_ << back_off;
  if (random_delay_ != 0) {
    next_delay += Util::Random(random_delay_);
  }
  SetStatus(DOWNLOAD_WAITING_FOR_RETRY);
  VLOG(2) << " next_delay:" << next_delay;
  pp::Module::Get()->core()->CallOnMainThread(
      next_delay,
      callback_factory_.NewCallback(
          &DictionaryDownloader::Impl::StartDownloadCallback));
}

DictionaryDownloader::DictionaryDownloader(
    const string &url,
    const string &file_name)
    : impl_(new Impl(url,
                     file_name,
                     GetPepperInstanceForHTTPClient())) {
}

DictionaryDownloader::~DictionaryDownloader() {
}

void DictionaryDownloader::SetOption(uint32 start_delay,
                                     uint32 random_delay,
                                     uint32 retry_interval,
                                     uint32 retry_backoff_count,
                                     uint32 max_retry) {
  impl_->SetOption(start_delay,
                   random_delay,
                   retry_interval,
                   retry_backoff_count,
                   max_retry);
}

void DictionaryDownloader::StartDownload() {
  impl_->StartDownload();
}

DictionaryDownloader::DownloadStatus DictionaryDownloader::GetStatus() {
  return impl_->GetStatus();
}

}  // namespace nacl
}  // namespace chrome
}  // namespace mozc
