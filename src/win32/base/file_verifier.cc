// Copyright 2010-2014, Google Inc.
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

#include "win32/base/file_verifier.h"

#include <Windows.h>
#include <Imagehlp.h>
#include <SoftPub.h>
#include <Wintrust.h>

#include <string>

#include "base/const.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/mmap.h"
#include "base/system_util.h"
#include "base/util.h"

namespace mozc {
namespace win32 {
namespace {

FileVerifier::IntegrityType VerifyPEHeaderChecksum(const string &filename) {
  Mmap mapped_file;
  if (!mapped_file.Open(filename.c_str())) {
    return FileVerifier::kIntegrityFileOpenFailed;
  }
  // CheckSumMappedFile does not support multi-thread.
  // TODO(yukawa): Add a critical section.
  DWORD compile_time_check_sum = 0;
  DWORD actual_check_sum = 0;
  const IMAGE_NT_HEADERS *nt_header = ::CheckSumMappedFile(
      mapped_file.begin(), mapped_file.size(),
      &compile_time_check_sum, &actual_check_sum);
  if (nt_header == nullptr || compile_time_check_sum == 0) {
    return FileVerifier::kIntegrityUnknown;
  }
  if (compile_time_check_sum != actual_check_sum) {
    return FileVerifier::kIntegrityCorrupted;
  }
  return FileVerifier::kIntegrityVerifiedWithPEChecksum;
}

}  // namespace

FileVerifier::IntegrityType FileVerifier::VerifyIntegrity(
    FileVerifier::MozcSystemFile system_file, string *filename_with_version) {
  if (filename_with_version == nullptr) {
    return kIntegrityInvalidParameter;
  }
  filename_with_version->clear();

  string filepath;
  string filename;
  switch (system_file) {
    case kMozcServerFile:
      filename = kMozcServerName;
      filepath = SystemUtil::GetServerPath();
      break;
    case kMozcRendererFile:
      filename = kMozcRenderer;
      filepath = SystemUtil::GetRendererPath();
      break;
    case kMozcToolFile:
      filename = kMozcTool;
      filepath = SystemUtil::GetToolPath();
      break;
  }
  if (filepath.empty()) {
    return kIntegrityFileNotFound;
  }

  wstring wfilename;
  Util::UTF8ToWide(filename, &wfilename);

  wstring wfilepath;
  Util::UTF8ToWide(filepath, &wfilepath);

  const IntegrityType result = VerifyIntegrityImpl(filepath);

  int version_major = 0;
  int version_minor = 0;
  int version_build = 0;
  int version_revision = 0;
  if (SystemUtil::GetFileVersion(wfilepath, &version_major, &version_minor,
                                 &version_build, &version_revision)) {
    filename_with_version->assign(Util::StringPrintf(
        "%s (%d.%d.%d.%d)", filename.c_str(), version_major, version_minor,
        version_build, version_revision));
  } else {
    // Failed to retrieve version info. Use the filename as fallback.
    filename_with_version->assign(filename);
  }

  return result;
}

FileVerifier::IntegrityType FileVerifier::VerifyIntegrityImpl(
    const string &filepath) {
  if (!FileUtil::FileExists(filepath)) {
    return kIntegrityFileNotFound;
  }

  wstring wfilepath;
  Util::UTF8ToWide(filepath, &wfilepath);

  WINTRUST_FILE_INFO file_info = {};
  file_info.cbStruct = sizeof(WINTRUST_FILE_INFO);
  file_info.pcwszFilePath = wfilepath.c_str();

  WINTRUST_DATA trust_data = {};
  trust_data.cbStruct = sizeof(WINTRUST_DATA);
  trust_data.dwUIChoice = WTD_UI_NONE;
  trust_data.fdwRevocationChecks = WTD_REVOKE_NONE;
  trust_data.dwUnionChoice = WTD_CHOICE_FILE;
  trust_data.dwStateAction = WTD_STATEACTION_VERIFY;
  trust_data.pFile = &file_info;
  // Note: here we use WTD_HASH_ONLY_FLAG to check the hash equality only.
  trust_data.dwProvFlags = WTD_HASH_ONLY_FLAG | WTD_REVOCATION_CHECK_NONE;
  trust_data.dwUIContext = WTD_UICONTEXT_EXECUTE;
  GUID guid = WINTRUST_ACTION_GENERIC_VERIFY_V2;

  IntegrityType integrity_type = kIntegrityVerifiedWithAuthenticodeHash;
  const LONG trust_result = ::WinVerifyTrust(
      static_cast<HWND>(INVALID_HANDLE_VALUE), &guid, &trust_data);
  if (trust_result != ERROR_SUCCESS) {
    integrity_type = kIntegrityUnknown;
    const DWORD error = ::GetLastError();
    if (error == TRUST_E_NOSIGNATURE) {
      // Note that TRUST_E_NOSIGNATURE is not only returned if the target file
      // is actually signed and the hash does not match but also if the target
      // file is not actually signed when WTD_HASH_ONLY_FLAG is specified.
      // To distinguish the former from the latter, try to obtain the code
      // sign info.
      CRYPT_PROVIDER_DATA* cpd =
          ::WTHelperProvDataFromStateData(trust_data.hWVTStateData);
      const CRYPT_PROVIDER_SGNR* cps =
          ::WTHelperGetProvSignerFromChain(cpd, 0, FALSE, 0);
      if (cps != nullptr) {
        // In this case, the target file is actually signed.
        integrity_type = kIntegrityCorrupted;
      }
    } else if (error == TRUST_E_BAD_DIGEST) {
      // Basically we should not reach here as long as WTD_HASH_ONLY_FLAG is
      // specified. We keep this code just in case WTD_HASH_ONLY_FLAG is
      // removed for some purpose.
      integrity_type = kIntegrityCorrupted;
    } else {
      DCHECK_EQ(kIntegrityUnknown, integrity_type);
      LOG(ERROR) << "WinVerifyTrust failed. error: " << error;
    }
  }
  trust_data.dwStateAction = WTD_STATEACTION_CLOSE;
  ::WinVerifyTrust(nullptr, &guid, &trust_data);

  // If the integrity state is still unknown, try to use PE header checksum
  // to detect the data corruption.
  if (integrity_type == kIntegrityUnknown) {
    integrity_type = VerifyPEHeaderChecksum(filepath);
  }

  return integrity_type;
}

}  // namespace win32
}  // namespace mozc
