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

#include <Windows.h>
#include <Imagehlp.h>
#include <SoftPub.h>
#include <Wintrust.h>

#include <sstream>
#include <string>

#include "base/const.h"
#include "base/run_level.h"
#include "base/mmap.h"
#include "base/scoped_ptr.h"
#include "base/util.h"
#include "base/win_util.h"
#include "client/client_interface.h"
#include "renderer/renderer_client.h"
#include "win32/base/imm_util.h"
#include "win32/base/file_verifier.h"
#include "win32/broker/mozc_broker_resource.h"

namespace mozc {
namespace win32 {
namespace {

const int kErrorLevelSuccess = 0;
const int kErrorLevelGeneralError = 1;
const int kErrorLevelCorruptedFileDetected = 2;

wstring LoadStringResource(int resource_id) {
  wchar_t buffer[4096] = {};
  const int copied_chars_without_null =
      ::LoadString(::GetModuleHandleW(NULL), resource_id,
      buffer, arraysize(buffer));

  if (copied_chars_without_null == arraysize(buffer)) {
    DLOG(ERROR) << "Message is truncated.";
    return L"";
  }

  return wstring(buffer, copied_chars_without_null);
}

void EnterBackgroundThreadMode() {
  // GetCurrentThread returns pseudo handle, which you need not
  // to pass CloseHandle.
  const HANDLE thread_handle = ::GetCurrentThread();

  // Enter low priority mode.
  if (mozc::Util::IsVistaOrLater()) {
    ::SetThreadPriority(thread_handle, THREAD_MODE_BACKGROUND_BEGIN);
  } else {
    ::SetThreadPriority(thread_handle, THREAD_PRIORITY_IDLE);
  }
}

}  // namespace

int RunPrelaunchProcesses(int argc, char *argv[]) {
  if (RunLevel::IsProcessInJob()) {
    return kErrorLevelGeneralError;
  }

  bool is_service_process = false;
  if (!WinUtil::IsServiceProcess(&is_service_process)) {
    // Returns DENY conservatively.
    return kErrorLevelGeneralError;
  }
  if (is_service_process) {
    return kErrorLevelGeneralError;
  }

  if (!ImeUtil::IsDefault()) {
    // If Mozc is not default, do nothing.
    return kErrorLevelSuccess;
  }

  // To keep performance impact minimum, set current thread as background.
  // On Vista and later, all the I/O including page faults well be handled
  // in low priority if the thread is marked as background.
  EnterBackgroundThreadMode();

  const FileVerifier::MozcSystemFile kSourceFiles[] = {
    FileVerifier::kMozcServerFile,
    FileVerifier::kMozcRendererFile,
    FileVerifier::kMozcToolFile,
  };

  wstringstream missing_files;
  wstringstream corrupted_files;
  for (size_t i = 0; i < arraysize(kSourceFiles); ++i) {
    string fileinfo;
    const FileVerifier::IntegrityType integrity_type =
        FileVerifier::VerifyIntegrity(kSourceFiles[i], &fileinfo);
    switch (integrity_type) {
      case FileVerifier::kIntegrityCorrupted: {
        wstring wfileinfo;
        Util::UTF8ToWide(fileinfo, &wfileinfo);
        corrupted_files << L"\r\n  " << wfileinfo;
        break;
      }
      case FileVerifier::kIntegrityFileNotFound: {
        wstring wfileinfo;
        Util::UTF8ToWide(fileinfo, &wfileinfo);
        missing_files << L"\r\n  " << wfileinfo;
        break;
      }
    }
  }

  const wstring &corrupted_files_str = corrupted_files.str();
  const wstring &missing_files_str = missing_files.str();
  if (!corrupted_files_str.empty() || !missing_files_str.empty()) {
    wstringstream message;
    message << LoadStringResource(IDS_DIALOG_MESSAGE);
    if (!corrupted_files_str.empty()) {
      message << L"\r\n" << LoadStringResource(IDS_DIALOG_FILE_CORRUPTED)
              << L":" << corrupted_files_str;
    }
    if (!missing_files_str.empty()) {
      message << L"\r\n" << LoadStringResource(IDS_DIALOG_FILE_NOT_FOUND)
              << L":" << missing_files_str;
    }
    ::MessageBoxW(NULL, message.str().c_str(),
                  LoadStringResource(IDS_DIALOG_TITLE).c_str(),
                  MB_OK | MB_ICONERROR | MB_DEFAULT_DESKTOP_ONLY);
    return kErrorLevelCorruptedFileDetected;
  }

  {
    scoped_ptr<client::ClientInterface> converter_client(
        client::ClientFactory::NewClient());
    converter_client->set_suppress_error_dialog(true);
    converter_client->EnsureConnection();
  }

  {
    scoped_ptr<renderer::RendererClient> renderer_client(
        new mozc::renderer::RendererClient);
    renderer_client->set_suppress_error_dialog(true);
    renderer_client->Activate();
  }

  return kErrorLevelSuccess;
}

}  // namespace win32
}  // namespace mozc
