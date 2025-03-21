How to build Mozc on macOS
=========================

[![macOS](https://github.com/google/mozc/actions/workflows/macos.yaml/badge.svg)](https://github.com/google/mozc/actions/workflows/macos.yaml)

## Summary

If you are not sure what the following commands do, please check the descriptions below
and make sure the operations before running them.

```
git clone https://github.com/google/mozc.git
cd mozc/src

python3 build_tools/update_deps.py

# CMake is also required to build Qt.
python3 build_tools/build_qt.py --release --confirm_license

MOZC_QT_PATH=${PWD}/third_party/qt bazelisk build package --config oss_macos --config release_build
open bazel-bin/mac/Mozc.pkg
```

💡 With the above build steps, the target CPU architecture of the binaries in
`Mozc.pkg` is the same as the CPU architecture of the build environment.
That is, if you build Mozc on arm64 environment, `Mozc.pkg` contains arm64
binaries.  See the
["how to specify target CPU architectures"](#how-to-specify-target-cpu-architectures)
section below about how to do cross build.

💡 You can also download `Mozc.pkg` from GitHub Actions.
Check [Build with GitHub Actions](#build-with-github-actions) for details.

## Setup

### System Requirements

64-bit macOS 11 and later versions are supported.

### Software Requirements

Building on Mac requires the following software.

* [Xcode](https://apps.apple.com/us/app/xcode/id497799835)
  * Xcode 13 (macOS 13 SDK) or later
  * ⚠️Xcode Command Line Tools aren't sufficient.
* [Bazelisk](https://github.com/bazelbuild/bazelisk)
  * Bazelisk is a wrapper of [Bazel](https://bazel.build/) to use the specific version of Bazel.
  * [src/.bazeliskrc](../src/.bazeliskrc) controls which version of Bazel is used.
* Python 3.9 or later.
* CMake 3.18.4 or later (to build Qt6)

## Get the Code

You can download Mozc source code as follows:

```
git clone https://github.com/google/mozc.git
cd mozc/src
```

Hereafter you can do all the operations without changing directory.

### Check out additional build dependencies

```
python build_tools/update_deps.py
```

In this step, additional build dependencies will be downloaded.

  * [Ninja 1.11.0](https://github.com/ninja-build/ninja/releases/download/v1.11.0/ninja-mac.zip)
  * [Qt 6.8.0](https://download.qt.io/archive/qt/6.8/6.8.0/submodules/qtbase-everywhere-src-6.8.0.tar.xz)
  * [git submodules](../.gitmodules)

You can specify `--noqt` option if you would like to use your own Qt binaries.

### Build Qt

```
python3 build_tools/build_qt.py --release --confirm_license
```

Drop `--confirm_license` option if you would like to manually confirm the Qt license.

You can also specify `--debug` option to build debug version of Mozc.

```
python3 build_tools/build_qt.py --release --debug --confirm_license
```

You can also specify `--macos_cpus` option, which has the same semantics as the
[same name option in Bazel](https://bazel.build/reference/command-line-reference#flag--macos_cpus),
for cross-build including building a Universal macOS Binary.

```
# Building x86_64 binaries regardless of the host CPU architecture.
python3 build_tools/build_qt.py --release --debug --confirm_license --macos_cpus=x86_64

# Building Universal macOS Binary for both x86_64 and arm64.
python3 build_tools/build_qt.py --release --debug --confirm_license --macos_cpus=x86_64,arm64
```

You can skip this process if you have already installed Qt prebuilt binaries.

CMake is also required to build Qt. If you use `brew`, you can install `cmake` as follows.

```
brew install cmake
```

-----

## Build with Bazel

### Build installer

```
MOZC_QT_PATH=${PWD}/third_party/qt bazelisk build package --config oss_macos --config release_build
open bazel-bin/mac/Mozc.pkg
```

#### How to specify target CPU architectures

To build an Intel64 macOS binary regardless of the host CPU architecture.
```
python3 build_tools/build_qt.py --release --debug --confirm_license --macos_cpus=x64_64
MOZC_QT_PATH=${PWD}/third_party/qt bazelisk build package --config oss_macos --config release_build --macos_cpus=x64_64
open bazel-bin/mac/Mozc.pkg
```

To build a Universal macOS Binary both x86_64 and arm64.
```
python3 build_tools/build_qt.py --release --debug --confirm_license --macos_cpus=x86_64,arm64
MOZC_QT_PATH=${PWD}/third_party/qt bazelisk build package --config oss_macos --config release_build --macos_cpus=x86_64,arm64
open bazel-bin/mac/Mozc.pkg
```

### Unit tests

```
MOZC_QT_PATH=${PWD}/third_party/qt bazelisk test ... --config oss_macos --build_tests_only -c dbg
```

See [build Mozc in Docker](build_mozc_in_docker.md#unittests) for details.


### Edit src/config.bzl

You can modify variables in `src/config.bzl` to fit your environment.
Note: `~` does not represent the home directory.
The exact path should be specified (e.g. `MACOS_QT_PATH = "/Users/mozc/myqt"`).

Tips: the following command makes the specified file untracked by Git.
```
git update-index --assume-unchanged src/config.bzl
```

This command reverts the above change.
```
git update-index --no-assume-unchanged src/config.bzl
```

-----

## Build with GitHub Actions

GitHub Actions steps are already set up in [macos.yaml](../.github/workflows/macos.yaml). With that, you can build and install Mozc with your own commit as follows.

1. Fork https://github.com/google/mozc to your GitHub repository.
2. Push a new commit to your own fork.
3. Click "Actions" tab on your fork.
4. Wait until the action triggered with your commit succeeds.
5. Download `Mozc.pkg` from the action result page.
6. Install `Mozc.pkg`.

Files in the GitHub Actions page remain available up to 90 days.

You can also find Mozc Installers for macOS in google/mozc repository. Please keep in mind that Mozc is not an officially supported Google product, even if downloaded from https://github.com/google/mozc/.

1. Sign in GitHub.
2. Check [recent successful macOS runs](https://github.com/google/mozc/actions/workflows/macos.yaml?query=is%3Asuccess) in google/mozc repository.
3. Find action in last 90 days and click it.
4. Download `Mozc.pkg` from the action result page.

-----

## Build with GYP (maintenance mode)

GYP build is under maintenance mode. Bazel build is recommended.

While the existing targets are supported by both GYP and Bazel as much as possible,
new targets will be supported by Bazel only.

Targets only for Bazel:

* AUX dictionary (//data/dictionary_oss:aux_dictionary)
* Filtered dictionary (//data/dictionary_oss:filtered_dictionary)
* SVS character input instead of CJK compatibility ideographs (//rewriter:single_kanji_rewriter)
* Zip code conversion (//server:mozc_server)

### Software Requirements

For GYP build, Ninja and Packages are also required.

* [Ninja](https://github.com/ninja-build/ninja) for GYP build
* [Packages](http://s.sudre.free.fr/Software/Packages/about.html) for installer
* Python 3.9 or later with the following pip module.
  * `six`

### Build executables

First, you'll need to generate Xcode project using a tool called [GYP](https://chromium.googlesource.com/external/gyp).

```
GYP_DEFINES="mac_sdk=13.0 mac_deployment_target=11.0" python3 build_mozc.py gyp
```

You can customize the SDK version target OS version here.

Then, build Mozc.app and necessary files:

```
python3 build_mozc.py build -c Release mac/mac.gyp:GoogleJapaneseInput
```

If you want to build Mozc without Qt dependencies, specify `--noqt` option as follows.  Note that GUI tools will be built as a mock version that does nothing if you specify `--noqt`.

```
GYP_DEFINES="mac_sdk=13.0 mac_deployment_target=11.0" python3 build_mozc.py gyp --noqt
python3 build_mozc.py build -c Release mac/mac.gyp:GoogleJapaneseInput
```

#### Executables

Executables are written in `out_mac/Release` for Release builds, and `out_mac/Debug` for Debug builds. For instance, you'll have `out_mac/Release/Mozc.app` once the build finishes successfully in the Release mode.

GUI tools executables are linked with the libraries in `third_party/qt`. You might want to change it with `install_name_tool`.

### Installer

You can also build an installer.
```
GYP_DEFINES="mac_sdk=13.0 mac_deployment_target=11.0" python3 build_mozc.py gyp
python3 build_mozc.py build -c Release :Installer
```


### Clean up the Tree

To clean up the tree, execute the following. This will remove executables and intermediate files like object files, generated source files, project files, etc.

```
python3 build_mozc.py clean
```

### Install built packages

Without building an installer .pkg file, you can just place the created Mozc.app into `/Library/Input Methods`, and `mac/installer/LaunchAgents/org.mozc.inputmethod.Japanese.Converter.plist` and `org.mozc.inputmethod.Japanese.Renderer.plist` into `/Library/LaunchAgents`, and then log in again.  Then it works well.

```
sudo cp -r out_mac/Release/Mozc.app /Library/Input\ Methods/
sudo cp mac/installer/LaunchAgents/org.mozc.inputmethod.Japanese.Converter.plist /Library/LaunchAgents
sudo cp mac/installer/LaunchAgents/org.mozc.inputmethod.Japanese.Renderer.plist /Library/LaunchAgents
```
