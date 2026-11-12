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

#include <cstddef>
#include <fstream>
#include <optional>
#include <string>

#include "absl/base/const_init.h"
#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/ascii.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"

namespace {
constexpr char kBuildPropPath[] = "/system/build.prop";
constinit absl::Mutex sys_prop_mutex(absl::kConstInit);
}  // namespace

namespace mozc {

// static
std::string AndroidUtil::GetSystemProperty(absl::string_view key,
                                           absl::string_view default_value) {
  static absl::NoDestructor<
      absl::flat_hash_map<std::string, std::optional<std::string>>>
      property_cache;
  absl::MutexLock lock(sys_prop_mutex);
  auto [it, inserted] = property_cache->try_emplace(key);
  if (inserted) {
    it->second = GetPropertyFromFile(key);
  }
  return it->second.has_value() ? *it->second : std::string(default_value);
}

// static
std::optional<std::string> AndroidUtil::GetPropertyFromFile(
    absl::string_view key) {
  std::ifstream ifs(kBuildPropPath);
  if (!ifs) {
    return std::nullopt;
  }
  std::string line;
  std::string lhs;
  std::string rhs;
  while (std::getline(ifs, line)) {
    if (!AndroidUtil::ParseLine(line, lhs, rhs)) {
      continue;
    }
    if (key == lhs) {
      return rhs;
    }
  }
  return std::nullopt;
}

// Valid line's format : /[ \t]*([^#=][^=]*)=([^\r\n]*)[\r\n]*/
// $1 == Key
// $2 == Value
// android_util_test.cc has some samples.
//
// static
bool AndroidUtil::ParseLine(absl::string_view line, std::string& lhs,
                            std::string& rhs) {
  while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
    line.remove_suffix(1);
  }
  line = absl::StripLeadingAsciiWhitespace(line);
  if (line.empty() || line.front() == '#') {
    return false;
  }
  const size_t delimiter_pos = line.find('=');
  if (delimiter_pos == absl::string_view::npos || delimiter_pos == 0) {
    return false;
  }
  lhs = line.substr(0, delimiter_pos);
  rhs = line.substr(delimiter_pos + 1);
  return true;
}
}  // namespace mozc
