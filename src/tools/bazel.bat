@echo off
SET selfpath=%~dp0
set BAZEL_LLVM=%selfpath:~0,-6%\third_party\llvm\clang+llvm-19.1.7-x86_64-pc-windows-msvc
%BAZEL_REAL% %* & call:myexit

:myexit
exit /b
