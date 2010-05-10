// Copyright 2010, Google Inc.
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

#ifdef OS_WINDOWS
#include <windows.h>
#include <wininet.h>
#else
#include "curl/curl.h"
#include "net/proxy_manager.h"
#endif  // OS_WINDOWS

#include "base/base.h"
#include "base/singleton.h"
#include "base/util.h"

namespace mozc {

namespace {

enum MethodType {
  GET, HEAD, POST,
};

const int kOKResponseCode = 200;
const char kUserAgent[] = "Mozilla/5.0";

class HTTPStream {
 public:
  HTTPStream(string *output_string, ostream *output_stream,
             size_t max_data_size)
      : output_string_(output_string),
        output_stream_(output_stream),
        max_data_size_(max_data_size),
        output_size_(0) {
    if (NULL != output_string_) {
      output_string_->clear();
    }
    VLOG(2) << "max_data_size=" << max_data_size;
  }

  virtual ~HTTPStream() {
    VLOG(2) << output_size_ << " bytes received";
  }

  size_t Append(const char *buf, size_t size) {
    if (output_size_ + size >= max_data_size_) {
      size = (max_data_size_ - output_size_);
      LOG(WARNING) << "too long data max_data_size=" << max_data_size_;
    }

    if (output_string_ != NULL) {
      VLOG(2) << "Recived: " << size << " bytes to std::string";
      output_string_->append(buf, size);
    }

    if (output_stream_ != NULL) {
      VLOG(2) << "Recived: " << size << " bytes to std::ostream";
      output_stream_->write(buf, size);
    }

    output_size_ += size;

    return size;
  }

 private:
  string  *output_string_;
  ostream *output_stream_;
  size_t   max_data_size_;
  size_t   output_size_;
};

#ifdef OS_WINDOWS
// RAII class for HINTERNET
class ScopedHINTERNET {
 public:
  explicit ScopedHINTERNET(HINTERNET internet)
      : internet_(internet) {}
  virtual ~ScopedHINTERNET() {
    if (NULL != internet_) {
      VLOG(2) << "InternetCloseHandle() called";
      ::InternetCloseHandle(internet_);
    }
    internet_ = NULL;
  }

  HINTERNET get() {
    return internet_;
  };

 private:
  HINTERNET internet_;
};

void CALLBACK StatusCallback(HINTERNET internet,
                             DWORD_PTR context,
                             DWORD status,
                             LPVOID status_info,
                             DWORD status_info_size) {
  if (status == INTERNET_STATUS_REQUEST_COMPLETE) {
    ::SetEvent(reinterpret_cast<HANDLE>(context));
  }
  if (status == INTERNET_STATUS_HANDLE_CLOSING) {
    ::CloseHandle(reinterpret_cast<HANDLE>(context));
  }
}

bool CheckTimeout(HANDLE event, DWORD target_time) {
  const DWORD error = ::GetLastError();
  if (error == ERROR_IO_PENDING) {
    const DWORD time_left = target_time - ::timeGetTime();
    if (time_left > 0 &&
        WAIT_OBJECT_0 == ::WaitForSingleObject(event, time_left)) {
      ::ResetEvent(event);
      return true;
    } else {
      LOG(WARNING) << "Timeout: " << time_left;
      return false;
    }
  }

  LOG(ERROR) << error;
  return false;
}

bool RequestInternal(MethodType type,
                     const string &url,
                     const char *post_data,
                     size_t post_size,
                     const HTTPClient::Option &option,
                     string *output_string,
                     ostream *output_stream) {
  if (option.timeout <= 0) {
    LOG(ERROR) << "timeout should not be negative nor 0";
    return false;
  }

  const DWORD target_time = ::timeGetTime() + option.timeout;

  HANDLE event = ::CreateEvent(NULL, FALSE, FALSE, NULL);
  if (NULL == event) {
    LOG(ERROR) << "CreateEvent() failed: " << ::GetLastError();
    return false;
  }

  ScopedHINTERNET internet(::InternetOpenA(kUserAgent,
                                           INTERNET_OPEN_TYPE_PRECONFIG,
                                           NULL,
                                           NULL,
                                           INTERNET_FLAG_ASYNC));

  if (NULL == internet.get()) {
    LOG(ERROR) << "InternetOpen() failed: "
               << ::GetLastError() << " " << url;
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

  uc.dwStructSize  = sizeof(uc);
  uc.lpszScheme    = Scheme;
  uc.lpszHostName  = HostName;
  uc.lpszUserName  = UserName;
  uc.lpszPassword  = Password;
  uc.lpszUrlPath   = UrlPath;
  uc.lpszExtraInfo = ExtraInfo;

  uc.dwSchemeLength    = sizeof(Scheme);
  uc.dwHostNameLength  = sizeof(HostName);
  uc.dwUserNameLength  = sizeof(UserName);
  uc.dwPasswordLength  = sizeof(Password);
  uc.dwUrlPathLength   = sizeof(UrlPath);
  uc.dwExtraInfoLength = sizeof(ExtraInfo);

  wstring wurl;
  Util::UTF8ToWide(url.c_str(), &wurl);

  if (!::InternetCrackUrlW(wurl.c_str(), 0, 0, &uc)) {
    LOG(WARNING) << "InternetCrackUrl() failed: "
                 << ::GetLastError() << " " << url;
    return false;
  }

  if (uc.nScheme != INTERNET_SCHEME_HTTP &&
      uc.nScheme != INTERNET_SCHEME_HTTPS) {
    LOG(WARNING) << "Only accept HTTP or HTTPS: " << url;
    return false;
  }

  ScopedHINTERNET session(::InternetConnect(internet.get(),
                                            uc.lpszHostName,
                                            uc.nPort,
                                            NULL,
                                            NULL,
                                            INTERNET_SERVICE_HTTP,
                                            0,
                                            0));

  if (NULL == session.get()) {
    LOG(ERROR) << "InternetConnect() failed: "
               << ::GetLastError() << " " << url;
    return false;
  }

  wstring uri = UrlPath;
  if (uc.dwExtraInfoLength != 0) {
    uri += uc.lpszExtraInfo;
  }

  const wchar_t *method_type_string[]
      = { L"GET", L"HEAD", L"POST", L"PUT", L"DELETE" };
  const wchar_t *method = method_type_string[static_cast<int>(type)];
  CHECK(method);

  ScopedHINTERNET handle(::HttpOpenRequestW
                         (session.get(),
                          method,
                          uri.c_str(),
                          NULL, NULL, NULL,
                          INTERNET_FLAG_RELOAD |
                          INTERNET_FLAG_DONT_CACHE |
                          INTERNET_FLAG_NO_UI |
                          INTERNET_FLAG_PRAGMA_NOCACHE |
                          (uc.nScheme == INTERNET_SCHEME_HTTPS ?
                           INTERNET_FLAG_SECURE : 0),
                          reinterpret_cast<DWORD_PTR>(event)));

  if (NULL == handle.get()) {
    LOG(ERROR) << "HttpOpenRequest() failed: "
               << ::GetLastError() << " " << url;
    return false;
  }

  for (size_t i = 0; i < option.headers.size(); ++i) {
    const string header = option.headers[i] + "\r\n";
    wstring wheader;
    Util::UTF8ToWide(header.c_str(), &wheader);
    if (!::HttpAddRequestHeadersW(handle.get(), wheader.c_str(), -1,
                                  HTTP_ADDREQ_FLAG_ADD |
                                  HTTP_ADDREQ_FLAG_REPLACE)) {
      LOG(WARNING) << "HttpAddRequestHeaders() failed: " << option.headers[i]
                   << " " << ::GetLastError();
      return false;
    }
  }


  if (!::HttpSendRequest(handle.get(),
                         NULL, 0,
                         (type == POST) ? (LPVOID)post_data : NULL,
                         (type == POST) ? post_size : 0)) {
    if (!CheckTimeout(event, target_time)) {
      LOG(ERROR) << "HttpSendRequest() failed: "
                 << ::GetLastError() << " " << url;
      return false;
    }
  } else {
    return false;
  }


  if (VLOG_IS_ON(2)) {
    char buf[8192];
    DWORD size = sizeof(buf);
    if (::HttpQueryInfoA(handle.get(),
                         HTTP_QUERY_RAW_HEADERS_CRLF |
                         HTTP_QUERY_FLAG_REQUEST_HEADERS,
                         buf,
                         &size,
                         0)) {
      LOG(INFO) << "Request Header: " << buf;
    }
  }

  DWORD code = 0;
  DWORD size = sizeof(code);
  if (!::HttpQueryInfoW(handle.get(),
                        HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                        &code,
                        &size,
                        0)) {
    LOG(ERROR) << "HttpQueryInfo() failed: "
               << ::GetLastError() << " " << url;
    return false;
  }

  if (kOKResponseCode != code) {
    LOG(WARNING) << "status code is not 200: " << code << " " << url;
    return false;
  }

  // make stream
  HTTPStream stream(output_string, output_stream,
                    option.max_data_size);

  if (option.include_header || type == HEAD) {
    char buf[8192];
    DWORD size = sizeof(buf);
    if (!::HttpQueryInfoA(handle.get(),
                          HTTP_QUERY_RAW_HEADERS_CRLF,
                          buf,
                          &size,
                          0)) {
      LOG(ERROR) << "HttpQueryInfo() failed: "
                 << ::GetLastError() << " " << url;
      return false;
    }

    if (size != stream.Append(buf, size)) {
      return false;
    }

    if (type == HEAD) {
      return true;
    }
  }

  if (type == POST || type == GET) {
    char buf[8129];
    INTERNET_BUFFERSA ibuf;
    ::ZeroMemory(&ibuf, sizeof(ibuf));
    ibuf.dwStructSize = sizeof(ibuf);
    while (true) {
      ibuf.lpvBuffer = buf;
      ibuf.dwBufferLength = sizeof(buf);
      if (::InternetReadFileExA(handle.get(),
                                &ibuf,
                                WININET_API_FLAG_ASYNC,
                                reinterpret_cast<DWORD_PTR>(event)) ||
          CheckTimeout(event, target_time)) {
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

  return true;
}

#else   // #ifdef OS_WINDOWS

class CurlInitializer {
 public:
  CurlInitializer() {
    curl_global_init(CURL_GLOBAL_ALL);
  }

  ~CurlInitializer() {
    curl_global_cleanup();
  }

  void Init() {}
};

size_t HTTPOutputCallback(void *ptr, size_t size, size_t nmemb, void *stream) {
  HTTPStream *s = reinterpret_cast<HTTPStream *>(stream);
  return s->Append(reinterpret_cast<const char *>(ptr), size * nmemb);
}

int HTTPDebugCallback(CURL *curl, curl_infotype type,
                      char *buf, size_t size, void *data) {
  if (CURLINFO_TEXT != type) {
    return 0;
  }
  string *output = reinterpret_cast<string *>(data);
  output->append(buf, size);
  return 0;
}

bool RequestInternal(MethodType type,
                     const string &url,
                     const char *post_data,
                     size_t post_size,
                     const HTTPClient::Option &option,
                     string *output_string,
                     ostream *output_stream) {
  if (option.timeout < 0) {
    LOG(ERROR) << "timeout should not be negative nor 0";
    return false;
  }

  Singleton<CurlInitializer>::get()->Init();

  CURL *curl = curl_easy_init();
  if (NULL == curl) {
    LOG(ERROR) << "curl_easy_init() failed";
    return false;
  }

  HTTPStream stream(output_string, output_stream,
                    option.max_data_size);

  string debug;
  if (VLOG_IS_ON(2)) {
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
    curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, HTTPDebugCallback);
    curl_easy_setopt(curl, CURLOPT_DEBUGDATA, reinterpret_cast<void *>(&debug));
  }

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
  curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
  curl_easy_setopt(curl, CURLOPT_AUTOREFERER, 1);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, kUserAgent);
  curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, option.timeout);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, option.timeout);
  curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &HTTPOutputCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA,
                   reinterpret_cast<void *>(&stream));

  string proxy_host;
  string proxy_auth;
  if (ProxyManager::GetProxyData(url, &proxy_host, &proxy_auth)) {
    curl_easy_setopt(curl, CURLOPT_PROXY, proxy_host.c_str());
    if (!proxy_auth.empty()) {
      curl_easy_setopt(curl, CURLOPT_PROXYUSERPWD, proxy_auth.c_str());
    }
  }

  struct curl_slist *slist = NULL;
  for (size_t i = 0; i < option.headers.size(); ++i) {
    VLOG(2) << "Add header: " << option.headers[i];
    slist = curl_slist_append(slist, option.headers[i].c_str());
  }

  if (slist != NULL) {
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
  }

  if (option.include_header) {
    curl_easy_setopt(curl, CURLOPT_HEADER, 1);
  }

  switch (type) {
    case GET:
      curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
      break;
    case POST:
      curl_easy_setopt(curl, CURLOPT_HTTPPOST, 1);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, post_size);
      break;
    case HEAD:
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

  if (result) {
    int code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    if (kOKResponseCode != code) {
      LOG(WARNING) << "status code is not 200: " << code;
      result = false;
    }
  }

  curl_easy_cleanup(curl);
  if (slist != NULL) {
    curl_slist_free_all(slist);
  }

  if (VLOG_IS_ON(2)) {
    LOG(INFO) << debug;
  }

  return result;
}
#endif
}  // namespace

class HTTPClientImpl: public HTTPClientInterface {
 public:
  virtual bool Get(const string &url, string *output_string) const {
    HTTPClient::Option option;
    return RequestInternal(GET, url, NULL, 0, option,
                           output_string, NULL);
  }

  virtual bool Head(const string &url, string *output_string) const {
    HTTPClient::Option option;
    return RequestInternal(HEAD, url, NULL, 0, option,
                           output_string, NULL);
  }

  virtual bool Post(const string &url, const string &data,
                    string *output_string) const {
    HTTPClient::Option option;
    return RequestInternal(POST, url, data.data(), data.size(),
                           option, output_string, NULL);
  }

  virtual bool Get(const string &url, const HTTPClient::Option &option,
                   string *output_string) const {
    return RequestInternal(GET, url, NULL, 0, option,
                           output_string, NULL);
  }

  virtual bool Head(const string &url, const HTTPClient::Option &option,
                    string *output_string) const {
    return RequestInternal(HEAD, url, NULL, 0, option,
                           output_string, NULL);
  }

  virtual bool Post(const string &url, const string &data,
                    const HTTPClient::Option &option,
                    string *output_string) const {
    return RequestInternal(POST, url, data.data(), data.size(),
                           option, output_string, NULL);
  }

  virtual bool Post(const string &url, const char *data,
                    size_t data_size,
                    const HTTPClient::Option &option,
                    string *output_string) const {
    return RequestInternal(POST, url, data, data_size, option,
                           output_string, NULL);
  }

  // stream
  virtual bool Get(const string &url, ostream *output_stream) const {
    HTTPClient::Option option;
    return RequestInternal(GET, url, NULL, 0, option,
                           NULL, output_stream);
  }

  virtual bool Head(const string &url, ostream *output_stream) const {
    HTTPClient::Option option;
    return RequestInternal(HEAD, url, NULL, 0, option,
                           NULL, output_stream);
  }

  virtual bool Post(const string &url, const string &data,
                    ostream *output_stream) const {
    HTTPClient::Option option;
    return RequestInternal(POST, url, data.data(), data.size(),
                           option, NULL, output_stream);
  }

  virtual bool Get(const string &url, const HTTPClient::Option &option,
                   ostream *output_stream) const {
    return RequestInternal(GET, url, NULL, 0, option,
                           NULL, output_stream);
  }

  virtual bool Head(const string &url, const HTTPClient::Option &option,
                    ostream *output_stream) const {
    return RequestInternal(HEAD, url, NULL, 0, option,
                           NULL, output_stream);
  }

  virtual bool Post(const string &url, const string &data,
                    const HTTPClient::Option &option,
                    ostream *output_stream) const {
    return RequestInternal(POST, url, data.data(), data.size(), option,
                           NULL, output_stream);
  }

  virtual bool Post(const string &url, const char *data,
                    size_t data_size,
                    const HTTPClient::Option &option,
                    ostream *output_stream) const {
    return RequestInternal(POST, url, data, data_size, option,
                           NULL, output_stream);
  }
};

namespace {

const HTTPClientInterface *g_http_connection_handler = NULL;

const HTTPClientInterface &GetHTTPClient() {
  if (g_http_connection_handler == NULL) {
    g_http_connection_handler = Singleton<HTTPClientImpl>::get();
  }
  return *g_http_connection_handler;
}
} // namespace

void HTTPClient::SetHTTPClientHandler(
    const HTTPClientInterface *handler) {
  g_http_connection_handler = handler;
}

bool HTTPClient::Get(const string &url, string *output_string) {
  return GetHTTPClient().Get(url, output_string);
}

bool HTTPClient::Head(const string &url, string *output_string) {
  return GetHTTPClient().Head(url, output_string);
}

bool HTTPClient::Post(const string &url, const string &data,
                      string *output_string) {
  return GetHTTPClient().Post(url, data, output_string);
}

bool HTTPClient::Get(const string &url, const HTTPClient::Option &option,
                     string *output_string) {
  return GetHTTPClient().Get(url, option, output_string);
}

bool HTTPClient::Head(const string &url, const HTTPClient::Option &option,
                      string *output_string) {
  return GetHTTPClient().Head(url, option, output_string);
}

bool HTTPClient::Post(const string &url, const string &data,
                      const HTTPClient::Option &option,
                      string *output_string) {
  return GetHTTPClient().Post(url, data, option, output_string);
}

bool HTTPClient::Post(const string &url, const char *data,
                      size_t data_size,
                      const HTTPClient::Option &option,
                      string *output_string) {
  return GetHTTPClient().Post(url, data, data_size, option, output_string);
}

// stream
bool HTTPClient::Get(const string &url, ostream *output_stream) {
  return GetHTTPClient().Get(url, output_stream);
}

bool HTTPClient::Head(const string &url, ostream *output_stream) {
  return GetHTTPClient().Head(url, output_stream);
}

bool HTTPClient::Post(const string &url, const string &data,
                      ostream *output_stream) {
  return GetHTTPClient().Post(url, data, output_stream);
}

bool HTTPClient::Get(const string &url, const HTTPClient::Option &option,
                     ostream *output_stream) {
  return GetHTTPClient().Get(url, option, output_stream);
}

bool HTTPClient::Head(const string &url, const HTTPClient::Option &option,
                      ostream *output_stream) {
  return GetHTTPClient().Head(url, option, output_stream);
}

bool HTTPClient::Post(const string &url, const string &data,
                      const HTTPClient::Option &option,
                      ostream *output_stream) {
  return GetHTTPClient().Post(url, data, option, output_stream);
}

bool HTTPClient::Post(const string &url, const char *data,
                      size_t data_size,
                      const HTTPClient::Option &option,
                      ostream *output_stream) {
  return GetHTTPClient().Post(url, data, data_size, option, output_stream);
}
}  // namespace mozc
