// Copyright 2010, Google Inc.
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

#include "gui/base/singleton_window_helper.h"

#ifdef OS_WINDOWS
#include <windows.h>
#endif

#include "base/base.h"
#include "base/file_stream.h"
#include "base/mutex.h"
#include "base/process_mutex.h"
#include "base/scoped_handle.h"
#include "base/util.h"
#include "gui/base/win_util.h"
#include "ipc/window_info.pb.h"

namespace mozc {
namespace gui {
namespace {
bool ReadWindowInfo(const string &lock_name,
                    ipc::WindowInfo *window_info) {
#ifdef OS_WINDOWS
  wstring wfilename;
  mozc::Util::UTF8ToWide(lock_name.c_str(), &wfilename);
  {
    mozc::ScopedHandle handle(
      ::CreateFileW(wfilename.c_str(),
                    GENERIC_READ,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL, OPEN_EXISTING, 0, NULL));
    if (NULL == handle.get()) {
      LOG(ERROR) << "cannot open: " << lock_name << " " << ::GetLastError();
      return false;
    }

    const DWORD size = ::GetFileSize(handle.get(), NULL);
    if (-1 == static_cast<int>(size)) {
      LOG(ERROR) << "GetFileSize failed:" << ::GetLastError();
      return false;
    }

    const DWORD kMaxFileSize = 2096;
    if (size == 0 || size >= kMaxFileSize) {
      LOG(ERROR) << "Invalid file size: " << kMaxFileSize;
      return false;
    }

    scoped_array<char> buf(new char[size]);

    DWORD read_size = 0;
    if (!::ReadFile(handle.get(), buf.get(),
                    size, &read_size, NULL)) {
      LOG(ERROR) << "ReadFile failed: " << ::GetLastError();
      return false;
    }

    if (read_size != size) {
      LOG(ERROR) << "read_size != size";
      return false;
    }

    if (!window_info->ParseFromArray(buf.get(), static_cast<int>(size))) {
      LOG(ERROR) << "ParseFromArra failed";
      return false;
    }
  }
#else
  InputFileStream is(lock_name.c_str(), ios::binary|ios::in);
  if (!is) {
    LOG(ERROR) << "cannot open: " << lock_name;
    return false;
  }

  if (!window_info->ParseFromIstream(&is)) {
    LOG(ERROR) << "ParseFromStream failed";
    return false;
  }
#endif
  return true;
}
}  // namespace

SingletonWindowHelper::SingletonWindowHelper(const string &name) {
  mutex_.reset(new mozc::ProcessMutex(name.c_str()));
}

SingletonWindowHelper::~SingletonWindowHelper() {}

bool SingletonWindowHelper::FindPreviousWindow() {
  ipc::WindowInfo window_info;
#ifdef OS_WINDOWS
  window_info.set_process_id(static_cast<uint32>(::GetCurrentProcessId()));
#else
  window_info.set_process_id(static_cast<uint32>(getpid()));
#endif

  string window_info_str;
  if (!window_info.SerializeToString(&window_info_str)) {
    LOG(ERROR) << "SerializeToString failed";
    return false;
  }

  if (!mutex_->LockAndWrite(window_info_str)) {
    LOG(ERROR) << "config_dialog is already running";
    return true;
  }
  return false;
}

// Windows
//  Activate window using process_id
// Others
//  Not implemented.
bool SingletonWindowHelper::ActivatePreviousWindow() {
  ipc::WindowInfo window_info;
  if (!ReadWindowInfo(mutex_->lock_filename(), &window_info)) {
    LOG(ERROR) << "ReadWindowInfo failed";
    return false;
  }
#ifdef OS_WINDOWS
  WinUtil::ActivateWindow(window_info.process_id());
  return true;
#else
  // not implemented
  return false;
#endif
}

}  // namespace gui
}  // namespace mozc
