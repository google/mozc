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

#include "sync/sync_util.h"

#include <stdlib.h>

#ifdef OS_WINDOWS
#include <windows.h>
#endif

#include "base/base.h"
#include "base/util.h"

namespace mozc {
namespace sync {

string SyncUtil::GenRandomString(int size) {
  string result;
  for (int i = 0; i < size; ++i) {
    const char32 l = Util::Random(static_cast<int>('z' - 'a')) + 'a';
    Util::UCS4ToUTF8Append(l, &result);
  }
  return result;
}

bool SyncUtil::CopyLastSyncedFile(const string &current,
                                  const string &prev) {
  if (!Util::CopyFile(current, prev)) {
    LOG(ERROR) << "cannot copy " << current << " prev " << prev;
    return false;
  }

#ifdef OS_WINDOWS
  // make the file |prev| 'system hidden'.
  wstring wfilename;
  Util::UTF8ToWide(prev.c_str(), &wfilename);
  if (!::SetFileAttributes(wfilename.c_str(),
                           FILE_ATTRIBUTE_HIDDEN |
                           FILE_ATTRIBUTE_SYSTEM)) {
    LOG(ERROR) << "Cannot make hidden: " << prev
               << " " << ::GetLastError();
  }
#endif

  return true;
}
}  // sync
}  // mozc
