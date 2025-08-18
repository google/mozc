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

#include "base/android_util.h"

#include <fstream>
#include <utility>

#include "absl/base/const_init.h"
#include "absl/log/check.h"
#include "absl/synchronization/mutex.h"
#include "base/util.h"

namespace {
constexpr char kBuildPropPath[] = "/system/build.prop";
constinit absl::Mutex sys_prop_mutex(absl::kConstInit);
}  // namespace

namespace mozc {
std::map<std::string, std::string> AndroidUtil::property_cache;
std::set<std::string> AndroidUtil::undefined_keys;
constexpr char AndroidUtil::kSystemPropertyOsVersion[] =
    "ro.build.version.release";
constexpr char AndroidUtil::kSystemPropertyModel[] = "ro.product.model";
constexpr char AndroidUtil::kSystemPropertySdkVersion[] =
    "ro.build.version.sdk";

// static
std::string AndroidUtil::GetSystemProperty(const std::string& key,
                                           const std::string& default_value) {
  absl::MutexLock lock(&sys_prop_mutex);
  std::map<std::string, std::string>::iterator it = property_cache.find(key);
  if (it != property_cache.end()) {
    // Cache is found.
    return it->second;
  }
  if (undefined_keys.find(key) != undefined_keys.end()) {
    // Found the key invalid in the |undefined_keys| cache.
    return default_value;
  }
  // Cache is not found.
  // We have not been passed |key| yet. It is the first time.
  std::string value;
  if (GetPropertyFromFile(key, &value)) {
    // Successfully read from the property file.
    // Update the cache and return the result.
    property_cache[key] = value;
    return value;
  } else {
    // Reading from the file is failed.
    // Add |key| into |undefined_keys|.
    // If the same |key| is passed to this method in future,
    // GetPropertyFromFile() will not be invoked
    // and |default_value| will be directly returned.
    undefined_keys.insert(key);
    return default_value;
  }
}

// static
bool AndroidUtil::GetPropertyFromFile(const std::string& key,
                                      std::string* output) {
  std::ifstream ifs(kBuildPropPath);
  if (!ifs) {
    return false;
  }
  std::string line;
  bool found = false;
  std::string lhs;
  std::string rhs;
  while (getline(ifs, line)) {
    if (!mozc::AndroidUtil::ParseLine(line, &lhs, &rhs)) {
      continue;
    }
    if (key == lhs) {
      found = true;
      *output = std::move(rhs);
      break;
    }
  }
  ifs.close();
  return found;
}

// Valid line's format : /[ \t]*([^#=][^=]*)=([^\r\n]*)[\r\n]*/
// $1 == Key
// $2 == Value
// android_util_test.cc has some samples.
//
// static
bool AndroidUtil::ParseLine(const std::string& line, std::string* lhs,
                            std::string* rhs) {
  DCHECK(lhs);
  DCHECK(rhs);
  std::string tmp_line = line;
  mozc::Util::ChopReturns(&tmp_line);
  // Trim white spaces at the head.
  size_t line_start = tmp_line.find_first_not_of(" \t");
  if (line_start != std::string::npos) {
    tmp_line = tmp_line.substr(line_start);
  }
  if (tmp_line.empty() || tmp_line.at(0) == '#') {
    return false;
  }
  size_t delimiter_pos = tmp_line.find('=');
  if (delimiter_pos == std::string::npos) {
    return false;
  }
  *lhs = tmp_line.substr(0, delimiter_pos);
  *rhs = tmp_line.substr(delimiter_pos + 1);
  return !lhs->empty();
}
}  // namespace mozc
