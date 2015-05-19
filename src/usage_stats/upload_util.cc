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

#include "usage_stats/upload_util.h"

#include "base/number_util.h"
#include "base/util.h"
#include "net/http_client.h"

namespace mozc {
namespace usage_stats {
namespace {
const char kStatServerAddress[] =
    "http://clients4.google.com/tbproxy/usagestats";
const char kStatServerSecureAddress[] =
    "https://clients4.google.com/tbproxy/usagestats";
const char kStatServerSourceId[] = "sourceid=ime";
const char kStatServerAddedSendHeader[] =
    "Content-Type: application/x-www-form-urlencoded";
}  // namespace

UploadUtil::UploadUtil()
    : use_https_(false) {
}

UploadUtil::~UploadUtil() {
}

void UploadUtil::SetHeader(
    const string &type,
    int elapsed_sec,
    const vector<pair<string, string> > &optional_url_params) {
  if (elapsed_sec < 0) {
    LOG(WARNING) << "elapsed_sec < 0";
    elapsed_sec = 0;
  }
  stat_header_ = type + "&" +
      NumberUtil::SimpleItoa(static_cast<uint32>(elapsed_sec));
  optional_url_params_ = optional_url_params;
}

void UploadUtil::SetUseHttps(bool use_https) {
  use_https_ = use_https;
}

void UploadUtil::AddCountValue(const string &name, uint32 count) {
  string encoded_name;
  Util::EncodeURI(name, &encoded_name);
  stat_values_.append("&");
  stat_values_.append(encoded_name);
  stat_values_.append(":c=");
  stat_values_.append(NumberUtil::SimpleItoa(count));
}

void UploadUtil::AddTimingValue(const string &name, uint32 num_timings,
                                uint32 avg_time, uint32 min_time,
                                uint32 max_time) {
  string encoded_name;
  Util::EncodeURI(name, &encoded_name);
  stat_values_.append("&");
  stat_values_.append(encoded_name);
  stat_values_.append(":t=");
  stat_values_.append(NumberUtil::SimpleItoa(num_timings));
  stat_values_.append(";");
  stat_values_.append(NumberUtil::SimpleItoa(avg_time));
  stat_values_.append(";");
  stat_values_.append(NumberUtil::SimpleItoa(min_time));
  stat_values_.append(";");
  stat_values_.append(NumberUtil::SimpleItoa(max_time));
}

void UploadUtil::AddIntegerValue(const string &name, int int_value) {
  string encoded_name;
  Util::EncodeURI(name, &encoded_name);
  stat_values_.append("&");
  stat_values_.append(encoded_name);
  stat_values_.append(":i=");
  stat_values_.append(NumberUtil::SimpleItoa(int_value));
}

void UploadUtil::AddBooleanValue(const string &name, bool boolean_value) {
  string encoded_name;
  Util::EncodeURI(name, &encoded_name);
  stat_values_.append("&");
  stat_values_.append(encoded_name);
  stat_values_.append(":b=");
  stat_values_.append(boolean_value ? "t" : "f");
}

void UploadUtil::RemoveAllValues() {
  stat_values_.clear();
}

bool UploadUtil::Upload() {
  DCHECK(!stat_header_.empty());
  const string header_values = stat_header_ + stat_values_;
  string url =
      string(use_https_ ? kStatServerSecureAddress : kStatServerAddress) +
      "?" + string(kStatServerSourceId);
  if (!optional_url_params_.empty()) {
    url.append("&");
    Util::AppendCGIParams(optional_url_params_, &url);
  }

  string response;
  HTTPClient::Option option;
  option.timeout = 30000;  // 30 sec
  // TODO(toshiyuki): is this reasonable?
  option.max_data_size = 8192;
  option.headers.push_back(kStatServerAddedSendHeader);
  if (!HTTPClient::Post(url, header_values, option, &response)) {
    LOG(ERROR) << "Cannot connect to " << url;
    return false;
  }
  VLOG(3) << response;
  return true;
}
}  // namespace usage_stats
}  // namespace mozc
