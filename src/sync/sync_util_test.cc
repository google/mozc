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

#include <string>
#include "base/base.h"
#include "base/file_stream.h"
#include "base/util.h"
#include "sync/sync_util.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace sync {

TEST(SyncUtil, CopyLastSyncedFile) {
  // just test rename operation works as intended
  const string from = Util::JoinPath(FLAGS_test_tmpdir,
                                     "copy_from");
  const string to = Util::JoinPath(FLAGS_test_tmpdir,
                                   "copy_to");
  Util::Unlink(from);
  Util::Unlink(to);

  EXPECT_FALSE(SyncUtil::CopyLastSyncedFile(from, to));

  const char kData[] = "This is a test";

  {
    OutputFileStream ofs(from.c_str(), ios::binary);
    ofs.write(kData, arraysize(kData));
  }

  EXPECT_TRUE(SyncUtil::CopyLastSyncedFile(from, to));
  EXPECT_TRUE(Util::IsEqualFile(from, to));

#ifdef OS_WINDOWS
  // check filename has 'system hidden' attributes
  wstring wfilename;
  Util::UTF8ToWide(to.c_str(), &wfilename);
  const DWORD attributes = ::GetFileAttributes(wfilename.c_str());
  EXPECT_TRUE(attributes & FILE_ATTRIBUTE_HIDDEN);
  EXPECT_TRUE(attributes & FILE_ATTRIBUTE_SYSTEM);
#endif

  Util::Unlink(from);
  Util::Unlink(to);
}
}  // sync
}  // mozc
