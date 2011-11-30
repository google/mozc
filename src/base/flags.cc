// Copyright 2010-2011, Google Inc.
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

#include <stdlib.h>  // for atexit, getenv
#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include "base/base.h"
#include "base/init.h"
#include "base/singleton.h"
#include "base/util.h"

DEFINE_string(program_invocation_name, "", "Program name copied from argv[0].");

int mozc::Flags::argc = 0;
char **mozc::Flags::argv = NULL;

namespace {

typedef map<string, mozc_flags::Flag *> FlagMap;

FlagMap *GetFlagMap() {
  return mozc::Singleton<FlagMap>::get();
}

bool IsTrue(const char *value) {
  const char* kTrue[] = { "1", "t", "true", "y", "yes" };
  const char* kFalse[] = { "0", "f", "false", "n", "no" };
  for (size_t i = 0; i < arraysize(kTrue); ++i) {
    if (strcmp(value, kTrue[i]) == 0) {
      return true;
    } else if (strcmp(value, kFalse[i]) == 0) {
      return false;
    }
  }
  return false;
}
}  // namespace

namespace mozc_flags {

class FlagUtil {
 public:
  static bool SetFlag(const string &name, const string &value);
  static void PrintFlags(string *output);
};

struct Flag {
  int type;
  void *storage;
  const void *default_storage;
  string help;
};

FlagRegister::FlagRegister(const char *name,
                           void *storage,
                           const void *default_storage,
                           int shorttpe,
                           const char *help): flag_(new Flag) {
  flag_->type = shorttpe;
  flag_->storage = storage;
  flag_->default_storage = default_storage;
  flag_->help = help;
  GetFlagMap()->insert(make_pair(string(name), flag_));
}

FlagRegister::~FlagRegister() {
  delete flag_;
}

bool FlagUtil::SetFlag(const string &name, const string &value) {
  map<string, Flag *>::iterator it = GetFlagMap()->find(name);
  if (it == GetFlagMap()->end()) return false;
  string v = value;
  Flag *flag = it->second;
  if (value.empty()) {
    if (flag->type == B) {
      v = "true";
    } else {
      return false;
    }
  }
  switch (flag->type) {
    case I:
      *(reinterpret_cast<int32 *>(flag->storage)) = atoi32(v.c_str());
      break;
    case B:
      *(reinterpret_cast<bool *>(flag->storage)) = IsTrue(v.c_str());
      break;
    case I64:
      *(reinterpret_cast<int64 *>(flag->storage)) =
          strtoll(v.c_str(), NULL, 10);
      break;
    case U64:
      *(reinterpret_cast<uint64 *>(flag->storage)) =
          strtoull(v.c_str(), NULL, 10);
      break;
    case D:
      *(reinterpret_cast<double *>(flag->storage)) = strtod(v.c_str(), NULL);
      break;
    case S:
      *(reinterpret_cast<string *>(flag->storage)) = v;
      break;
    default:
      break;
  }
  return true;
}

void FlagUtil::PrintFlags(string *output) {
  ostringstream os;
  for (map<string, Flag *>::const_iterator it = GetFlagMap()->begin();
       it != GetFlagMap()->end(); ++it) {
    os << "   --" << it->first << " (" << it->second->help << ")";
    const Flag *flag = it->second;
    switch (flag->type) {
      case I:
        os << "  type: int32  default: " <<
            *(reinterpret_cast<const int *>(flag->default_storage)) << endl;
                                break;
      case B:
        os << "  type: bool  default: " <<
            (*(reinterpret_cast<const bool *>(flag->default_storage))
             ? "true" : "false") << endl;
                               break;
      case I64:
        os << "  type: int64 default: " <<
            *(reinterpret_cast<const int64 *>(flag->default_storage)) << endl;
                               break;
      case U64:
        os << "  type: uint64  default: " <<
            *(reinterpret_cast<const uint64 *>(flag->default_storage)) << endl;
                                 break;
      case D:
        os << "  type: double  default: " <<
            *(reinterpret_cast<const double *>(flag->default_storage)) << endl;
                                 break;
      case S:
        os << "  type: string  default: " <<
            *(reinterpret_cast<const string *>(flag->default_storage)) << endl;
                                 break;
      default:
        break;
    }
  }
  *output = os.str();
}

uint32 ParseCommandLineFlags(int *argc, char*** argv,
                             bool remove_flags) {
  int used_argc = 0;
  string key, value;
  for (int i = 1; i < *argc; i += used_argc) {
    if (!mozc::Util::CommandLineGetFlag(*argc - i, *argv + i,
                                  &key, &value, &used_argc)) {
      // TODO(komatsu): Do error handling
      continue;
    }

    if (key == "fromenv") {
      vector<string> keys;
      mozc::Util::SplitStringUsing(value, ",", &keys);
      for (size_t j = 0; j < keys.size(); ++j) {
        if (keys[j].empty() || keys[j] == "fromenv") {
          continue;
        }
        string env_key = "FLAGS_";
        env_key += keys[j];
        const char *env_value = getenv(env_key.c_str());
        if (env_value == NULL) {
          continue;
        }
        if (!FlagUtil::SetFlag(keys[j], env_value)) {
#ifndef IGNORE_INVALID_FLAG
          cerr << "Unknown/Invalid flag " << key << endl;
#endif
        }
      }
      continue;
    }

    if (key == "help") {
#ifndef IGNORE_HELP_FLAG
      string help;
      FlagUtil::PrintFlags(&help);
      cout << help;
      exit(0);
#endif
    }
#ifdef OS_MACOSX
    // Mac OSX specifies process serial number like -psn_0_217141.
    // Let's ignore it.
    if (key.find("psn_") == 0) {
      continue;
    }
#endif
    if (!FlagUtil::SetFlag(key, value)) {
#ifndef IGNORE_INVALID_FLAG
      cerr << "Unknown/Invalid flag " << key << endl;
      exit(0);
#endif
    }
  }
  return *argc;
}
}  // mozc_flags

namespace mozc {
void InitGoogleInternal(const char *argv0,
                        int *argc, char ***argv,
                        bool remove_flags) {
  mozc_flags::FlagUtil::SetFlag("program_invocation_name", *argv[0]);
  mozc_flags::ParseCommandLineFlags(argc, argv, remove_flags);
  if (*argc > 0) {
    mozc::Logging::InitLogStream((*argv)[0]);
  } else {
    mozc::Logging::InitLogStream();
  }

  mozc::RunInitializers();  // do all static initialization

  // copy argc/argv
  mozc::Flags::argc = *argc;
  mozc::Flags::argv = *argv;
}
}  // namespace mozc
