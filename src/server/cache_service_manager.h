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

#ifndef MOZC_SERVER_CACHE_SERVICE_MANAGER_H_
#define MOZC_SERVER_CACHE_SERVICE_MANAGER_H_

#ifdef OS_WINDOWS
#include <stdio.h>
#include <string>

namespace mozc {
class CacheServiceManager {
 public:
  // return true if the cache service is installed.
  static bool IsInstalled();

  // return true if the cache service is not disabled,
  // i.e. registered as "auto start" mode
  static bool IsEnabled();

  // return true if the cache service is running.
  static bool IsRunning();

  // return the name of cache service.
  static const wchar_t *GetServiceName();

  // Return the unquoted path to the cache service exe file.
  static wstring GetUnquotedServicePath();

  // Return the quoted path to the cache service exe file.
  static wstring GetQuotedServicePath();

  // Enable the autostart of the cache service.
  // Fails if the cache service is not installed.
  // Config dialog UI can use this API.
  static bool EnableAutostart();

  // Disable the cache service.
  // It first stops the service if needed.
  // Config dialog UI can use this API.
  static bool DisableService();

  // Restart cache service.
  // Even when the service is stopped, this method
  // tries to start the cache service.
  // If the service is "Disable" mode, RestartService() fails.
  // You can combine IsEnabled() / IsRunning()
  // to control the conditions of the restart
  static bool RestartService();

  // return true if the machine has enough memory
  // to start the cache service
  static bool HasEnoughMemory();

  // Return true if the current status of the serivice is successfully
  // serialized into the specified wstring instance.
  // You can assume that the serialized data consists of ASCII printable
  // characters.
  // If the cache service is not installed, default settings is returned
  // with setting |installed| flag to false.
  static bool BackupStateAsString(wstring *buffer);

  // Return true if the previous status of the service is successfully
  // restored from the specified wstring instance.
  // This function may start the service if needed.
  static bool RestoreStateFromString(const wstring &buffer);

  // Return true if:
  // - The cache service is not installed.
  // - The cache service is installed but stopped when this function is called.
  // - The cache service is installed and running but successfully stopped by
  //   this function.
  static bool EnsureServiceStopped();


 private:
  CacheServiceManager() {}
  ~CacheServiceManager() {}
};
}  // namespace mozc
#endif  // OS_WINDOWS
#endif  // MOZC_SERVER_CAC_SERVICE_MANAGER_H_
