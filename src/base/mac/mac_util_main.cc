// Copyright 2010-2021, Google Inc.
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

#include <iostream>
#include <ostream>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "base/init_mozc.h"
#include "base/mac/mac_util.h"
#include "base/util.h"

ABSL_FLAG(bool, label_for_suffix, false,
          "call GetLabelForSuffix when specified");
ABSL_FLAG(bool, application_support_directory, false,
          "call GetApplicationSupportDirectory when specified");
ABSL_FLAG(bool, logging_directory, false,
          "call GetLoggingDirectory when specified");
ABSL_FLAG(bool, os_version_string, false,
          "call GetOSVersionString when specified");
ABSL_FLAG(bool, server_directory, false,
          "call GetServerDirectory when specified");
ABSL_FLAG(bool, serial_number, false, "call GetSerialNumber when specified");
ABSL_FLAG(bool, start_launchd_service, false,
          "call StartLaunchdService when specified");

ABSL_FLAG(std::string, suffix, "", "The argument for GetLabelForSuffix");
ABSL_FLAG(std::string, service_name, "", "The service name to be launched");

#ifdef __APPLE__
using mozc::MacUtil;
#endif  // __APPLE__

int main(int argc, char **argv) {
  mozc::InitMozc(argv[0], &argc, &argv);

#ifdef __APPLE__
  if (absl::GetFlag(FLAGS_label_for_suffix)) {
    std::cout << MacUtil::GetLabelForSuffix(absl::GetFlag(FLAGS_suffix))
              << std::endl;
  }

  if (absl::GetFlag(FLAGS_application_support_directory)) {
    std::cout << MacUtil::GetApplicationSupportDirectory() << std::endl;
  }

  if (absl::GetFlag(FLAGS_logging_directory)) {
    std::cout << MacUtil::GetLoggingDirectory() << std::endl;
  }

  if (absl::GetFlag(FLAGS_os_version_string)) {
    std::cout << MacUtil::GetOSVersionString() << std::endl;
  }

  if (absl::GetFlag(FLAGS_server_directory)) {
    std::cout << MacUtil::GetServerDirectory() << std::endl;
  }

  if (absl::GetFlag(FLAGS_serial_number)) {
    std::cout << MacUtil::GetSerialNumber() << std::endl;
  }

  if (absl::GetFlag(FLAGS_start_launchd_service)) {
    if (absl::GetFlag(FLAGS_service_name).empty()) {
      std::cout << "Specify the service name to be launched" << std::endl;
    } else {
      pid_t pid;
      MacUtil::StartLaunchdService(absl::GetFlag(FLAGS_service_name), &pid);
      std::cout << "pid: " << pid << std::endl;
    }
  }
#else   // __APPLE__
  std::cout << "This command works on macOS or iOS only." << std::endl;
#endif  // __APPLE__

  return 0;
}
