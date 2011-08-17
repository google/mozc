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

#ifndef MOZC_BASE_UPDATE_CHECKER_H_
#define MOZC_BASE_UPDATE_CHECKER_H_

#if defined(OS_WINDOWS)
#include <Windows.h>
#endif  // OS_WINDOWS

#include "base/port.h"

namespace mozc {

class UpdateChecker {
 public:
#if defined(OS_WINDOWS)
  struct CallbackInfo {
    // Window handle to which the callback message is sent.
    HWND mesage_receiver_window;
    // Message ID of the update check callback.
    UINT mesage_id;
    CallbackInfo()
      : mesage_receiver_window(NULL),
        mesage_id(0)
    {}
  };

  // WParam of the callback message.  LParam is not used yet.
  enum CallbackWParam {
    UPGRADE_IS_AVAILABLE,
    UPGRADE_ALREADY_UP_TO_DATE,
    UPGRADE_ERROR,
  };
#else
  struct CallbackInfo {
    void *dummy;  // a dummy element to avoid to be an empty struct.
    CallbackInfo()
      : dummy(NULL)
    {}
  };
#endif  // OS_WINDOWS

  // On Windows, this method immediately returns true when background update
  // check begins.  The actual result will be delivered as Win32 message as
  // specified in CallbackInfo.
  // On other platforms, returns false and does nothing.
  static bool BeginCheck(const CallbackInfo &info);

 private:
  DISALLOW_COPY_AND_ASSIGN(UpdateChecker);
};

class UpdateInvoker {
 public:
#if defined(OS_WINDOWS)
  struct CallbackInfo {
    // Window handle to which the callback message is sent.
    HWND mesage_receiver_window;
    // Message ID of the update check callback.
    UINT mesage_id;
    CallbackInfo()
      : mesage_receiver_window(NULL),
        mesage_id(0)
    {}
  };

  // WParam of the callback message.
  // Here are some samples which describe how the |mesage_receiver_window|
  // receives callback messages.
  //
  // Case A. New version is installed successfully.
  //  1. ON_SHOW
  //  2. ON_CHECKING_FOR_UPDATE
  //  3. ON_UPDATE_AVAILABLE
  //  4. ON_DOWNLOADING, |lParam| == 0, 1, 2, .... 99
  //  5. ON_WAITING_TO_INSTALL
  //  6. ON_INSTALLING
  //  7. ON_COMPLETE, lParam == JOB_SUCCEEDED
  //
  // Case B. The latest version has already been installed.
  //  1. ON_SHOW
  //  2. ON_CHECKING_FOR_UPDATE
  //  3. ON_COMPLETE, lParam == JOB_SUCCEEDED
  //
  // Case C. Network connection is not available.
  //  1. ON_SHOW
  //  2. ON_CHECKING_FOR_UPDATE
  //  3. ON_COMPLETE, lParam == JOB_FAILED
  enum CallbackWParam {
    ON_SHOW = 0,
    ON_CHECKING_FOR_UPDATE,
    ON_UPDATE_AVAILABLE,
    ON_WAITING_TO_DOWNLOAD,
    ON_DOWNLOADING,  // lParam is the parcentage of downloading.
    ON_WAITING_TO_INSTALL,
    ON_INSTALLING,
    ON_PAUSE,
    ON_COMPLETE,     // lParam is a value of CallbackOnCompleteLParam.
  };

  // LParam when WParam == ON_COMPLETE.
  enum CallbackOnCompleteLParam {
    JOB_FAILED = 0,
    JOB_SUCCEEDED,
  };
#else
  struct CallbackInfo {
    void *dummy;  // a dummy element to avoid to be an empty struct.
    CallbackInfo()
      : dummy(NULL)
    {}
  };
#endif  // OS_WINDOWS

  // On Windows, this method immediately returns true when background update
  // begins.  The actual result will be delivered as Win32 message as
  // specified in CallbackInfo.  See the comment of CallbackWParam for details.
  // On other platforms, returns false and does nothing.
  static bool BeginUpdate(const CallbackInfo &info);

 private:
  DISALLOW_COPY_AND_ASSIGN(UpdateInvoker);
};
}  // namespace mozc
#endif  // MOZC_BASE_UPDATE_CHECKER_H_
