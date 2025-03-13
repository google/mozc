@echo off

rem set BAZEL_LLVM only if clang-cl exists under third_party/llvm.
set TMP_THIRD_PARTY_LLVN_DIR=third_party\llvm\clang+llvm-19.1.7-x86_64-pc-windows-msvc
set TMP_MOZC_BAZEL_WRAPPER_DIR=%~dp0
set TMP_MOZC_SRC_DIR=%TMP_MOZC_BAZEL_WRAPPER_DIR:~0,-21%
set TMP_MOZC_LLVM_DIR=%TMP_MOZC_SRC_DIR%\%TMP_THIRD_PARTY_LLVN_DIR%
if exist %TMP_MOZC_LLVM_DIR% set BAZEL_LLVM=%TMP_MOZC_LLVM_DIR%
set TMP_MOZC_BAZEL_WRAPPER_DIR=
set TMP_MOZC_SRC_DIR=
set TMP_MOZC_LLVM_DIR=
set TMP_THIRD_PARTY_LLVN_DIR=

%BAZEL_REAL% %* & call:myexit

:myexit
exit /b
