// Copyright 2010-2020, Google Inc.
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

#include "base/flags.h"

#include <cstring>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "base/port.h"
#include "base/singleton.h"

namespace mozc_flags {

struct Flag {
  int type;
  void *storage;
  const void *default_storage;
  string help;
};

namespace {

typedef std::map<string, mozc_flags::Flag *> FlagMap;

FlagMap *GetFlagMap() { return mozc::Singleton<FlagMap>::get(); }

bool IsTrue(const char *value) {
  const char *kTrue[] = {"1", "t", "true", "y", "yes"};
  const char *kFalse[] = {"0", "f", "false", "n", "no"};
  for (size_t i = 0; i < arraysize(kTrue); ++i) {
    if (strcmp(value, kTrue[i]) == 0) {
      return true;
    } else if (strcmp(value, kFalse[i]) == 0) {
      return false;
    }
  }
  return false;
}

// Wraps std::sto* functions by a template class so that appropriate functions
// are chosen for platform-dependent integral types by type deduction.  For
// example, if int64 is defined as long long, then StrToNumberImpl<int64>::Do()
// is mapped to StrToNumberImpl::Do<long long>().  Here, struct (class) is
// intentionally used instead of a template function because, if we use a
// function, compiler may warn of "unused function".
template <typename T>
struct StrToNumberImpl;

template <>
struct StrToNumberImpl<int> {
  static int Do(const string &s) { return std::stoi(s); }
};

template <>
struct StrToNumberImpl<long> {                              // NOLINT
  static long Do(const string &s) { return std::stol(s); }  // NOLINT
};

template <>
struct StrToNumberImpl<long long> {                               // NOLINT
  static long long Do(const string &s) { return std::stoll(s); }  // NOLINT
};

template <>
struct StrToNumberImpl<unsigned long> {                               // NOLINT
  static unsigned long Do(const string &s) { return std::stoul(s); }  // NOLINT
};

template <>
struct StrToNumberImpl<unsigned long long> {       // NOLINT
  static unsigned long long Do(const string &s) {  // NOLINT
    return std::stoull(s);
  }
};

template <typename T>
inline T StrToNumber(const string &s) {
  return StrToNumberImpl<T>::Do(s);
}

#if defined(DEBUG) || defined(__APPLE__)
// Defines std::string version of absl::string_view::starts_with here to make
// flags module from independent of string_piece.cc because absl::string_view
// depends on logging.cc etc. and using it causes cyclic dependency.
inline bool StartsWith(const string &s, const string &prefix) {
  return s.size() >= prefix.size() &&
         memcmp(s.data(), prefix.data(), prefix.size()) == 0;
}
#endif  // defined(DEBUG) || defined(__APPLE__)

}  // namespace

FlagRegister::FlagRegister(const char *name, void *storage,
                           const void *default_storage, int shorttpe,
                           const char *help)
    : flag_(new Flag) {
  flag_->type = shorttpe;
  flag_->storage = storage;
  flag_->default_storage = default_storage;
  flag_->help = help;
  GetFlagMap()->insert(std::make_pair(string(name), flag_));
}

FlagRegister::~FlagRegister() { delete flag_; }

bool SetFlag(const string &name, const string &value) {
  std::map<string, Flag *>::iterator it = GetFlagMap()->find(name);
  if (it == GetFlagMap()->end()) return false;
  string v = value;
  Flag *flag = it->second;

  // If empty value is set, we assume true or empty string is set
  // for boolean or string option. With other types, setting fails.
  if (value.empty()) {
    switch (flag->type) {
      case B:
        v = "true";
        break;
      case S:
        v = "";
        break;
      default:
        return false;
    }
  }

  switch (flag->type) {
    case I:
      *reinterpret_cast<int32 *>(flag->storage) = StrToNumber<int32>(v);
      break;
    case B:
      *(reinterpret_cast<bool *>(flag->storage)) = IsTrue(v.c_str());
      break;
    case I64:
      *reinterpret_cast<int64 *>(flag->storage) = StrToNumber<int64>(v);
      break;
    case U64:
      *reinterpret_cast<uint64 *>(flag->storage) = StrToNumber<uint64>(v);
      break;
    case D:
      *reinterpret_cast<double *>(flag->storage) = strtod(v.c_str(), nullptr);
      break;
    case S:
      *reinterpret_cast<string *>(flag->storage) = v;
      break;
    default:
      break;
  }
  return true;
}

namespace {

#ifndef IGNORE_HELP_FLAG

void PrintFlags(string *output) {
  std::ostringstream os;
  for (std::map<string, Flag *>::const_iterator it = GetFlagMap()->begin();
       it != GetFlagMap()->end(); ++it) {
    os << "   --" << it->first << " (" << it->second->help << ")";
    const Flag *flag = it->second;
    switch (flag->type) {
      case I:
        os << "  type: int32  default: "
           << *(reinterpret_cast<const int *>(flag->default_storage))
           << std::endl;
        break;
      case B:
        os << "  type: bool  default: "
           << (*(reinterpret_cast<const bool *>(flag->default_storage))
                   ? "true"
                   : "false")
           << std::endl;
        break;
      case I64:
        os << "  type: int64 default: "
           << *(reinterpret_cast<const int64 *>(flag->default_storage))
           << std::endl;
        break;
      case U64:
        os << "  type: uint64  default: "
           << *(reinterpret_cast<const uint64 *>(flag->default_storage))
           << std::endl;
        break;
      case D:
        os << "  type: double  default: "
           << *(reinterpret_cast<const double *>(flag->default_storage))
           << std::endl;
        break;
      case S:
        os << "  type: string  default: "
           << *(reinterpret_cast<const string *>(flag->default_storage))
           << std::endl;
        break;
      default:
        break;
    }
  }
  *output = os.str();
}

#endif  // IGNORE_HELP_FLAG

bool CommandLineGetFlag(int argc, char **argv, string *key, string *value,
                        int *used_args) {
  key->clear();
  value->clear();
  *used_args = 0;
  if (argc < 1) {
    return false;
  }

  *used_args = 1;
  const char *start = argv[0];
  if (start[0] != '-') {
    return false;
  }

  ++start;
  if (start[0] == '-') ++start;
  const string arg = start;
  const size_t n = arg.find("=");
  if (n != string::npos) {
    *key = arg.substr(0, n);
    *value = arg.substr(n + 1, arg.size() - n);
    return true;
  }

  key->assign(arg);
  value->clear();

  if (argc == 1) {
    return true;
  }
  start = argv[1];
  if (start[0] == '-') {
    return true;
  }

  *used_args = 2;
  value->assign(start);
  return true;
}

}  // namespace

uint32 ParseCommandLineFlags(int *argc, char ***argv) {
  int used_argc = 0;
  string key, value;
  for (int i = 1; i < *argc; i += used_argc) {
    if (!CommandLineGetFlag(*argc - i, *argv + i, &key, &value, &used_argc)) {
      // TODO(komatsu): Do error handling
      continue;
    }

    if (key == "help") {
#ifndef IGNORE_HELP_FLAG
      string help;
      PrintFlags(&help);
      std::cout << help;
      exit(0);
#endif
    }
#ifdef DEBUG
    // Ignores unittest specific commandline flags.
    // In the case of Release build, IGNORE_INVALID_FLAG macro is set, so that
    // following condition makes no sense.
    if (StartsWith(key, "gtest_") || StartsWith(key, "gunit_")) {
      continue;
    }
#endif  // DEBUG
#ifdef __APPLE__
    // Mac OSX specifies process serial number like -psn_0_217141.
    // Let's ignore it.
    if (StartsWith(key, "psn_")) {
      continue;
    }
#endif
    if (!SetFlag(key, value)) {
#ifndef IGNORE_INVALID_FLAG
      std::cerr << "Unknown/Invalid flag " << key << std::endl;
      exit(1);
#endif
    }
  }
  return *argc;
}

}  // namespace mozc_flags

