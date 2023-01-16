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

#ifndef MOZC_BASE_WINMAIN_H_
#define MOZC_BASE_WINMAIN_H_

// When we want to make a non-console Windows application,
// we need to prepare WinMain function as an entry point.
// However, making both WinMain() and main() and switching them
// #ifdef hack are too dirty.
//
// This header is trying to hide WinMain function from users
// and automatically dispatches WinMain() args to main() args.
// We can use standard main(argc, arg) entry point transparently
// as long as this header file is included.
//
// Usage:
//  #include "winmain.h"   // Use WinMain
//  // here main() is automatically converted to WinMain
//  int main(int argc,  char *argv[]) { .. }
#ifdef OS_WIN
// clang-format off
#include <Windows.h>
#include <ShellAPI.h>  // for CommandLineToArgvW
// clang-format on

#include "base/const.h"
#include "base/port.h"
#include "base/util.h"

namespace mozc {
// Wrapper Class for WinMain Command Line:
// WinMain's entry point is not argc and argv style.
// WinCommandLine() internally converts the argument and
// program name into standard argc/argv parameters.
class WinCommandLine {
 public:
  WinCommandLine() : argc_(0), argv_(nullptr) {
    LPWSTR *argvw = ::CommandLineToArgvW(::GetCommandLineW(), &argc_);
    if (argvw == nullptr) {
      return;
    }

    argv_ = new char *[argc_];
    for (int i = 0; i < argc_; ++i) {
      std::string str;
      mozc::Util::WideToUtf8(argvw[i], &str);
      argv_[i] = new char[str.size() + 1];
      ::memcpy(argv_[i], str.data(), str.size());
      argv_[i][str.size()] = '\0';
    }

    ::LocalFree(argvw);
  }

  WinCommandLine(const WinCommandLine &) = delete;
  WinCommandLine &operator=(const WinCommandLine &) = delete;

  virtual ~WinCommandLine() {
    for (int i = 0; i < argc_; ++i) {
      delete[] argv_[i];
    }
    delete[] argv_;
    argv_ = nullptr;
  }

  int argc() const { return argc_; }
  char **argv() const { return argv_; }

 private:
  int argc_;
  char **argv_;
};
}  // namespace mozc
// force to use WinMain.
#pragma comment(linker, "/SUBSYSTEM:WINDOWS")

int WinMainToMain(int argc, char *argv[]);

// Replace the main() function with WinMainToMain
// in order to disable the entiry point main()
#define main(argc, argv) WinMainToMain(argc, argv)

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
#ifndef MOZC_NO_LOGGING
  {
    // Load debug_sleep_time from registry.
    // With this parameter, developer can inject a debugger
    // while the main process is sleeping.
    DWORD sleep_time = 0;
    DWORD size = sizeof(sleep_time);
    DWORD vt = 0;
    HKEY hKey = 0;
    if (ERROR_SUCCESS == ::RegOpenKeyExW(HKEY_CURRENT_USER, mozc::kMozcRegKey,
                                         0, KEY_READ, &hKey)) {
      if (ERROR_SUCCESS == ::RegQueryValueExW(
                               hKey, L"debug_sleep_time", nullptr, &vt,
                               reinterpret_cast<BYTE *>(&sleep_time), &size) &&
          vt == REG_DWORD && sleep_time > 0) {
        ::Sleep(sleep_time * 1000);
      }
      ::RegCloseKey(hKey);
    }
  }
#endif  // !MOZC_NO_LOGGING

  mozc::WinCommandLine cmd;
  int argc = cmd.argc();
  char **argv = cmd.argv();

  // call main()
  return WinMainToMain(argc, argv);
}
#endif  // OS_WIN
#endif  // MOZC_BASE_WINMAIN_H_
