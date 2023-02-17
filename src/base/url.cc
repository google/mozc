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

#include <string>
#include <vector>

#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace url {

std::string DecodeUrl(const absl::string_view input) {
  std::string result;
  result.reserve(input.size());
  for (auto p = input.begin(); p < input.end(); ++p) {
    if (*p == '%' && p + 2 < input.end()) {
      const char h = absl::ascii_toupper(p[1]);
      const char l = absl::ascii_toupper(p[2]);
      const int vh = absl::ascii_isalpha(h) ? (10 + (h - 'A')) : (h - '0');
      const int vl = absl::ascii_isalpha(l) ? (10 + (l - 'A')) : (l - '0');
      result.push_back((vh << 4) + vl);
      p += 2;
    } else if (*p == '+') {
      result.push_back(' ');
    } else {
      result.push_back(*p);
    }
  }
  return result;
}

std::string EncodeUrl(const absl::string_view input) {
  std::string result;
  result.reserve(input.size());
  for (const char c : input) {
    if (absl::ascii_isascii(c) && absl::ascii_isalnum(c)) {
      result.append(1, c);
    } else {
      absl::StrAppendFormat(&result, "%%%02X", c);
    }
  }
  return result;
}

namespace {
constexpr char kSurveyBaseUrl[] =
    "http://www.google.com/support/ime/japanese/bin/request.py";
constexpr char kSurveyVersionEntry[] = "version";
constexpr char kSurveyContactTypeEntry[] = "contact_type";
constexpr char kSurveyContactType[] = "surveyime";
constexpr char kSurveyHtmlLanguageEntry[] = "hl";
constexpr char kSurveyHtmlLanguage[] = "jp";
constexpr char kSurveyFormatEntry[] = "format";
constexpr char kSurveyFormat[] = "inproduct";

std::string ParamPairToString(const absl::string_view key,
                              const absl::string_view value) {
  return absl::StrCat(key, "=", EncodeUrl(value));
}

}  // namespace

std::string GetUninstallationSurveyUrl(const absl::string_view version) {
  std::vector<std::string> params = {
      ParamPairToString(kSurveyContactTypeEntry, kSurveyContactType),
      ParamPairToString(kSurveyHtmlLanguageEntry, kSurveyHtmlLanguage),
      ParamPairToString(kSurveyFormatEntry, kSurveyFormat),
  };
  if (!version.empty()) {
    params.push_back(ParamPairToString(kSurveyVersionEntry, version));
  }

  return absl::StrCat(kSurveyBaseUrl, "?", absl::StrJoin(params, "&"));
}

}  // namespace url
}  // namespace mozc
