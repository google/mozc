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

// Upload util for usage stats
// example:
//
// UploadUtil uploader;
// vector<pair<string, string> > params;
// params.push_back(make_pair("hl", "ja"));
// params.push_back(make_pair("v", "0.0.0.0"));
// uploader.SetHeader("Daily", 100, params);
// uploader.AddBooleanValue("Boolean", false);
// bool result = uploader.AttemptUpload();

#ifndef MOZC_USAGE_STATS_UPLOAD_UTIL_H_
#define MOZC_USAGE_STATS_UPLOAD_UTIL_H_

#include <vector>
#include <string>
#include <utility>
#include "base/base.h"

namespace mozc {
namespace usage_stats {
class UploadUtil {
 public:
  UploadUtil();
  virtual ~UploadUtil();

  // Sets usage stats header and optional url params.
  // usage stats data format is:
  // <type>&<elapsed_sec>&<name1>=<val1> ...
  // type is transmission type (Daily, Weekly, etc.)
  // elapsed_sec is the time elapsed since the last transmission.
  void SetHeader(const string &type,
                 int elapsed_sec,
                 const vector<pair<string, string> > &optional_url_params);

  // Adds count data.
  // &<name>:c=<count>
  void AddCountValue(const string &name, uint32 count);

  // Adds timing data.
  // &<name>:t=<num_timings>;<avg_time>;<min_time>;<max_time>
  void AddTimingValue(const string &name, uint32 num_timings, uint32 avg_time,
                      uint32 min_time, uint32 max_time);

  // Adds integer data.
  // &<name>:i=<value>
  void AddIntegerValue(const string &name, int int_value);

  // Adds boolean data.
  // &<name>:b=<value>
  void AddBooleanValue(const string &name, bool boolean_value);

  // Remove all data
  void RemoveAllValues();

  // Returns false when upload failed.
  bool Upload();

 private:
  string stat_header_;
  string stat_values_;
  vector<pair<string, string> > optional_url_params_;
  DISALLOW_COPY_AND_ASSIGN(UploadUtil);
};
}  // namespace mozc::usage_stats
}  // namespace mozc
#endif  // MOZC_USAGE_STATS_UPLOAD_UTIL_H_
