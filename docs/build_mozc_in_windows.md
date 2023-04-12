How to build Mozc in Windows
============================

[![Windows](https://github.com/google/mozc/actions/workflows/windows.yaml/badge.svg)](https://github.com/google/mozc/actions/workflows/windows.yaml)

## Summary

If you are not sure what the following commands do, please check the descriptions below
and make sure the operations before running them.

```
if not exist "%USERPROFILE%\source\repos" mkdir "%USERPROFILE%\source\repos"
cd "%USERPROFILE%\source\repos"
git clone https://github.com/google/mozc.git -b master --single-branch --recursive
cd mozc\src

python3 -m pip install six

"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsamd64_x86.bat"

python build_mozc.py gyp --qtdir=C:\Qt\Qt5.15.2\msvc2019
python build_mozc.py build -c Release package
```

## Setup

### System Requirements

64-bit Windows 10 or later.

### Software Requirements

Building Mozc on Windows requires the following software.

  * [Visual Studio 2022 Community Edition](https://visualstudio.microsoft.com/downloads/#visual-studio-community-2022)
    * [Build Tools for Visual Studio 2022](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022) should also work
  * Python 3.7 or later
  * git
  * (**optional**) [Qt 5](https://download.qt.io/official_releases/qt/)
    + 32-bit version is required because `mozc_tool.exe` is build as a 32-bit executable.

### Download the repository from GitHub

```
if not exist "%USERPROFILE%\source\repos" mkdir "%USERPROFILE%\source\repos"
cd "%USERPROFILE%\source\repos"
git clone https://github.com/google/mozc.git -b master --single-branch --recursive
cd mozc\src
```

Hereafter you can do all the operations without changing directory.

### Download build tools

```
python3 -m pip install six
```

## Build

### Setup Build system

If you have not set up the build system in your command prompt, you might need
to execute the setup command like this.

```
"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsamd64_x86.bat"
```

### Build Mozc

The following command builds Mozc without Qt based GUI tools.

```
python build_mozc.py gyp --noqt
python build_mozc.py build -c Release package
```

To build Mozc with GUI tools, you need to specify the Qt directory.

```
python build_mozc.py gyp --qtdir=C:\Qt\Qt5.15.2\msvc2019
python build_mozc.py build -c Release package
```

The directory of Qt (`C:\Qt\Qt5.12.2\msvc2019` in this example) differs
based on Qt version. If you specify `--noqt` option
instead of `--qtdir=<dir to Qt>`, mozc\_tool will be built as a mock version,
which does nothing.

To use other versions of Visual Studio, e.g. Visual Studio 2019, try `--msvs_version` option as follows.

```
python build_mozc.py gyp --qtdir=C:\Qt\Qt5.15.2\msvc2019 --msvs_version=2019
```

If you need debug information, you can build debug version of Mozc as follows.

```
python build_mozc.py build -c Debug package
```

### Executables

You have release build binaries in `out_win\Release` and `out_win\Release_x64`.

### Clean up the Tree

To clean up the tree, execute the following. This will remove executables and intermediate files like object files, generated source files, project files, etc.

```
python build_mozc.py clean
```

### Installation and Uninstallation

Although the code repository covers source files of the official Google Japanese Input installer (see `win32/custom_action` and `win32/installer`), building Windows Installer package for OSS Mozc is not supported yet. You need to manually copy Mozc binaries and run a command as follows.

---

## Install Mozc (32-bit)

Following files must be placed under %ProgramFiles%\Mozc.

  * `out_win\Release\mozc_broker.exe`
  * `out_win\Release\mozc_cache_service.exe`
  * `out_win\Release\mozc_renderer.exe`
  * `out_win\Release\mozc_server.exe`
  * `out_win\Release\mozc_tool.exe` (if you specified `--noqt` option)
  * `out_win\ReleaseDynamic\mozc_tool.exe` (if you didn't specify `--noqt` option)
  * `out_win\ReleaseDynamic\Qt5Core.dll` (not required if you specified `--noqt` option)
  * `out_win\ReleaseDynamic\Qt5Gui.dll` (not required if you specified `--noqt` option)
  * `out_win\ReleaseDynamic\Qt5Widgets.dll` (not required if you specified `--noqt` option)
  * `out_win\ReleaseDynamic\platforms\qwindows.dll` (not required if you specified `--noqt` option)

`Qt5Core.dll`, `Qt5Gui.dll`, `Qt5Widgets.dll`, and `qwindows.dll` are not required if you specified `--noqt` option into the gyp command.

### Register Mozc for TSF into 32-bit environment

Following file must be placed under `%ProgramFiles%\Mozc`.

  * `out_win\Release\mozc_ja_tip32.dll`

Finally, you must run `regsvr32` with administrator privilege to register IME module as follows.

```
regsvr32 "%ProgramFiles%\Mozc\mozc_ja_tip32.dll"
```

---

## Uninstall Mozc (32-bit)

### Unregister Mozc for TSF from 32-bit environment

Run `regsvr32` with administrator privilege to unregister IME module as follows.

```
regsvr32 /u "%ProgramFiles%\Mozc\mozc_ja_tip32.dll"
```

### Uninstall common files of Mozc from 32-bit environment

Delete following directory and files after unregistering Mozc from IMM32/TSF.

  * `%ProgramFiles%\Mozc\`

---

## Install Mozc (64-bit)

Following files must be placed under %ProgramFiles(x86)%\Mozc.

  * `out_win\Release\mozc_cache_service.exe`
  * `out_win\Release\mozc_renderer.exe`
  * `out_win\Release\mozc_server.exe`
  * `out_win\Release\mozc_tool.exe` (if you specified `--noqt` option)
  * `out_win\ReleaseDynamic\mozc\_tool.exe` (if you didn't specify `--noqt` option)
  * `out_win\ReleaseDynamic\Qt5Core.dll` (not required if you specified `--noqt` option)
  * `out_win\ReleaseDynamic\Qt5Gui.dll` (not required if you specified `--noqt` option)
  * `out_win\ReleaseDynamic\Qt5Widgets.dll` (not required if you specified `--noqt` option)
  * `out_win\ReleaseDynamic\platforms\qwindows.dll` (not required if you specified `--noqt` option)
  * `out_win\Release_x64\mozc_broker.exe`

`Qt5Core.dll`, `Qt5Gui.dll`, `Qt5Widgets.dll`, and `qwindows.dll` are not required if you specified `--noqt` option into the gyp command.

### Register Mozc for TSF into 64-bit environment

Following file must be placed under `%ProgramFiles(x86)%\Mozc`.

  * `out_win\Release\mozc_ja_tip32.dll`
  * `out_win\Release_x64\mozc_ja_tip64.dll`

Finally, you must run `regsvr32` with administrator privilege to register IME module as follows.

```
regsvr32 "%ProgramFiles(x86)%\Mozc\mozc_ja_tip32.dll"
regsvr32 "%ProgramFiles(x86)%\Mozc\mozc_ja_tip64.dll"
```

---

## Uninstall Mozc (64-bit)

### Unregister Mozc for TSF from 64-bit environment

Run `regsvr32` with administrator privilege to unregister IME module as follows.

```
regsvr32 /u "%ProgramFiles(x86)%\Mozc\mozc_ja_tip32.dll"
regsvr32 /u "%ProgramFiles(x86)%\Mozc\mozc_ja_tip64.dll"
```

### Uninstall common files of Mozc from 64-bit environment

Delete following directory and files after unregistering Mozc from IMM32/TSF.

  * `%ProgramFiles(x86)%\Mozc\`

---


## Run unit tests

You can run unit tests as follows.

```
python build_mozc.py gyp --noqt
python build_mozc.py runtests -c Release
```

Note that you can specify `--qtdir=` option instead of `--noqt` in GYP phase since currently there is no unit test that depends on Qt.
