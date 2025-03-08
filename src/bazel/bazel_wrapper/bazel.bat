@echo off
%BAZEL_REAL% %* & call:myexit

:myexit
exit /b
