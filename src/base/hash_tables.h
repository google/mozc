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

// Provides hash_map, hash_set, and the basic hash functions.
// Eventually we'll be switching to unorderd_map and unorderd_set.

#ifndef MOZC_BASE_HASH_TABLES_H_
#define MOZC_BASE_HASH_TABLES_H_


#ifdef OS_WINDOWS
#include <hash_map>
#include <hash_set>
using stdext::hash_map;
using stdext::hash_set;
#else  // not OS_WINDOWS


#include <ext/hash_map>
#include <ext/hash_set>
using __gnu_cxx::hash_map;
using __gnu_cxx::hash_set;

#include <string>
namespace __gnu_cxx {
// FNV-1a hash similar to tr1/functional_hash.h.
template <>
struct hash<std::string> {
  std::size_t operator()(const std::string& s) const {
    std::size_t result = 0;
    for (std::string::const_iterator i = s.begin(); i != s.end(); ++i) {
      result = (result * 131) + *i;
    }
    return result;
  }
};
}
#endif  // not OS_WINDOWS

#endif  // MOZC_BASE_HASH_TABLES_H_
