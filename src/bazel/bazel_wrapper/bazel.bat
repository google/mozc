@echo off

set TMP_MOZC_BAZEL_WRAPPER_DIR=%~dp0
set TMP_MOZC_SRC_DIR=%TMP_MOZC_BAZEL_WRAPPER_DIR:~0,-21%

rem set BAZEL_LLVM only if clang-cl exists under third_party/llvm.
set TMP_MOZC_LLVM_DIR=%TMP_MOZC_SRC_DIR%\third_party\llvm\clang+llvm-20.1.0-x86_64-pc-windows-msvc
if exist %TMP_MOZC_LLVM_DIR% set BAZEL_LLVM=%TMP_MOZC_LLVM_DIR%
set TMP_MOZC_LLVM_DIR=

rem set BAZEL_SH only if MSYS2 exists under third_party/msys64/bin/bash.exe.
set TMP_MOZC_BASH_PATH=%TMP_MOZC_SRC_DIR%\third_party\msys64\usr\bin\bash.exe
if exist %TMP_MOZC_BASH_PATH% set BAZEL_SH=%TMP_MOZC_BASH_PATH%
set TMP_MOZC_BASH_PATH=

set TMP_MOZC_BAZEL_WRAPPER_DIR=
set TMP_MOZC_SRC_DIR=

%BAZEL_REAL% %* & call:myexit

:myexit
exit /b
