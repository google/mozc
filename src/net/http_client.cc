// Copyright 2010-2020, Google Inc.
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

#include "net/http_client.h"

#ifdef GOOGLE_JAPANESE_INPUT_BUILD

#if defined(OS_WIN)
#include <windows.h>
#include <wininet.h>
#elif defined(HAVE_CURL)
#include <curl/curl.h>
#endif  // defined(OS_WIN), defined(HAVE_CURL)

#endif  // GOOGLE_JAPANESE_INPUT_BUILD

#include "base/logging.h"
#include "base/port.h"
#include "base/singleton.h"
#include "base/stopwatch.h"
#include "base/util.h"
#include "net/http_client_common.h"

#ifdef GOOGLE_JAPANESE_INPUT_BUILD
#if defined(__APPLE__)
#include "net/http_client_mac.h"
#elif defined(OS_NACL)  // __APPLE__
#include "net/http_client_null.h"
#endif  // __APPLE__ or OS_NACL
#else   // GOOGLE_JAPANESE_INPUT_BUILD
#include "net/http_client_null.h"
#endif  // GOOGLE_JAPANESE_INPUT_BUILD

#include "net/proxy_manager.h"

namespace mozc {
// We use a dummy user agent.
const char *kUserAgent = "Mozilla/5.0";
const int kOKResponseCode = 200;

namespace {
class HTTPStream {
 public:
  HTTPStream(std::string *output_string, size_t max_data_size)
      : output_string_(output_string),
        max_data_size_(max_data_size),
        output_size_(0) {
    if (nullptr != output_string_) {
      output_string_->clear();
    }
    VLOG(2) << "max_data_size=" << max_data_size;
  }

  virtual ~HTTPStream() { VLOG(2) << output_size_ << " bytes received"; }

  size_t Append(const char *buf, size_t size) {
    if (output_size_ + size >= max_data_size_) {
      size = (max_data_size_ - output_size_);
      LOG(WARNING) << "too long data max_data_size=" << max_data_size_;
    }

    if (output_string_ != nullptr) {
      VLOG(2) << "Recived: " << size << " bytes to std::string";
      output_string_->append(buf, size);
    }

    output_size_ += size;

    return size;
  }

 private:
  std::string *output_string_;
  size_t max_data_size_;
  size_t output_size_;
};

#ifdef GOOGLE_JAPANESE_INPUT_BUILD

#ifdef OS_WIN
// RAII class for HINTERNET
class ScopedHINTERNET {
 public:
  explicit ScopedHINTERNET(HINTERNET internet) : internet_(internet) {}
  virtual ~ScopedHINTERNET() {
    if (nullptr != internet_) {
      VLOG(2) << "InternetCloseHandle() called";
      ::InternetCloseHandle(internet_);
    }
    internet_ = nullptr;
  }

  HINTERNET get() { return internet_; }

 private:
  HINTERNET internet_;
};

void CALLBACK StatusCallback(HINTERNET internet, DWORD_PTR context,
                             DWORD status, LPVOID status_info,
                             DWORD status_info_size) {
  if (status == INTERNET_STATUS_REQUEST_COMPLETE) {
    ::SetEvent(reinterpret_cast<HANDLE>(context));
  }
  if (status == INTERNET_STATUS_HANDLE_CLOSING) {
    ::CloseHandle(reinterpret_cast<HANDLE>(context));
  }
}

bool CheckTimeout(HANDLE event, int64 elapsed_msec, int32 timeout_msec) {
  const DWORD error = ::GetLastError();
  if (error != ERROR_IO_PENDING) {
    LOG(ERROR) << "Unexpected error state: " << error;
    return false;
  }
  const int64 time_left = timeout_msec - elapsed_msec;
  if (time_left < 0) {
    LOG(WARNING) << "Already timed-out: " << time_left;
    return false;
  }
  DCHECK_GE(elapsed_msec, 0);
  DCHECK_LE(time_left, static_cast<int64>(MAXDWORD))
      << "This should always be true because |timeout_msec| <= MAXDWORD";
  const DWORD positive_time_left = static_cast<DWORD>(time_left);
  const DWORD wait_result = ::WaitForSingleObject(event, positive_time_left);
  if (wait_result == WAIT_FAILED) {
    const DWORD wait_error = ::GetLastError();
    LOG(ERROR) << "WaitForSingleObject failed. error: " << wait_error;
    return false;
  }
  if (wait_result == WAIT_TIMEOUT) {
    LOG(WARNING) << "WaitForSingleObject timed out after " << positive_time_left
                 << " msec.";
    return false;
  }
  if (wait_result != WAIT_OBJECT_0) {
    LOG(ERROR) << "WaitForSingleObject returned unexpected result: "
               << wait_result;
    return false;
  }
  ::ResetEvent(event);
  return true;
}

bool RequestInternal(HTTPMethodType type, const string &url,
                     const char *post_data, size_t post_size,
                     const HTTPClient::Option &option, string *output_string) {
  if (option.timeout <= 0) {
    LOG(ERROR) << "timeout should not be negative nor 0";
    return false;
  }

  Stopwatch stopwatch = stopwatch.StartNew();

  HANDLE event = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
  if (nullptr == event) {
    LOG(ERROR) << "CreateEvent() failed: " << ::GetLastError();
    return false;
  }

  ScopedHINTERNET internet(
      ::InternetOpenA(kUserAgent, INTERNET_OPEN_TYPE_PRECONFIG, nullptr,
                      nullptr, INTERNET_FLAG_ASYNC));

  if (nullptr == internet.get()) {
    LOG(ERROR) << "InternetOpen() failed: " << ::GetLastError() << " " << url;
    ::CloseHandle(event);
    return false;
  }

  ::InternetSetStatusCallback(internet.get(), StatusCallback);

  URL_COMPONENTSW uc;
  wchar_t Scheme[128];
  wchar_t HostName[512];
  wchar_t UserName[64];
  wchar_t Password[64];
  wchar_t UrlPath[256];
  wchar_t ExtraInfo[512];

  uc.dwStructSize = sizeof(uc);
  uc.lpszScheme = Scheme;
  uc.lpszHostName = HostName;
  uc.lpszUserName = UserName;
  uc.lpszPassword = Password;
  uc.lpszUrlPath = UrlPath;
  uc.lpszExtraInfo = ExtraInfo;

  uc.dwSchemeLength = sizeof(Scheme);
  uc.dwHostNameLength = sizeof(HostName);
  uc.dwUserNameLength = sizeof(UserName);
  uc.dwPasswordLength = sizeof(Password);
  uc.dwUrlPathLength = sizeof(UrlPath);
  uc.dwExtraInfoLength = sizeof(ExtraInfo);

  std::wstring wurl;
  Util::UTF8ToWide(url, &wurl);

  if (!::InternetCrackUrlW(wurl.c_str(), 0, 0, &uc)) {
    LOG(WARNING) << "InternetCrackUrl() failed: " << ::GetLastError() << " "
                 << url;
    return false;
  }

  if (uc.nScheme != INTERNET_SCHEME_HTTP &&
      uc.nScheme != INTERNET_SCHEME_HTTPS) {
    LOG(WARNING) << "Only accept HTTP or HTTPS: " << url;
    return false;
  }

  ScopedHINTERNET session(::InternetConnect(internet.get(), uc.lpszHostName,
                                            uc.nPort, nullptr, nullptr,
                                            INTERNET_SERVICE_HTTP, 0, 0));

  if (nullptr == session.get()) {
    LOG(ERROR) << "InternetConnect() failed: " << ::GetLastError() << " "
               << url;
    return false;
  }

  std::wstring uri = UrlPath;
  if (uc.dwExtraInfoLength != 0) {
    uri += uc.lpszExtraInfo;
  }

  const wchar_t *method_type_string[] = {L"GET", L"HEAD", L"POST", L"PUT",
                                         L"DELETE"};
  const wchar_t *method = method_type_string[static_cast<int>(type)];
  CHECK(method);

  ScopedHINTERNET handle(::HttpOpenRequestW(
      session.get(), method, uri.c_str(), nullptr, nullptr, nullptr,
      INTERNET_FLAG_RELOAD | INTERNET_FLAG_DONT_CACHE | INTERNET_FLAG_NO_UI |
          INTERNET_FLAG_PRAGMA_NOCACHE |
          (uc.nScheme == INTERNET_SCHEME_HTTPS ? INTERNET_FLAG_SECURE : 0),
      reinterpret_cast<DWORD_PTR>(event)));

  if (nullptr == handle.get()) {
    LOG(ERROR) << "HttpOpenRequest() failed: " << ::GetLastError() << " "
               << url;
    return false;
  }

  for (size_t i = 0; i < option.headers.size(); ++i) {
    const string header = option.headers[i] + "\r\n";
    std::wstring wheader;
    Util::UTF8ToWide(header, &wheader);
    if (!::HttpAddRequestHeadersW(
            handle.get(), wheader.c_str(), -1,
            HTTP_ADDREQ_FLAG_ADD | HTTP_ADDREQ_FLAG_REPLACE)) {
      LOG(WARNING) << "HttpAddRequestHeaders() failed: " << option.headers[i]
                   << " " << ::GetLastError();
      return false;
    }
  }

  if (!::HttpSendRequest(handle.get(), nullptr, 0,
                         (type == HTTP_POST) ? (LPVOID)post_data : nullptr,
                         (type == HTTP_POST) ? post_size : 0)) {
    if (!CheckTimeout(event, stopwatch.GetElapsedMilliseconds(),
                      option.timeout)) {
      LOG(ERROR) << "HttpSendRequest() failed: " << ::GetLastError() << " "
                 << url;
      return false;
    }
  } else {
    return false;
  }

  if (VLOG_IS_ON(2)) {
    char buf[8192];
    DWORD size = sizeof(buf);
    if (::HttpQueryInfoA(
            handle.get(),
            HTTP_QUERY_RAW_HEADERS_CRLF | HTTP_QUERY_FLAG_REQUEST_HEADERS, buf,
            &size, 0)) {
      LOG(INFO) << "Request Header: " << buf;
    }
  }

  DWORD code = 0;
  {
    DWORD code_size = sizeof(code);
    if (!::HttpQueryInfoW(handle.get(),
                          HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                          &code, &code_size, 0)) {
      LOG(ERROR) << "HttpQueryInfo() failed: " << ::GetLastError() << " "
                 << url;
      return false;
    }
  }

  // make stream
  HTTPStream stream(output_string, option.max_data_size);

  if (option.include_header || type == HTTP_HEAD) {
    char buf[8192];
    DWORD buf_size = sizeof(buf);
    if (!::HttpQueryInfoA(handle.get(), HTTP_QUERY_RAW_HEADERS_CRLF, buf,
                          &buf_size, 0)) {
      LOG(ERROR) << "HttpQueryInfo() failed: " << ::GetLastError() << " "
                 << url;
      return false;
    }

    if (buf_size != stream.Append(buf, buf_size)) {
      return false;
    }

    if (type == HTTP_HEAD) {
      return true;
    }
  }

  if (type == HTTP_POST || type == HTTP_GET) {
    char buf[8129];
    INTERNET_BUFFERSA ibuf;
    ::ZeroMemory(&ibuf, sizeof(ibuf));
    ibuf.dwStructSize = sizeof(ibuf);
    while (true) {
      ibuf.lpvBuffer = buf;
      ibuf.dwBufferLength = sizeof(buf);
      if (::InternetReadFileExA(handle.get(), &ibuf, WININET_API_FLAG_ASYNC,
                                reinterpret_cast<DWORD_PTR>(event)) ||
          CheckTimeout(event, stopwatch.GetElapsedMilliseconds(),
                       option.timeout)) {
        const DWORD size = ibuf.dwBufferLength;
        if (size == 0) {
          break;
        }
        if (size != stream.Append(buf, size)) {
          return false;
        }
      } else {
        LOG(ERROR) << "InternetReadFileEx() failed: " << ::GetLastError();
        return false;
      }
    }
  }

  if (kOKResponseCode != code) {
    LOG(WARNING) << "status code is not 200: " << code << " " << url;
    return false;
  }

  return true;
}

#elif defined(__APPLE__)  // OS_WIN

bool RequestInternal(HTTPMethodType type, const string &url,
                     const char *post_data, size_t post_size,
                     const HTTPClient::Option &option, string *output_string) {
  return MacHTTPRequestHandler::Request(type, url, post_data, post_size, option,
                                        output_string);
}

#elif defined(OS_NACL)  // OS_WIN, __APPLE__

bool RequestInternal(HTTPMethodType type, const string &url,
                     const char *post_data, size_t post_size,
                     const HTTPClient::Option &option, string *output_string) {
  return false;
}

#elif defined(HAVE_CURL)

class CurlInitializer {
 public:
  CurlInitializer() { curl_global_init(CURL_GLOBAL_ALL); }

  ~CurlInitializer() { curl_global_cleanup(); }

  void Init() {}
};

size_t HTTPOutputCallback(void *ptr, size_t size, size_t nmemb, void *stream) {
  HTTPStream *s = reinterpret_cast<HTTPStream *>(stream);
  return s->Append(reinterpret_cast<const char *>(ptr), size * nmemb);
}

int HTTPDebugCallback(CURL *curl, curl_infotype type, char *buf, size_t size,
                      void *data) {
  if (CURLINFO_TEXT != type) {
    return 0;
  }
  std::string *output = reinterpret_cast<std::string *>(data);
  output->append(buf, size);
  return 0;
}

bool RequestInternal(HTTPMethodType type, const std::string &url,
                     const char *post_data, size_t post_size,
                     const HTTPClient::Option &option,
                     std::string *output_string) {
  if (option.timeout < 0) {
    LOG(ERROR) << "timeout should not be negative nor 0";
    return false;
  }

  Singleton<CurlInitializer>::get()->Init();

  CURL *curl = curl_easy_init();
  if (nullptr == curl) {
    LOG(ERROR) << "curl_easy_init() failed";
    return false;
  }

  HTTPStream stream(output_string, option.max_data_size);

  std::string debug;
  if (VLOG_IS_ON(2)) {
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
    curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, HTTPDebugCallback);
    curl_easy_setopt(curl, CURLOPT_DEBUGDATA, reinterpret_cast<void *>(&debug));
  }

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
  curl_easy_setopt(curl, CURLOPT_AUTOREFERER, 1);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, kUserAgent);
  curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, option.timeout);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, option.timeout);
  curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &HTTPOutputCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, reinterpret_cast<void *>(&stream));

  std::string proxy_host;
  std::string proxy_auth;
  if (ProxyManager::GetProxyData(url, &proxy_host, &proxy_auth)) {
    curl_easy_setopt(curl, CURLOPT_PROXY, proxy_host.c_str());
    if (!proxy_auth.empty()) {
      curl_easy_setopt(curl, CURLOPT_PROXYUSERPWD, proxy_auth.c_str());
    }
  }

  struct curl_slist *slist = nullptr;
  for (size_t i = 0; i < option.headers.size(); ++i) {
    VLOG(2) << "Add header: " << option.headers[i];
    slist = curl_slist_append(slist, option.headers[i].c_str());
  }

  if (slist != nullptr) {
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
  }

  if (option.include_header) {
    curl_easy_setopt(curl, CURLOPT_HEADER, 1);
  }

  switch (type) {
    case HTTP_GET:
      curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
      break;
    case HTTP_POST:
      curl_easy_setopt(curl, CURLOPT_HTTPPOST, 1);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, post_size);
      break;
    case HTTP_HEAD:
      curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "HEAD");
      curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
      curl_easy_setopt(curl, CURLOPT_HEADER, 1);
      break;
    default:
      LOG(ERROR) << "Unknown method: " << type;
      curl_easy_cleanup(curl);
      return false;
      break;
  }

  const CURLcode ret = curl_easy_perform(curl);
  bool result = (CURLE_OK == ret);

  if (!result) {
    LOG(WARNING) << "curl_easy_perform() failed: " << curl_easy_strerror(ret);
  }

  int code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
  if (kOKResponseCode != code) {
    LOG(WARNING) << "status code is not 200: " << code;
    result = false;
  }

  curl_easy_cleanup(curl);
  if (slist != nullptr) {
    curl_slist_free_all(slist);
  }

  if (VLOG_IS_ON(2)) {
    LOG(INFO) << debug;
  }

  return result;
}
#else  // OS_WIN, __APPLE__, OS_NACL, HAVE_CURL
#error "HttpClient does not support your platform."
#endif  // OS_WIN, __APPLE__, OS_NACL, HAVE_CURL

#else  // GOOGLE_JAPANESE_INPUT_BUILD

bool RequestInternal(HTTPMethodType type, const string &url,
                     const char *post_data, size_t post_size,
                     const HTTPClient::Option &option, string *output_string) {
  return NullHTTPRequestHandler::Request(type, url, post_data, post_size,
                                         option, output_string);
}

#endif  // GOOGLE_JAPANESE_INPUT_BUILD

}  // namespace

class HTTPClientImpl : public HTTPClientInterface {
 public:
  bool Get(const std::string &url, const HTTPClient::Option &option,
           std::string *output_string) const override {
    return RequestInternal(HTTP_GET, url, nullptr, 0, option, output_string);
  }

  bool Head(const std::string &url, const HTTPClient::Option &option,
            std::string *output_string) const override {
    return RequestInternal(HTTP_HEAD, url, nullptr, 0, option, output_string);
  }

  bool Post(const std::string &url, const std::string &data,
            const HTTPClient::Option &option,
            std::string *output_string) const override {
    return RequestInternal(HTTP_POST, url, data.data(), data.size(), option,
                           output_string);
  }
};

namespace {

const HTTPClientInterface *g_http_connection_handler = nullptr;

const HTTPClientInterface &GetHTTPClient() {
  if (g_http_connection_handler == nullptr) {
    g_http_connection_handler = Singleton<HTTPClientImpl>::get();
  }
  return *g_http_connection_handler;
}
}  // namespace

void HTTPClient::SetHTTPClientHandler(const HTTPClientInterface *handler) {
  g_http_connection_handler = handler;
}

bool HTTPClient::Get(const std::string &url, std::string *output_string) {
  return GetHTTPClient().Get(url, Option(), output_string);
}

bool HTTPClient::Head(const std::string &url, std::string *output_string) {
  return GetHTTPClient().Head(url, Option(), output_string);
}

bool HTTPClient::Post(const std::string &url, const std::string &data,
                      std::string *output_string) {
  return GetHTTPClient().Post(url, data, Option(), output_string);
}

bool HTTPClient::Get(const std::string &url, const Option &option,
                     std::string *output_string) {
  return GetHTTPClient().Get(url, option, output_string);
}

bool HTTPClient::Head(const std::string &url, const Option &option,
                      std::string *output_string) {
  return GetHTTPClient().Head(url, option, output_string);
}

bool HTTPClient::Post(const std::string &url, const std::string &data,
                      const Option &option, std::string *output_string) {
  return GetHTTPClient().Post(url, data, option, output_string);
}
}  // namespace mozc
