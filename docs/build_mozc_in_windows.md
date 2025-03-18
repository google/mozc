How to build Mozc in Windows
============================

[![Windows](https://github.com/google/mozc/actions/workflows/windows.yaml/badge.svg)](https://github.com/google/mozc/actions/workflows/windows.yaml)

## Summary

If you are not sure what the following commands do, please check the descriptions below and make sure the operations before running them.

```
python -m pip install six requests

git clone https://github.com/google/mozc.git
cd mozc\src

python build_tools/update_deps.py
python build_tools/build_qt.py --release --confirm_license
python build_mozc.py gyp
python build_mozc.py build -c Release package

# Install Mozc
out_win\Release\Mozc64.msi
```

Hint: You can also download `Mozc64.msi` from GitHub Actions. Check [Build with GitHub Actions](#build-with-github-actions) for details.

Hint: You can use Bazel to build Mozc (experimental). For details, please see below.

## Setup

### System Requirements

64-bit Windows 10 or later.

### Software Requirements

Building Mozc on Windows requires the following software.

  * [Visual Studio 2022 Community Edition](https://visualstudio.microsoft.com/downloads/#visual-studio-community-2022)
    * [Build Tools for Visual Studio 2022](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022) should also work
  * Python 3.9 or later with the following pip modules.
    * `six`
    * `requests`
  * `.NET 6` or later (for `dotnet` command).

For additional requirements for building Mozc with Bazel, please see below.

### Install pip modules

```
python3 -m pip install six requests
```

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

In this step, additional build dependencies will be downloaded.

  * [LLVM 20.1.0](https://github.com/llvm/llvm-project/releases/tag/llvmorg-20.1.0)
  * [MSYS2 2025-02-21](https://github.com/msys2/msys2-installer/releases/tag/2025-02-21)
  * [Ninja 1.11.0](https://github.com/ninja-build/ninja/releases/download/v1.11.0/ninja-win.zip)
  * [Qt 6.8.0](https://download.qt.io/archive/qt/6.8/6.8.0/submodules/qtbase-everywhere-src-6.8.0.tar.xz)
  * [.NET tools](../dotnet-tools.json)
  * [git submodules](../.gitmodules)

You can skip this step if you would like to manually download these libraries.

## Build

### Build Qt

```
python build_tools/build_qt.py --release --confirm_license
```

If you would like to manually confirm the Qt license, drop `--confirm_license` option.

You can skip this process if you have already installed Qt prebuilt binaries.

### Build Mozc

If you have built Qt in the above step, the following commands should be sufficient to build Mozc installers.

```
python build_mozc.py gyp
python build_mozc.py build -c Release package
```

If you want to build Mozc without Qt dependencies, specify `--noqt` option as follows.  Note that `mozc_tool.exe` will be built as a mock version that does nothing if you specify `--noqt`.

```
python build_mozc.py gyp --noqt
python build_mozc.py build -c Release package
```

If you want to build Mozc with your own Qt binaries, specify `--qtdir` option as follows.

```
python build_mozc.py gyp --qtdir=<your Qt directory>
python build_mozc.py build -c Release package
```

If you need debug information, you can build debug version of Mozc as follows.

```
python build_mozc.py build_tools/build_qt.py --release --debug --confirm_license
python build_mozc.py build -c Debug package
```

### Installers

You have release build binaries in `out_win\Release\Mozc64.msi`.

### Clean up the Tree

To clean up the tree, execute the following. This will remove executables and intermediate files like object files, generated source files, project files, etc.

```
python build_mozc.py clean
```

## Install Mozc

On a 64-bit environment, run the following command (or simply double click the corresponding file).

```
out_win\Release\Mozc64.msi
```

## Uninstall Mozc

Press Win+R to open the dialog and type `ms-settings:appsfeatures-app`, or on terminal run the following command.

```
start ms-settings:appsfeatures-app
```

Then uninstall `Mozc` from the list.

---

## Run unit tests

You can run unit tests as follows.

```
python build_mozc.py gyp --noqt
python build_mozc.py runtests -c Release
```

Note that you can specify `--qtdir=` option instead of `--noqt` in GYP phase since currently there is no unit test that depends on Qt.

---

## Build with Bazel (experimental)

Additional requirements:

* [Bazelisk](https://github.com/bazelbuild/bazelisk)
  * Bazelisk is a wrapper of [Bazel](https://bazel.build) to use the specific version of Bazel.
  * [src/.bazeliskrc](../src/.bazeliskrc) controls which version of Bazel is used.

After running `build_tools/update_deps.py` and `build_tools/build_qt.py`, run the following command instead of `build_mozc.py`:

```
bazelisk build --config oss_windows --config release_build package
```

You have release build binaries in `bazel-bin\win32\installer\Mozc64.msi`.

### Tips for Bazel setup

* You do not need to install a new JDK just for Mozc.

---

## Build with GitHub Actions

GitHub Actions are already set up in [windows.yaml](../.github/workflows/windows.yaml). With that, you can build and install Mozc with your own commit as follows.

1. Fork https://github.com/google/mozc to your GitHub repository.
2. Push a new commit to your own fork.
3. Click "Actions" tab on your fork.
4. Wait until the action triggered with your commit succeeds.
5. Download `Mozc64.msi` from the action result page if you are using 64-bit Windows.
6. Install `Mozc64.msi`.

Files in the GitHub Actions page remain available up to 90 days.

You can also find Mozc Installers for Windows in google/mozc repository. Please keep in mind that Mozc is not an officially supported Google product, even if downloaded from https://github.com/google/mozc/.

1. Sign in GitHub.
2. Check [recent successfull Windows runs](https://github.com/google/mozc/actions/workflows/windows.yaml?query=is%3Asuccess) in google/mozc repository.
3. Find action in last 90 days and click it.
4. Download `Mozc64.msi` from the action result page if you are using 64-bit Windows.

