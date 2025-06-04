How to build Mozc in Windows
============================

[![Windows](https://github.com/google/mozc/actions/workflows/windows.yaml/badge.svg)](https://github.com/google/mozc/actions/workflows/windows.yaml)

## Summary

If you are unsure about what the following commands do, please review the
descriptions below to understand the operations before running them.

```
git clone https://github.com/google/mozc.git
cd mozc\src

python build_tools/update_deps.py
python build_tools/build_qt.py --release --confirm_license
bazelisk build --config oss_windows --config release_build package

python build_tools/open.py bazel-bin/win32/installer/Mozc64.msi
```

> [!TIP]
> You can also download `Mozc64.msi` from GitHub Actions. Check
> [Build with GitHub Actions](#build-with-github-actions) for details.

## Setup

### System Requirements

64-bit Windows 10 or later.

### Software Requirements

Building Mozc on Windows requires the following software.

  * [Visual Studio 2022 Community Edition](https://visualstudio.microsoft.com/downloads/#visual-studio-community-2022)
    with the following components.
    * Windows 11 SDK
    * MSVC v143 - VS 2022 C++ x64/x86 build tools (Latest)
    * C++ ATL for latest v143 build tools (x86 & x64)
  * Python 3.9 or later.
  * `.NET 6` or later (for `dotnet` command).
  * [Bazelisk](https://github.com/bazelbuild/bazelisk)

> [!NOTE]
> Bazelisk is a wrapper of [Bazel](https://bazel.build) that allows you to use a
> specific version of Bazel.

### Download the repository from GitHub

```
git clone https://github.com/google/mozc.git
cd mozc\src
```

Hereafter you can do all the operations without changing directory.

### Check out additional build dependencies

```
python build_tools/update_deps.py
```

In this step, additional build dependencies will be downloaded, including:

  * [LLVM 20.1.1](https://github.com/llvm/llvm-project/releases/tag/llvmorg-20.1.1)
  * [MSYS2 2025-02-21](https://github.com/msys2/msys2-installer/releases/tag/2025-02-21)
  * [Ninja 1.11.0](https://github.com/ninja-build/ninja/releases/tag/v1.11.0)
  * [Qt 6.9.1](https://download.qt.io/archive/qt/6.8/6.8.0/submodules/qtbase-everywhere-src-6.9.1.tar.xz)
  * [.NET tools](../dotnet-tools.json)
  * [git submodules](../.gitmodules)

## Build

### Build Qt

```
python build_tools/build_qt.py --release --confirm_license
```

If you would like to manually confirm the Qt license, omit the
`--confirm_license` option.

### Build Mozc

Assuming `bazelisk` is in your `%PATH%`, run the following command to build Mozc
for Windows.

```
bazelisk build --config oss_windows --config release_build package
```

#### Install Mozc
After building Mozc, run the following command to install it:

```
python build_tools/open.py bazel-bin/win32/installer/Mozc64.msi
```

#### Uninstall Mozc
To Uninstall Mozc, press <kbd>Win</kbd>+<kbd>R</kbd> to open the Run dialog and
type and type `ms-settings:appsfeatures-app`, run the following command in the
terminal:

```
start ms-settings:appsfeatures-app
```

Then, uninstall `Mozc` from the list of installed applications.

## Bazel command examples

### Bazel User Guide
  * [Build programs with Bazel](https://bazel.build/run/build)
  * [Commands and Options](https://bazel.build/docs/user-manual)
  * [Write bazelrc configuration files](https://bazel.build/run/bazelrc)

### Run all tests

```
bazelisk test ... --config oss_windows --build_tests_only -c dbg
```

> [!NOTE]
> `...` means all targets under the current and subdirectories.

---

## Build with GitHub Actions

GitHub Actions are already set up in
[windows.yaml](../.github/workflows/windows.yaml). With that, you can build and
install Mozc with your own commit as follows.

1. Fork https://github.com/google/mozc to your GitHub repository.
2. Push a new commit to your own fork.
3. Click "Actions" tab on your fork.
4. Wait until the action triggered by your commit succeeds.
5. Download `Mozc64.msi` from the action result page.
6. Install `Mozc64.msi`.

Files on the GitHub Actions page remain available for up to 90 days.

You can also find Mozc Installers for Windows in the google/mozc repository.
Please keep in mind that Mozc is not an officially supported Google product,
even if downloaded from https://github.com/google/mozc/.

1. Sign in GitHub.
2. Check [recent successful Windows runs](https://github.com/google/mozc/actions/workflows/windows.yaml?query=is%3Asuccess) in the google/mozc repository.
3. Find an action from the last 90 days and click it.
4. Download `Mozc64.msi` from the action result page if you are using 64-bit
   Windows.

---

## Build with GYP (maintenance mode)

GYP build is in maintenance mode.

While the existing targets are supported by both GYP and Bazel as much as
possible, new targets will be supported by Bazel only.

Targets only for Bazel:

* AUX dictionary (`//data/dictionary_oss:aux_dictionary`)
* Filtered dictionary (`//data/dictionary_oss:filtered_dictionary`)
* SVS character input instead of CJK compatibility ideographs (`//rewriter:single_kanji_rewriter`)
* Zip code conversion (`//server:mozc_server`)

Here are the build commands to build Mozc using GYP and then install it:

```
python -m pip install six

git clone https://github.com/google/mozc.git
cd mozc\src

python build_tools/update_deps.py
python build_tools/build_qt.py --release --confirm_license
python build_mozc.py gyp
python build_mozc.py build -c Release package

out_win\Release\Mozc64.msi
```

### Build Mozc with GYP

If you have already built Qt, the following commands should be sufficient to
build the Mozc installers.

```
python build_mozc.py gyp
python build_mozc.py build -c Release package
```

If you want to build Mozc without Qt dependencies, specify `--noqt` option as
follows. Note that if you specify `--noqt`, `mozc_tool.exe` will be built as a
mock version that does nothing.

```
python build_mozc.py gyp --noqt
python build_mozc.py build -c Release package
```

If you need debug information, you can build debug version of Mozc as follows.

```
python build_mozc.py build_tools/build_qt.py --release --debug --confirm_license
python build_mozc.py build -c Debug package
```

### Clean up the Tree with GYP

To clean up the tree, execute the following. This will remove executables and
intermediate files like object files, generated source files, project files,
etc.

```
python build_mozc.py clean
```

### Run unit tests with GYP

You can run unit tests as follows.

```
python build_mozc.py gyp --noqt
python build_mozc.py runtests -c Release
```

Note that you can specify the `--qtdir=` option instead of `--noqt` in the GYP
phase, as there are currently no unit tests that depend on Qt.
