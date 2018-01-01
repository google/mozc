// Copyright 2010-2018, Google Inc.
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

#ifndef MOZC_BASE_PEPPER_FILE_UTIL_H_
#define MOZC_BASE_PEPPER_FILE_UTIL_H_

#ifdef OS_NACL

#include <ppapi/cpp/instance.h>
#include <ppapi/c/pp_file_info.h>

#include <string>

#include "base/mmap_sync_interface.h"
#include "base/port.h"

namespace mozc {

// Interface for Papper File system.
class PepperFileSystemInterface {
 public:
  virtual ~PepperFileSystemInterface() {}
  virtual bool Open(pp::Instance *instance, int64 expected_size) = 0;
  virtual bool FileExists(const string &filename) = 0;
  virtual bool DirectoryExists(const string &dirname) = 0;
  virtual bool ReadBinaryFile(const string &filename, string *buffer) = 0;
  virtual bool WriteBinaryFile(const string &filename,
                               const string &buffer) = 0;
  virtual bool CreateDirectory(const string &dirname) = 0;
  virtual bool Delete(const string &path) = 0;
  virtual bool Rename(const string &from, const string &to) = 0;
  virtual bool RegisterMmap(MmapSyncInterface *mmap) = 0;
  virtual bool UnRegisterMmap(MmapSyncInterface *mmap) = 0;
  virtual bool SyncMmapToFile() = 0;
  virtual bool Query(const string &path, PP_FileInfo *file_info) = 0;
};

// Utility class for Pepper FileIO.
// This class can't be used in the NaCl main thread.
// The Pepper FileIO APIs must be called in the NaCl main thread, and they are
// asynchronous. So the methods of PepperFileUtil calls Pepper FileIO APIs in
// the NaCl main thread using pp::Core::CallOnMainThread() and waits the result
// using mozc::UnnamedEvent.
class PepperFileUtil {
 public:
  // Initializes PepperFileUtil.
  static bool Initialize(pp::Instance *instance, int64 expected_size);

  // Sets mock for unit testing.
  static void SetPepperFileSystemInterfaceForTest(
      PepperFileSystemInterface *mock_interface);

  // Returns true if a file or a directory with the name exists.
  static bool FileExists(const string &filename);

  // Returns true if the directory exists.
  static bool DirectoryExists(const string &dirname);

  // Reads a file and returns ture.
  // If the file does not exist or an error occurs returns false.
  static bool ReadBinaryFile(const string &filename, string *buffer);

  // Writes a file.
  static bool WriteBinaryFile(const string &filename, const string &buffer);

  // Creates an empty directory.
  static bool CreateDirectory(const string &dirname);

  // Deletes a file or an empty directory.
  static bool Delete(const string &path);

  // Renames a file or an directory.
  static bool Rename(const string &from, const string &to);

  // Registers Mmap object.
  static bool RegisterMmap(MmapSyncInterface *mmap);

  // Unegisters Mmap object.
  static bool UnRegisterMmap(MmapSyncInterface *mmap);

  // Call SyncToFile() method of the all registered Mmap objects.
  static bool SyncMmapToFile();

  // Queries file information.
  static bool Query(const string &path, PP_FileInfo *file_info);
};

}  // namespace mozc

#endif  // OS_NACL

#endif  // MOZC_BASE_PEPPER_FILE_UTIL_H_
