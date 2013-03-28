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

#include "net/http_client_pepper.h"

#include <ppapi/cpp/instance.h>
#include <ppapi/cpp/url_loader.h>
#include <ppapi/cpp/url_request_info.h>
#include <ppapi/cpp/url_response_info.h>
#include <ppapi/cpp/var.h>
#include <ppapi/utility/completion_callback_factory.h>

#include "base/base.h"
#include "base/logging.h"
#include "base/mutex.h"
#include "base/number_util.h"
#include "base/pepper_scoped_obj.h"
#include "base/util.h"

namespace mozc {
namespace {

const int kReadBufferSize = 1024 * 1024;
pp::Instance *g_pepper_instance = NULL;

class PepperURLLoader {
 public:
  PepperURLLoader(HTTPMethodType type,
                  const string &url,
                  const char *post_data,
                  size_t post_size,
                  const HTTPClient::Option &option);
  ~PepperURLLoader();
  bool Start(int32 timeout_millisec, string *output_string);

 private:
  void StartImpl(int32_t result);
  void OnOpen(int32 result);
  void OnRead(int32 result);
  void ReadBody();
  bool AppendDataBytes(const char *buffer, int32 num_bytes);
  bool CheckTimeouted();
  void Complete(bool result);

  const HTTPMethodType type_;
  const string url_;
  string post_data_;
  const HTTPClient::Option option_;
  bool result_;
  bool finished_;
  bool timeouted_;
  scoped_array<char> tmp_buffer_;
  pp::CompletionCallbackFactory<PepperURLLoader> cc_factory_;
  scoped_main_thread_destructed_object<pp::URLRequestInfo> url_request_;
  scoped_main_thread_destructed_object<pp::URLLoader> url_loader_;

  UnnamedEvent event_;
  Mutex mutex_;
  string data_buffer_;

  // The size of response header.
  // It is set when option.include_header is true or HTTPMethodType is HEAD.
  size_t response_header_size_;
  // Content-Length of the response.
  size_t response_content_length_;
};

PepperURLLoader::PepperURLLoader(HTTPMethodType type,
                                 const string &url,
                                 const char *post_data,
                                 size_t post_size,
                                 const HTTPClient::Option &option)
    : type_(type),
      url_(url),
      option_(option),
      result_(false),
      finished_(false),
      timeouted_(false),
      tmp_buffer_(new char[kReadBufferSize]),
      response_header_size_(0),
      response_content_length_(0) {
  if (post_data) {
    post_data_ = string(post_data, post_size);
  }
  cc_factory_.Initialize(this);
}

PepperURLLoader::~PepperURLLoader() {
  VLOG(2) << "PepperURLLoader deleted";
}

void PepperURLLoader::StartImpl(int32_t result) {
  VLOG(2) << "PepperURLLoader::StartImpl";
  if (CheckTimeouted()) {
    VLOG(2) << "PepperURLLoader::StartImpl Timeouted!";
    delete this;
    return;
  }
  CHECK(!url_request_.get());
  CHECK(!url_loader_.get());
  url_request_.reset(new pp::URLRequestInfo(g_pepper_instance));
  url_loader_.reset(new pp::URLLoader(g_pepper_instance));
  url_request_->SetURL(url_);
  switch (type_) {
    case HTTP_GET:
      url_request_->SetMethod("GET");
      break;
    case HTTP_HEAD:
      url_request_->SetMethod("HEAD");
      break;
    case HTTP_POST:
      url_request_->SetMethod("POST");
      break;
  }
  if (!option_.headers.empty()) {
    string headers;
    for (size_t i = 0; i < option_.headers.size(); ++i) {
      const string &header = option_.headers[i];
      if (Util::StartsWith(header, "User-Agent: ")) {
        LOG(ERROR) << "We can't set the custom user agent in Chrome.";
      } else {
        if (!headers.empty()) {
          headers += "\n";
        }
        headers += header;
      }
    }
    url_request_->SetHeaders(headers);
  }

  if (!post_data_.empty()) {
    url_request_->AppendDataToBody(post_data_.data(), post_data_.size());
  }

  url_request_->SetRecordDownloadProgress(true);
  VLOG(2) << "PepperURLLoader::StartImpl url_loader_->Open";
  const int32_t ret = url_loader_->Open(
      *url_request_,
      cc_factory_.NewCallback(&PepperURLLoader::OnOpen));
  if ((ret != PP_OK_COMPLETIONPENDING) && (ret != PP_OK)) {
    VLOG(2) << "url_loader_->Open error. ret: " << ret;
    Complete(false);
    return;
  }
}

void PepperURLLoader::OnOpen(int32 result) {
  VLOG(2) << "PepperURLLoader::OnOpen " << result;
  if (CheckTimeouted()) {
    VLOG(2) << "PepperURLLoader::OnOpen Timeouted!" << result;
    delete this;
    return;
  }
  if (result != PP_OK) {
    VLOG(2) << "pp::URLLoader::Open() failed: " << url_;
    Complete(false);
    return;
  }
  const pp::URLResponseInfo response = url_loader_->GetResponseInfo();
  if (response.GetStatusCode() != 200) {
    VLOG(2) << "pp::URLLoader::Open() failed: " << url_
            << " Status code: " << response.GetStatusCode();
    Complete(false);
    return;
  }

  if (option_.include_header || type_ == HTTP_HEAD) {
    string headers;
    string status_code = NumberUtil::SimpleItoa(response.GetStatusCode());
    string status_line = "OK";

    const pp::Var status_line_var = response.GetStatusLine();
    if (status_line_var.is_string()) {
      status_line = status_line_var.AsString();
    } else {
      LOG(ERROR) << "GetStatusLine Error";
    }
    headers = "HTTP/1.0 " + status_code + " " + status_line + "\n";
    const pp::Var headers_var = response.GetHeaders();
    if (headers_var.is_string()) {
      headers += headers_var.AsString() + "\n";
    }
    headers += "\n";
    response_header_size_ = headers.size();
    data_buffer_ = headers;
    if (response_header_size_ > option_.max_data_size) {
      VLOG(2) << "header_size(" << response_header_size_ <<") is bigger "
              << "than max_data_size(" << option_.max_data_size << ")";
      Complete(false);
      return;
    }
  }

  int64_t bytes_received = 0;
  int64_t bytes_total = 0;
  if (url_loader_->GetDownloadProgress(&bytes_received, &bytes_total)) {
    VLOG(2) << "pp::GetDownloadProgress: bytes_total " << bytes_total;
    if (bytes_total > 0) {
      response_content_length_ = bytes_total;
      const size_t expected_size = response_header_size_ +
                                   response_content_length_;
      if (expected_size > option_.max_data_size) {
        VLOG(2) << "expected_size(" << expected_size << ") is bigger than "
                << "max_data_size(" << option_.max_data_size << ")";
        Complete(false);
        return;
      }
      data_buffer_.reserve(expected_size);
    }
  }
  url_request_->SetRecordDownloadProgress(false);
  ReadBody();
}

bool PepperURLLoader::AppendDataBytes(const char *buffer, int32 num_bytes) {
  if (num_bytes <= 0) {
    return true;
  }
  num_bytes = min(kReadBufferSize, num_bytes);
  if (data_buffer_.size() + num_bytes > option_.max_data_size) {
    VLOG(2) << "PepperURLLoader::AppendDataBytes over flow :"
            << " option_.max_data_size: " << (option_.max_data_size)
            << " data_buffer_.size() + num_bytes: "
            << data_buffer_.size() + num_bytes;
    Complete(false);
    return false;
  }
  data_buffer_.append(buffer, num_bytes);
  return true;
}

void PepperURLLoader::OnRead(int32 result) {
  if (CheckTimeouted()) {
    VLOG(2) << "PepperURLLoader::OnRead Timeouted!";
    delete this;
    return;
  }
  if (result == PP_OK) {
    VLOG(2) << "PepperURLLoader::OnRead Complete!!" << data_buffer_.size();
    if (response_content_length_ != 0) {
      if (data_buffer_.size() !=
              response_content_length_ + response_header_size_) {
        VLOG(2) << "size miss match! actual size:"  << data_buffer_.size()
                << " expected size: "
                << (response_content_length_ - response_header_size_);
        Complete(false);
        return;
      }
    }
    Complete(true);
  } else if (result > 0) {
    if (!AppendDataBytes(tmp_buffer_.get(), result)) {
      return;
    }
    ReadBody();
  } else {
    VLOG(2) << "PepperURLLoader::OnRead ERROR!" << url_;
    Complete(false);
  }
}

void PepperURLLoader::ReadBody() {
  pp::CompletionCallback completion_callback =
      cc_factory_.NewOptionalCallback(&PepperURLLoader::OnRead);
  int32 result = PP_OK;
  do {
    result = url_loader_->ReadResponseBody(tmp_buffer_.get(),
                                           kReadBufferSize,
                                           completion_callback);
    if (result > 0) {
      if (!AppendDataBytes(tmp_buffer_.get(), result)) {
        return;
      }
    }
  } while (result > 0);

  if (result != PP_OK_COMPLETIONPENDING) {
    completion_callback.Run(result);
  }
}

bool PepperURLLoader::Start(int32 timeout_millisec, string *output_string) {
  pp::Module::Get()->core()->CallOnMainThread(
      0,
      cc_factory_.NewCallback(&PepperURLLoader::StartImpl));
  event_.Wait(timeout_millisec);
  VLOG(2) << "PepperURLLoader::GetResult Wait Done";
  bool deletable = false;
  bool result = false;
  {
    scoped_lock l(&mutex_);
    if (finished_) {
      VLOG(2) << "PepperURLLoader::GetResult finished";
      output_string->swap(data_buffer_);
      result = result_;
      deletable = true;
    } else {
      VLOG(2) << "PepperURLLoader::GetResult timeout";
      // PepperURLLoader will be deleted by itself.
      timeouted_ = true;
    }
  }
  if (deletable) {
    delete this;
  }
  return result;
}

bool PepperURLLoader::CheckTimeouted() {
  scoped_lock l(&mutex_);
  return timeouted_;
}

void PepperURLLoader::Complete(bool result) {
  VLOG(2) << "PepperURLLoader::Complete: " << result;
  bool deletable = false;
  {
    scoped_lock l(&mutex_);
    result_ = result;
    if (timeouted_) {
      VLOG(2) << "PepperURLLoader::Complete timeouted";
      deletable = true;
    } else {
      finished_ = true;
      VLOG(2) << "PepperURLLoader::Complete finished";
      event_.Notify();
    }
  }
  if (deletable) {
    delete this;
  }
}

}  // namespace

bool PepperHTTPRequestHandler::Request(HTTPMethodType type,
                                       const string &url,
                                       const char *post_data,
                                       size_t post_size,
                                       const HTTPClient::Option &option,
                                       string *output_string) {
  CHECK(!pp::Module::Get()->core()->IsMainThread());
  CHECK(g_pepper_instance);
  // loader will be deleted by itself.
  PepperURLLoader *loader = new PepperURLLoader(type,
                                                url,
                                                post_data,
                                                post_size,
                                                option);
  return loader->Start(option.timeout, output_string);
}

void RegisterPepperInstanceForHTTPClient(pp::Instance *instance) {
  g_pepper_instance = instance;
}

pp::Instance *GetPepperInstanceForHTTPClient() {
  return g_pepper_instance;
}

}  // namespace mozc
