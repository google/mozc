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

#ifndef MOZC_WIN32_BASE_FILE_VERIFIER_H_
#define MOZC_WIN32_BASE_FILE_VERIFIER_H_

#include <string>

#include "base/port.h"

namespace mozc {
namespace win32 {

// Binary embedding technique is widely used in Mozc to bundle some data
// structure into production binaries. Basically we assume that such data
// structure never contains invalid data and there is likely to be no data
// validation against _corrupted_ embedded data. However, in the real world,
// we saw a lot of wired crashes that indicate the existence of corrupted
// on-memory data. (b/5993773, b/6714190, b/6714268)
// One of the possible scenario is the file content stored in the local
// storage is somehow but actually corrupted. To test this scenario, you can
// use this class to make sure if the content of the executable is not changed
// from the build-time.
//
// IMPORTANT: This class is not designed for security purposes.
//
// TODO(yukawa): Make cross-platform version of this class.
class FileVerifier {
 public:
  enum MozcSystemFile {
    kMozcServerUnknown = 0,
    kMozcServerFile = 1,
    kMozcRendererFile = 2,
    kMozcToolFile = 3,
  };

  enum IntegrityType {
    // Means that we there is an invalid parameter.
    kIntegrityInvalidParameter = 0,
    // Means that we got actually no information: data corruption is not
    // detected but file integrity is also not verified.
    kIntegrityUnknown,
    // Means that the specified file is not found.
    kIntegrityFileNotFound,
    // Means that the verifier couldn't open the the file specified.
    kIntegrityFileOpenFailed,
    // Means that some data corruption symptom is detected.
    kIntegrityCorrupted,
    // Means that Authenticode hash stored in the executable is verified.
    // Do not use this flag for the security purpose: code signing itself is
    // not tested for performance reasons.
    kIntegrityVerifiedWithAuthenticodeHash,
    // Means that the checksum stored in the PE header is verified. Do not use
    // this flag for the security purpose: PE checksum is not designed for the
    // security purpose.
    kIntegrityVerifiedWithPEChecksum = 5,
  };

  // Returns the result of integrity check against the file specified by
  // |system_file|.
  // Note: you cannot call this method at the same time from multiple threads
  //     due to the limitation of the dependent library.
  static IntegrityType VerifyIntegrity(MozcSystemFile system_file,
                                       string *filename_with_version);

 protected:
  // The actual implemenation for unit test.
  static IntegrityType VerifyIntegrityImpl(const string &filepath);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(FileVerifier);
};

}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_BASE_FILE_VERIFIER_H_
