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

#include "absl/algorithm/container.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "testing/gunit.h"

namespace mozc {
namespace url {
namespace {

constexpr char kSurveyBaseUrl[] =
    "http://www.google.com/support/ime/japanese/bin/request.py";

bool FindEncodedParam(absl::Span<const std::string> params,
                      const absl::string_view key,
                      const absl::string_view value) {
  const std::string param = absl::StrCat(key, "=", mozc::url::EncodeUrl(value));
  return absl::c_find(params, param) != params.end();
}

struct ParsedUrl {
  std::string base_url;
  std::vector<std::string> params;
};

std::optional<ParsedUrl> ParseUrl(const absl::string_view url) {
  std::vector<std::string> url_and_params = absl::StrSplit(url, '?');
  if (url_and_params.size() != 2) {
    return std::nullopt;
  }
  return ParsedUrl{std::move(url_and_params[0]),
                   absl::StrSplit(url_and_params[1], '&')};
}

TEST(UrlTest, UninstallationSurveyUrl) {
  const std::string url = GetUninstallationSurveyUrl("0.1.2.3");
  const std::optional<ParsedUrl> parsed = ParseUrl(url);
  ASSERT_TRUE(parsed.has_value()) << "Unexpected URL format: " << url;
  EXPECT_EQ(parsed->base_url, kSurveyBaseUrl);
  EXPECT_EQ(parsed->params.size(), 4);
  EXPECT_TRUE(FindEncodedParam(parsed->params, "contact_type", "surveyime"));
  EXPECT_TRUE(FindEncodedParam(parsed->params, "hl", "jp"));
  EXPECT_TRUE(FindEncodedParam(parsed->params, "format", "inproduct"));
  EXPECT_TRUE(FindEncodedParam(parsed->params, "version", "0.1.2.3"));
}

TEST(UrlTest, UninstallationSurveyUrlWithNoVersion) {
  const std::string url = GetUninstallationSurveyUrl("");
  const std::optional<ParsedUrl> parsed = ParseUrl(url);
  ASSERT_TRUE(parsed.has_value()) << "Unexpected URL format: " << url;
  EXPECT_EQ(parsed->base_url, kSurveyBaseUrl);
  EXPECT_EQ(parsed->params.size(), 3);
  EXPECT_TRUE(FindEncodedParam(parsed->params, "contact_type", "surveyime"));
  EXPECT_TRUE(FindEncodedParam(parsed->params, "hl", "jp"));
  EXPECT_TRUE(FindEncodedParam(parsed->params, "format", "inproduct"));
}

TEST(UrlTest, EncodeUri) {
  EXPECT_EQ(EncodeUrl("もずく"), "%E3%82%82%E3%81%9A%E3%81%8F");
  EXPECT_EQ(EncodeUrl("mozc"), "mozc");
  EXPECT_EQ(EncodeUrl("http://mozc/?q=Hello World"),
            "http%3A%2F%2Fmozc%2F%3Fq%3DHello%20World");
}

TEST(UrlTest, DecodeUri) {
  EXPECT_EQ(DecodeUrl("%E3%82%82%E3%81%9A%E3%81%8F"), "もずく");
  EXPECT_EQ(DecodeUrl("mozc"), "mozc");
  EXPECT_EQ(DecodeUrl("http%3A%2F%2Fmozc%2F%3Fq%3DHello+World"),
            "http://mozc/?q=Hello World");
}

}  // namespace
}  // namespace url
}  // namespace mozc
