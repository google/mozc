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

// This is a simple port of google flags

#ifndef MOZC_BASE_FLAGS_H_
#define MOZC_BASE_FLAGS_H_

#include "base/port.h"

#include <string>

namespace mozc_flags {

enum { I, B, I64, U64, D, S };

struct Flag;

class FlagRegister {
 public:
  FlagRegister(const char *name, void *storage, const void *default_storage,
               int shorttpe, const char *help);
  ~FlagRegister();

 private:
  Flag *flag_;
};

uint32 ParseCommandLineFlags(int *argc, char ***argv);
bool SetFlag(const string &key, const string &value);

}  // namespace mozc_flags

#define DEFINE_VARIABLE(type, shorttype, name, value, help)               \
  namespace mozc_flags_fL##shorttype {                                    \
    using mozc_flags::shorttype;                                          \
    type FLAGS_##name = value;                                            \
    static const type FLAGS_DEFAULT_##name = value;                       \
    static const mozc_flags::FlagRegister fL##name(                       \
        #name, reinterpret_cast<void *>(&FLAGS_##name),                   \
        reinterpret_cast<const void *>(&FLAGS_DEFAULT_##name), shorttype, \
        help);                                                            \
  }                                                                       \
  using mozc_flags_fL##shorttype::FLAGS_##name

#define DECLARE_VARIABLE(type, shorttype, name) \
  namespace mozc_flags_fL##shorttype {          \
    extern type FLAGS_##name;                   \
  }                                             \
  using mozc_flags_fL##shorttype::FLAGS_##name

#define DEFINE_int32(name, value, help) \
  DEFINE_VARIABLE(int32, I, name, value, help)
#define DECLARE_int32(name) DECLARE_VARIABLE(int32, I, name)

#define DEFINE_int64(name, value, help) \
  DEFINE_VARIABLE(int64, I64, name, value, help)
#define DECLARE_int64(name) DECLARE_VARIABLE(int64, I64, name)

#define DEFINE_uint64(name, value, help) \
  DEFINE_VARIABLE(uint64, U64, name, value, help)
#define DECLARE_uint64(name) DECLARE_VARIABLE(uint64, U64, name)

#define DEFINE_double(name, value, help) \
  DEFINE_VARIABLE(double, D, name, value, help)
#define DECLARE_double(name) DECLARE_VARIABLE(double, D, name)

#define DEFINE_bool(name, value, help) \
  DEFINE_VARIABLE(bool, B, name, value, help)
#define DECLARE_bool(name) DECLARE_VARIABLE(bool, B, name)

#define DEFINE_string(name, value, help) \
  DEFINE_VARIABLE(string, S, name, value, help)
#define DECLARE_string(name) DECLARE_VARIABLE(string, S, name)

#define MOZC_FLAG(type, name, value, help) \
  DEFINE_##type(name, value, help)

#define MOZC_DECLARE_FLAG(type, name) \
  DECLARE_##type(name)

namespace mozc {

inline bool GetFlag(bool flag) { return flag; }
inline int32 GetFlag(int32 flag) { return flag; }
inline int64 GetFlag(int64 flag) { return flag; }
inline uint64 GetFlag(uint64 flag) { return flag; }
inline double GetFlag(double flag) { return flag; }
inline string GetFlag(const string &flag) { return flag; }

inline void SetFlag(bool *f, bool v) { *f = v; }
inline void SetFlag(int32 *f, int32 v) { *f = v; }
inline void SetFlag(int64 *f, int64 v) { *f = v; }
inline void SetFlag(uint64 *f, uint64 v) { *f = v; }
inline void SetFlag(double *f, double v) { *f = v; }
inline void SetFlag(string *f, const string &v) { *f = v; }

}  // namespace mozc

#endif  // MOZC_BASE_FLAGS_H_
