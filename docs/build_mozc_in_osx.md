How to build Mozc on OS X
=========================

## Get the Code

You can download Mozc source code as follows:

```
mkdir -p ~/work
cd ~/work
git clone https://github.com/google/mozc.git -b master --single-branch --recursive
```

## System Requirements

Only 64-bit macOS 10.9 and later versions are supported.

## Software Requirements

Building on Mac requires the following software.

  * Xcode
  * [Ninja](https://github.com/ninja-build/ninja)
  * [Qt 5](https://download.qt.io/official_releases/qt/) for GUI
  * [Packages](http://s.sudre.free.fr/Software/Packages/about.html) for installer

If you don't need to run gui tools like about dialog, config dialog, or dictionary tool, you can omit installing Qt.  Candidate window still shows without Qt.  See below for the detailed information.

### Path to Python3

You might need to use the preinstalled Python3 (i.e. /usr/bin/python3).
Make sure the path to python3.

```
% whereis python3
/usr/bin/python3
```

### Build Qt

Qt is required for GUI tools. The following is an example of build configuration.

```
# extract Qt.tar.gz to ~/myqt.
cd ~/myqt
./configure -opensource -developer-build -platform macx-clang -release
make
```


-----

## Build with GYP (stable)

### Apply a patch to GYP

The upstream GYP may or may not work on macOS for Mozc.
You probably need to apply the following patch to GYP.

```
cd src\third_party\gyp
git apply ..\..\gyp\gyp.patch
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

-----

## Bazel build (experimental)

Bazel build is experimental. Built binaries may have some problems.
This is only for development and testing at this moment.

### Software Requirements

Building on Mac with Bazel requires the following software.

  * Xcode
  * [Bazel](https://docs.bazel.build/versions/master/install-os-x.html)
  * [Qt 5](https://download.qt.io/official_releases/qt/)
  * [Packages](http://s.sudre.free.fr/Software/Packages/about.html)


### Edit src/config.bzl

Modify varibles in `src/config.bzl` to fit your environment.
Note: `~` does not represent the home directry.
The exact path should be specified (e.g. `QT_BASE_PATH = "/Users/mozc/myqt"`).

### Build installer

```
cd ~/work/mozc/src
bazel build package --config macos -c opt
open bazel-bin/mac/Mozc.pkg
```
