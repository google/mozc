---
name: Build error
about: Issues for building Mozc
title: ''
labels: ''
assignees: ''

---

**Description**
A clear and concise description of what the issue is.


**Commit-id**
[e.g. d50a8b9ae28c4fba265f734b38bc5ae392fe4d25]
If this is not the latest commit, please try the latest commit before reporting.


**Build target**
Choose one of them
1. Docker build for Linux and Android-lib
  + https://github.com/google/mozc/blob/master/docs/build_mozc_in_docker.md
2. macOS build
  + https://github.com/google/mozc/blob/master/docs/build_mozc_in_osx.md
3. Windows build
  + https://github.com/google/mozc/blob/master/docs/build_mozc_in_windows.md
4. Others (no guarantee)


**CI build status**
Whether the current CI build status is `passing` or `failure`.
https://github.com/google/mozc#build-status

Choose either of them
1. passing
2. failure


**Environment:**
 - OS: [e.g. Ubuntu 20.04, macOS 13.1, etc]
 - Python version (optional): [e.g. 3.7, etc. 3.7+ is required]
 - Qt5 version (optional): [e.g. 5.12, etc. 5.12+ is recommended]
 - Compiler version (optional): [e.g. MSVC 2019, Xcode 14.1, etc.]


**Build commands**
Steps of command lines to reproduce your error.
1. git ...
2. cd ...
3. bazel ...


**Error logs**

```
(copy-and-paste here)
```


**Additional context**
Add any other context about the problem here.

