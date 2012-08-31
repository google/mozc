// Copyright 2010-2012, Google Inc.
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

#include "base/logging.h"
#include "base/singleton.h"
#include "base/util.h"
#include "base/version.h"

namespace mozc {
namespace {
const char kSurveyBaseURL[] =
    "http://www.google.com/support/ime/japanese/bin/request.py";
const char kSurveyVersionEntry[] = "version";
const char kSurveyContactTypeEntry[] = "contact_type";
const char kSurveyContactType[] = "surveyime";
const char kSurveyHtmlLanguageEntry[] = "hl";
const char kSurveyHtmlLanguage[] = "jp";
const char kSurveyFormatEntry[] = "format";
const char kSurveyFormat[] = "inproduct";


class URLImpl {
 public:
  URLImpl() {
    InitUninstallationSurveyURL();
  }

  bool GetUninstallationSurveyURL(const string &version, string *url) const {
    DCHECK(url);
    *url = uninstallation_survey_url_;
    if (!version.empty()) {
      *url += "&";
      vector<pair<string, string> > params;
      params.push_back(make_pair(kSurveyVersionEntry, version));
      Util::AppendCGIParams(params, url);
    }
    return true;
  }

 private:
  void InitUninstallationSurveyURL() {
    uninstallation_survey_url_.clear();
    uninstallation_survey_url_ = kSurveyBaseURL;
    uninstallation_survey_url_ += "?";
    vector<pair<string, string> > params;
    params.push_back(make_pair(kSurveyContactTypeEntry, kSurveyContactType));
    params.push_back(make_pair(kSurveyHtmlLanguageEntry, kSurveyHtmlLanguage));
    params.push_back(make_pair(kSurveyFormatEntry, kSurveyFormat));
    Util::AppendCGIParams(params, &uninstallation_survey_url_);
  }

  string uninstallation_survey_url_;
};
}  // namespace

bool URL::GetUninstallationSurveyURL(const string &version, string *url) {
  return Singleton<URLImpl>::get()->GetUninstallationSurveyURL(version, url);
}

}  // namespace mozc
