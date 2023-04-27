How to build Mozc on macOS
=========================

[![macOS](https://github.com/google/mozc/actions/workflows/macos.yaml/badge.svg)](https://github.com/google/mozc/actions/workflows/macos.yaml)

## Summary

If you are not sure what the following commands do, please check the descriptions below
and make sure the operations before running them.

```
mkdir -p ~/work
cd ~/work
git clone https://github.com/google/mozc.git -b master --single-branch --recursive

cd ~/work/mozc/src
MOZC_QT_PATH={Your_Qt_path} bazel build package --config oss_macos -c opt
open bazel-bin/mac/Mozc.pkg
```

Hint: You can also download `Mozc.pkg` from GitHub Actions. Check [Build with GitHub Actions](#build-with-github-actions) for details.

## Get the Code

You can download Mozc source code as follows:

```
mkdir -p ~/work
cd ~/work
git clone https://github.com/google/mozc.git -b master --single-branch --recursive
```

## System Requirements

64-bit macOS 10.14 and later versions are supported.

## Software Requirements

Building on Mac requires the following software.

* Xcode
* [Bazel](https://docs.bazel.build/versions/master/install-os-x.html) for Bazel build
* [Qt 5](https://download.qt.io/official_releases/qt/) for GUI (e.g. qtbase [5.15.8](https://download.qt.io/official_releases/qt/5.15/5.15.8/submodules/))

To build the installer (target: `package`) or GUI tools, you need Qt.

### Build Qt

Qt is required for GUI tools. The following is an example of build configuration.

```
# extract qtbase-everywhere-opensource-src-5.NN.NN.tar.xz to ~/myqt.
cd ~/myqt
./configure -opensource -developer-build -platform macx-clang -release
make
```

The following configure options build a smaller Qt for Mozc.
```
./configure -opensource -developer-build -platform macx-clang -release \
-nomake examples -nomake tests -nomake tools \
-no-cups -no-dbus -no-icu -no-opengl \
-no-sql-db2 -no-sql-ibase -no-sql-mysql -no-sql-oci -no-sql-odbc -no-sql-psql \
-no-sql-sqlite -no-sql-sqlite2 -no-sql-tds
```

-----

## Bazel build

### Build installer

```
cd ~/work/mozc/src
MOZC_QT_PATH=/Users/mozc/myqt bazel build package --config oss_macos -c opt
open bazel-bin/mac/Mozc.pkg
```
You can use `MOZC_QT_PATH` to specificy the Qt5 directory.

### Unittests

```
bazel test ... --config oss_macos -c dbg -- -net/... -third_party/...
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
2. Check [recent successfull macOS runs](https://github.com/google/mozc/actions/workflows/macos.yaml?query=is%3Asuccess) in google/mozc repository.
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

### Path to Python3

You might need to use the preinstalled Python3 (i.e. /usr/bin/python3).
Make sure the path to python3.

```
% whereis python3
/usr/bin/python3
```

### Install python3 dependencies

```
python3 -m pip install six
```

### Build main converter and composition UI.

First, you'll need to generate Xcode project using a tool called [GYP](https://chromium.googlesource.com/external/gyp).

```
cd ~/work/mozc/src
GYP_DEFINES="mac_sdk=10.15 mac_deployment_target=10.9" python3 build_mozc.py gyp --noqt
```

You can customize the SDK version and target OS version here. Then, build Mozc.app and necessary files:

```
python3 build_mozc.py build -c Release mac/mac.gyp:GoogleJapaneseInput
```

#### Executables

Executables are written in `~/work/mozc/src/out_mac/Release` for Release builds, and `~/work/mozc/src/out_mac/Debug` for Debug builds. For instance, you'll have `~/work/mozc/src/out_mac/Release/Mozc.app` once the build finishes successfully in the Release mode.

### GUI tools

To build GUI tools, you need to specify --qtdir instead of --noqt.

```
GYP_DEFINES="mac_sdk=10.15 mac_deployment_target=10.9" python3 build_mozc.py gyp --qtdir ~/myqt
```

In the above case, `~/myqt` should contain Qt's sources, headers and libraries built from `configure` and `make`.

Then you can build `executable`s defined in `gui/gui.gyp`.  Here is an example to build the config dialog.

```
python3 build_mozc.py build -c Release gui/gui.gyp:config_dialog_main
```

These executables are linked with the libraries in `~/myqt`.  You might want to change it with `install_name_tool`.


### Installer

You can also build an installer.
```
GYP_DEFINES="mac_sdk=10.15 mac_deployment_target=10.9" python3 build_mozc.py gyp --qtdir ~/myqt
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
