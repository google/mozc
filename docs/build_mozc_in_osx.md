How to build Mozc on macOS
=========================

[![macOS](https://github.com/google/mozc/actions/workflows/macos.yaml/badge.svg)](https://github.com/google/mozc/actions/workflows/macos.yaml)

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
  * [Packages](http://s.sudre.free.fr/Software/Packages/about.html) for installer

To build the installer (target: `package`), you need both Qt and Packages.
To build other targets, you may not need to install them.

### Build Qt

Qt is required for GUI tools. The following is an example of build configuration.

```
# extract Qt.tar.gz to ~/myqt.
cd ~/myqt
./configure -opensource -developer-build -platform macx-clang -release
make
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

For GYP build, Ninja is also required.

  * [Ninja](https://github.com/ninja-build/ninja) for GYP build

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
