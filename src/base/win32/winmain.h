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

#ifndef MOZC_BASE_WIN32_WINMAIN_H_
#define MOZC_BASE_WIN32_WINMAIN_H_

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
#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <shellapi.h>  // for CommandLineToArgvW
// clang-format on
#include <wil/resource.h>

#include <string>
#include <vector>

#include "base/const.h"
#include "base/win32/wide_char.h"

namespace mozc {
// Wrapper Class for WinMain Command Line:
// WinMain's entry point is not argc and argv style.
// WinCommandLine() internally converts the argument and
// program name into standard argc/argv parameters.
class WinCommandLine {
 public:
  WinCommandLine() {
    wil::unique_any<wchar_t**, decltype(&::LocalFree), ::LocalFree> argvw(
        ::CommandLineToArgvW(::GetCommandLineW(), &argc_));
    if (argvw == nullptr) {
      return;
    }

    args_.reserve(argc_);
    argv_.resize(argc_);
    for (int i = 0; i < argc_; ++i) {
      args_.push_back(win32::WideToUtf8(argvw.get()[i]));
      argv_[i] = args_.back().data();
    }
  }

  int argc() const { return argc_; }
  char** argv() { return argv_.data(); }

 private:
  int argc_;
  std::vector<char*> argv_;
  std::vector<std::string> args_;
};
}  // namespace mozc
// force to use WinMain.
#pragma comment(linker, "/SUBSYSTEM:WINDOWS")

int WinMainToMain(int argc, char* argv[]);

// Replace the main() function with WinMainToMain
// in order to disable the entry point main()
#define main(argc, argv) WinMainToMain(argc, argv)

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
#ifndef NDEBUG
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
      if (ERROR_SUCCESS ==
              ::RegQueryValueExW(hKey, L"debug_sleep_time", nullptr, &vt,
                                 reinterpret_cast<BYTE*>(&sleep_time), &size) &&
          vt == REG_DWORD && sleep_time > 0) {
        ::Sleep(sleep_time * 1000);
      }
      ::RegCloseKey(hKey);
    }
  }
#endif  // !NDEBUG

  mozc::WinCommandLine cmd;
  int argc = cmd.argc();
  char** argv = cmd.argv();

  // call main()
  return WinMainToMain(argc, argv);
}
#endif  // _WIN32
#endif  // MOZC_BASE_WIN32_WINMAIN_H_
