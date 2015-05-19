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

#include "chrome/nacl/url_loader_util.h"

#include <ppapi/c/pp_file_info.h>
#include <ppapi/c/ppb_file_io.h>
#include <ppapi/cpp/file_io.h>
#include <ppapi/cpp/file_ref.h>
#include <ppapi/cpp/file_system.h>
#include <ppapi/cpp/instance.h>
#include <ppapi/cpp/url_loader.h>
#include <ppapi/cpp/url_request_info.h>
#include <ppapi/cpp/url_response_info.h>
#include <ppapi/utility/completion_callback_factory.h>

#include "base/logging.h"
#include "base/port.h"
#include "base/scoped_ptr.h"

namespace mozc {
namespace chrome {
namespace nacl {

namespace {
const int32_t kReadBufferSize = 32768;

class URLLoaderStreamToFileHandler {
 public:
  URLLoaderStreamToFileHandler(pp::Instance *instance,
                               const string &url,
                               const string &file_name,
                               pp::CompletionCallback callback);
  void Start();

 private:
  // "delete this" is called in URLLoaderStreamToFileHandler::Complete().
  ~URLLoaderStreamToFileHandler();
  void StartImpl(int32_t result);
  void OnOpen(int32 result);
  void OnStreamComplete(int32 result);
  void OnInputFileOpen(int32_t result);
  void OnInputFileQuery(int32_t result);
  void OnFileSystemOpen(int32_t result);
  void OnDeleteOutputFile(int32_t result);
  void OnOutputFileOpen(int32_t result);
  void OnInputFileRead(int32_t bytes_read);
  void OnOutputFileWrite(int32_t bytes_written);
  void OnOutputFileFlush(int32_t result);
  void Complete(bool result);

  pp::Instance *instance_;
  const string url_;
  const string file_name_;
  pp::CompletionCallback callback_;
  scoped_ptr<pp::URLRequestInfo> url_request_;
  scoped_ptr<pp::URLLoader> url_loader_;
  pp::URLResponseInfo url_response_;
  pp::FileRef body_file_ref_;
  pp::CompletionCallbackFactory<URLLoaderStreamToFileHandler> callback_factory_;
  scoped_ptr<pp::FileSystem> file_system_;
  scoped_ptr<pp::FileRef> output_file_ref_;
  scoped_ptr<pp::FileIO> output_file_io_;
  scoped_ptr<pp::FileIO> input_file_io_;
  PP_FileInfo input_file_info_;
  int64_t total_read_bytes_;
  int64_t total_written_bytes_;
  int64_t buffer_written_bytes_;
  scoped_array<char> tmp_buffer_;

  DISALLOW_COPY_AND_ASSIGN(URLLoaderStreamToFileHandler);
};

URLLoaderStreamToFileHandler::URLLoaderStreamToFileHandler(
    pp::Instance *instance,
    const string &url,
    const string &file_name,
    pp::CompletionCallback callback)
    : instance_(instance),
      url_(url),
      file_name_(file_name),
      callback_(callback),
      total_read_bytes_(0),
      total_written_bytes_(0),
      buffer_written_bytes_(0) {
}

URLLoaderStreamToFileHandler::~URLLoaderStreamToFileHandler() {
}

void URLLoaderStreamToFileHandler::Start() {
  callback_factory_.Initialize(this);
  if (pp::Module::Get()->core()->IsMainThread()) {
    StartImpl(0);
    return;
  }
  pp::Module::Get()->core()->CallOnMainThread(
      0,
      callback_factory_.NewCallback(&URLLoaderStreamToFileHandler::StartImpl));
}

void URLLoaderStreamToFileHandler::StartImpl(int32_t result) {
  CHECK(pp::Module::Get()->core()->IsMainThread());
  CHECK(!url_request_.get());
  CHECK(!url_loader_.get());
  url_request_.reset(new pp::URLRequestInfo(instance_));
  url_loader_.reset(new pp::URLLoader(instance_));
  url_request_->SetURL(url_);
  url_request_->SetMethod("GET");
  url_request_->SetStreamToFile(true);
  url_loader_->Open(*url_request_,
                    callback_factory_.NewCallback(
                        &URLLoaderStreamToFileHandler::OnOpen));
}

void URLLoaderStreamToFileHandler::OnOpen(int32 result) {
  if (result != PP_OK) {
    DLOG(ERROR) << "URLLoaderStreamToFileHandler::OnOpen error";
    Complete(false);
    return;
  }
  const pp::URLResponseInfo response = url_loader_->GetResponseInfo();
  if (response.GetStatusCode() != 200) {
    DLOG(ERROR) << "pp::URLLoader::Open() failed: " << url_
                << " Status code: " << response.GetStatusCode();
    Complete(false);
    return;
  }
  url_loader_->FinishStreamingToFile(
      callback_factory_.NewCallback(
          &URLLoaderStreamToFileHandler::OnStreamComplete));
  url_response_ = url_loader_->GetResponseInfo();
  body_file_ref_ = url_response_.GetBodyAsFileRef();
}

void URLLoaderStreamToFileHandler::OnStreamComplete(int32 result) {
  if (result != PP_OK) {
    DLOG(ERROR) << "URLLoaderStreamToFileHandler::OnStreamComplete error";
    Complete(false);
    return;
  }
  input_file_io_.reset(new pp::FileIO(instance_));
  const int32_t ret = input_file_io_->Open(
      body_file_ref_, PP_FILEOPENFLAG_READ,
      callback_factory_.NewCallback(
          &URLLoaderStreamToFileHandler::OnInputFileOpen));
  if ((ret != PP_OK_COMPLETIONPENDING) && (ret != PP_OK)) {
    DLOG(ERROR) << "input_file_io_->Open error";
    Complete(false);
    return;
  }
}

void URLLoaderStreamToFileHandler::OnInputFileOpen(int32_t result) {
  if (result != PP_OK) {
    DLOG(ERROR) << "URLLoaderStreamToFileHandler::OnInputFileOpen error";
    Complete(false);
    return;
  }
  const int32_t ret = input_file_io_->Query(
      &input_file_info_,
      callback_factory_.NewCallback(
          &URLLoaderStreamToFileHandler::OnInputFileQuery));
  if ((ret != PP_OK_COMPLETIONPENDING) && (ret != PP_OK)) {
    DLOG(ERROR) << "input_file_io_->Query error";
    Complete(false);
    return;
  }
}
void URLLoaderStreamToFileHandler::OnInputFileQuery(int32_t result) {
  if (result != PP_OK) {
    DLOG(ERROR) << "URLLoaderStreamToFileHandler::OnInputFileQuery error";
    Complete(false);
    return;
  }
  file_system_.reset(
      new pp::FileSystem(instance_, PP_FILESYSTEMTYPE_LOCALPERSISTENT));
  const int32_t ret = file_system_->Open(
      input_file_info_.size,
      callback_factory_.NewCallback(
          &URLLoaderStreamToFileHandler::OnFileSystemOpen));
  if ((ret != PP_OK_COMPLETIONPENDING) && (ret != PP_OK)) {
    DLOG(ERROR) << "file_system_->Open error";
    Complete(false);
    return;
  }
}

void URLLoaderStreamToFileHandler::OnFileSystemOpen(int32_t result) {
  if (result != PP_OK) {
    DLOG(ERROR) << "URLLoaderStreamToFileHandler::OnFileSystemOpen error";
    Complete(false);
    return;
  }
  output_file_ref_.reset(new pp::FileRef(*file_system_, file_name_.c_str()));
  const int32_t ret = output_file_ref_->Delete(
      callback_factory_.NewCallback(
          &URLLoaderStreamToFileHandler::OnDeleteOutputFile));
  if ((ret != PP_OK_COMPLETIONPENDING) && (ret != PP_OK)) {
    DLOG(ERROR) << "output_file_ref_->Delete error";
    Complete(false);
    return;
  }
}

void URLLoaderStreamToFileHandler::OnDeleteOutputFile(int32_t result) {
  output_file_io_.reset(new pp::FileIO(instance_));
  const int32_t ret = output_file_io_->Open(
      *output_file_ref_, PP_FILEOPENFLAG_WRITE | PP_FILEOPENFLAG_CREATE,
      callback_factory_.NewCallback(
          &URLLoaderStreamToFileHandler::OnOutputFileOpen));
  if ((ret != PP_OK_COMPLETIONPENDING) && (ret != PP_OK)) {
    DLOG(ERROR) << "output_file_io_->Open error";
    Complete(false);
    return;
  }
}

void URLLoaderStreamToFileHandler::OnOutputFileOpen(int32_t result) {
  if (result != PP_OK) {
    DLOG(ERROR) << "URLLoaderStreamToFileHandler::OnOutputFileOpen error";
    Complete(false);
    return;
  }
  tmp_buffer_.reset(new char[kReadBufferSize]);
  OnInputFileRead(0);
}

void URLLoaderStreamToFileHandler::OnInputFileRead(int32_t bytes_read) {
  if (bytes_read < 0) {
    DLOG(ERROR) << "URLLoaderStreamToFileHandler::OnInputFileRead error";
    Complete(false);
    return;
  }
  total_read_bytes_ += bytes_read;
  if (bytes_read == 0) {
    const int32_t ret = input_file_io_->Read(
        total_read_bytes_, tmp_buffer_.get(),
        min(kReadBufferSize,
            static_cast<int32_t>(input_file_info_.size - total_read_bytes_)),
        callback_factory_.NewCallback(
            &URLLoaderStreamToFileHandler::OnInputFileRead));
    if ((ret != PP_OK_COMPLETIONPENDING) && (ret != PP_OK)) {
      DLOG(ERROR) << "input_file_io_->Read error";
      Complete(false);
      return;
    }
  } else {
    buffer_written_bytes_ = 0;
    const int32_t ret = output_file_io_->Write(
        total_written_bytes_, tmp_buffer_.get(), bytes_read,
        callback_factory_.NewCallback(
            &URLLoaderStreamToFileHandler::OnOutputFileWrite));
    if ((ret != PP_OK_COMPLETIONPENDING) && (ret != PP_OK)) {
      DLOG(ERROR) << "output_file_io_->Write error";
      Complete(false);
      return;
    }
  }
}

void URLLoaderStreamToFileHandler::OnOutputFileWrite(int32_t bytes_written) {
  if (bytes_written < 0) {
    DLOG(ERROR) << "URLLoaderStreamToFileHandler::OnOutputFileWrite error";
    Complete(false);
    return;
  }
  total_written_bytes_ += bytes_written;
  buffer_written_bytes_ += bytes_written;
  if (total_read_bytes_ == total_written_bytes_) {
    if (total_read_bytes_ == input_file_info_.size) {
      // Finish writing
      const int32_t ret = output_file_io_->Flush(
          callback_factory_.NewCallback(
              &URLLoaderStreamToFileHandler::OnOutputFileFlush));
      if ((ret != PP_OK_COMPLETIONPENDING) && (ret != PP_OK)) {
        DLOG(ERROR) << "output_file_io_->Flush error";
        Complete(false);
        return;
      }
    } else {
      // Read more
      const int32_t ret = input_file_io_->Read(
          total_read_bytes_, tmp_buffer_.get(),
          min(kReadBufferSize,
              static_cast<int32_t>(input_file_info_.size - total_read_bytes_)),
          callback_factory_.NewCallback(
              &URLLoaderStreamToFileHandler::OnInputFileRead));
      if ((ret != PP_OK_COMPLETIONPENDING) && (ret != PP_OK)) {
        DLOG(ERROR) << "input_file_io_->Read error";
        Complete(false);
        return;
      }
    }
  } else {
    // Writes more
    const int32_t ret = output_file_io_->Write(
        total_written_bytes_,
        &tmp_buffer_[buffer_written_bytes_],
        total_read_bytes_ - total_written_bytes_,
        callback_factory_.NewCallback(
            &URLLoaderStreamToFileHandler::OnOutputFileWrite));
    if ((ret != PP_OK_COMPLETIONPENDING) && (ret != PP_OK)) {
      DLOG(ERROR) << "output_file_io_->Write error";
      Complete(false);
      return;
    }
  }
}

void URLLoaderStreamToFileHandler::OnOutputFileFlush(int32_t result) {
  if (result != PP_OK) {
    DLOG(ERROR) << "URLLoaderStreamToFileHandler::OnOutputFileFlush error";
    Complete(false);
    return;
  }
  Complete(true);
}

void URLLoaderStreamToFileHandler::Complete(bool result) {
  callback_.Run(result ? PP_OK : PP_ERROR_FAILED);
  delete this;
}

}  // namespace

void URLLoaderUtil::StartDownloadToFile(pp::Instance *instance,
                                        const string &url,
                                        const string &file_name,
                                        pp::CompletionCallback callback) {
  URLLoaderStreamToFileHandler *handler =
      new URLLoaderStreamToFileHandler(instance,
                                       url,
                                       file_name,
                                       callback);
  DCHECK(handler);
  handler->Start();
}

}  // namespace nacl
}  // namespace chrome
}  // namespace mozc
