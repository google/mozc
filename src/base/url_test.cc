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

#include "base/url.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/util.h"
#include "testing/gunit.h"
#include "absl/strings/str_split.h"

namespace mozc {
namespace {

constexpr char kSurveyBaseUrl[] =
    "http://www.google.com/support/ime/japanese/bin/request.py";

bool FindEncodedParam(const std::vector<std::string> &params,
                      const std::string &key, const std::string &value) {
  std::string encoded;
  Util::EncodeUri(value, &encoded);
  const std::string param = key + "=" + encoded;
  for (size_t i = 0; i < params.size(); ++i) {
    if (params[i] == param) {
      return true;
    }
  }
  return false;
}

struct ParsedUrl {
  std::string base_url;
  std::vector<std::string> params;
};

std::optional<ParsedUrl> ParseUrl(absl::string_view url) {
  std::vector<std::string> url_and_params = absl::StrSplit(url, '?');
  if (url_and_params.size() != 2) {
    return std::nullopt;
  }
  return ParsedUrl{std::move(url_and_params[0]),
                   absl::StrSplit(url_and_params[1], '&')};
}

TEST(UrlTest, UninstallationSurveyUrl) {
  std::string url;
  Url::GetUninstallationSurveyUrl("0.1.2.3", &url);
  const std::optional<ParsedUrl> parsed = ParseUrl(url);
  ASSERT_TRUE(parsed.has_value()) << "Unexpected URL format: " << url;
  EXPECT_EQ(kSurveyBaseUrl, parsed->base_url);
  EXPECT_EQ(4, parsed->params.size());
  EXPECT_TRUE(FindEncodedParam(parsed->params, "contact_type", "surveyime"));
  EXPECT_TRUE(FindEncodedParam(parsed->params, "hl", "jp"));
  EXPECT_TRUE(FindEncodedParam(parsed->params, "format", "inproduct"));
  EXPECT_TRUE(FindEncodedParam(parsed->params, "version", "0.1.2.3"));
}

TEST(UrlTest, UninstallationSurveyUrlWithNoVersion) {
  std::string url;
  Url::GetUninstallationSurveyUrl("", &url);
  const std::optional<ParsedUrl> parsed = ParseUrl(url);
  ASSERT_TRUE(parsed.has_value()) << "Unexpected URL format: " << url;
  EXPECT_EQ(kSurveyBaseUrl, parsed->base_url);
  EXPECT_EQ(3, parsed->params.size());
  EXPECT_TRUE(FindEncodedParam(parsed->params, "contact_type", "surveyime"));
  EXPECT_TRUE(FindEncodedParam(parsed->params, "hl", "jp"));
  EXPECT_TRUE(FindEncodedParam(parsed->params, "format", "inproduct"));
}

}  // namespace
}  // namespace mozc
