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

#include "base/url.h"

#include <string>
#include <vector>

#include "base/util.h"
#include "base/version.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace {
const char kSurveyBaseURL[] =
    "http://www.google.com/support/ime/japanese/bin/request.py";

bool FindEncodedParam(const vector<string> &params,
                      const string &key, const string &value) {
  string encoded;
  Util::EncodeURI(value, &encoded);
  const string param = key + "=" + encoded;
  for (size_t i = 0; i < params.size(); ++i) {
    if (params[i] == param) {
      return true;
    }
  }
  return false;
}
}  // namespace

TEST(URLTest, UninstallationSurveyURL) {
  string url;
  URL::GetUninstallationSurveyURL("0.1.2.3", &url);
  vector<string> url_and_params;
  Util::SplitStringUsing(url, "?", &url_and_params);
  EXPECT_EQ(2, url_and_params.size());
  EXPECT_EQ(kSurveyBaseURL, url_and_params[0]);
  vector<string> params;
  Util::SplitStringUsing(url_and_params[1], "&", &params);
  EXPECT_EQ(4, params.size());
  EXPECT_TRUE(FindEncodedParam(params, "contact_type", "surveyime"));
  EXPECT_TRUE(FindEncodedParam(params, "hl", "jp"));
  EXPECT_TRUE(FindEncodedParam(params, "format", "inproduct"));
  EXPECT_TRUE(FindEncodedParam(params, "version", "0.1.2.3"));
}

TEST(URLTest, UninstallationSurveyURLWithNoVersion) {
  string url;
  URL::GetUninstallationSurveyURL("", &url);
  vector<string> url_and_params;
  Util::SplitStringUsing(url, "?", &url_and_params);
  EXPECT_EQ(2, url_and_params.size());
  EXPECT_EQ(kSurveyBaseURL, url_and_params[0]);
  vector<string> params;
  Util::SplitStringUsing(url_and_params[1], "&", &params);
  EXPECT_EQ(3, params.size());
  EXPECT_TRUE(FindEncodedParam(params, "contact_type", "surveyime"));
  EXPECT_TRUE(FindEncodedParam(params, "hl", "jp"));
  EXPECT_TRUE(FindEncodedParam(params, "format", "inproduct"));
}
}  // namespace mozc
