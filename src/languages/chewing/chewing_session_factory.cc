// Copyright 2010-2013, Google Inc.
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

#include "languages/chewing/chewing_session_factory.h"

#include <pwd.h>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/singleton.h"
#include "engine/empty_user_data_manager.h"
#include "languages/chewing/session.h"

#if defined(OS_CHROMEOS)
DEFINE_string(datapath, "/usr/share/chewing",
              "the default path of libchewing");
#else
DEFINE_string(datapath, "/usr/share/libchewing3/chewing",
              "the default path of libchewing");
#endif  // OS_CHROMEOS

namespace mozc {

string GetHashPath() {
  // The logic below is copied from SystemUtil::GetUserProfileDirectory().
  string dir;
  char buf[1024];
  struct passwd pw, *ppw;
  const uid_t uid = geteuid();
  CHECK_EQ(0, getpwuid_r(uid, &pw, buf, sizeof(buf), &ppw))
      << "Can't get passwd entry for uid " << uid << ".";
  CHECK_LT(0, strlen(pw.pw_dir))
      << "Home directory for uid " << uid << " is not set.";
#if defined(OS_CHROMEOS)
  dir = FileUtil::JoinPath(pw.pw_dir, "user/.chewing");
#else
  dir = FileUtil::JoinPath(pw.pw_dir, ".chewing");
#endif  // OS_CHROMEOS
  return dir;
}

namespace chewing {

// The default session factory implementation for chewing.  We do not
// use the implementation in session/session_factory.cc.  We do not
// even link to it because the default session factory refers to the
// Japanese language models / Japanese vocabulary which we don't want
// here.
ChewingSessionFactory::ChewingSessionFactory() {
  string hash_path = GetHashPath();
  if (!FileUtil::DirectoryExists(hash_path)) {
    string hash_dir = FileUtil::Dirname(hash_path);
    // In Chrome OS, hash_dir would be ~/user, which might not exist.
    if (FileUtil::DirectoryExists(hash_dir) ||
        FileUtil::CreateDirectory(hash_dir)) {
      FileUtil::CreateDirectory(hash_path);
    }
  }
  ::chewing_Init(FLAGS_datapath.c_str(), hash_path.c_str());
}

ChewingSessionFactory::~ChewingSessionFactory() {
  ::chewing_Terminate();
}

session::SessionInterface *ChewingSessionFactory::NewSession() {
  return new Session();
}

UserDataManagerInterface *ChewingSessionFactory::GetUserDataManager() {
  return Singleton<EmptyUserDataManager>::get();
}

}  // namespace chewing
}  // namespace mozc
