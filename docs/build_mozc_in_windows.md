How to build Mozc in Windows
============================

[![Windows](https://github.com/google/mozc/actions/workflows/windows.yaml/badge.svg)](https://github.com/google/mozc/actions/workflows/windows.yaml)

## Summary

If you are not sure what the following commands do, please check the descriptions below
and make sure the operations before running them.

```
git config --global core.autocrlf false
git config --global core.eol lf

mkdir C:\work
cd C:\work
git clone https://github.com/google/mozc.git -b master --single-branch --recursive

cd C:\work\mozc\src\third_party
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
python3 -m pip install six

"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsamd64_x86.bat"
cd C:\work\mozc\src
python build_mozc.py gyp --qtdir=C:\Qt\Qt5.15.2\msvc2019
python build_mozc.py build -c Release package
```

## Setup

### System Requirements

64-bit Windows 10 or later.

### Software Requirements

Building Mozc on Windows requires the following software.

  * [Visual Studio 2017](http://visualstudio.com/free), or any greater edition.
  * Python 3.7 or later
  * git
  * [Qt 5](https://download.qt.io/official_releases/qt/) (optional for GUI)
    + 32-bit version is required because `mozc_tool.exe` is build as a 32-bit executable.

### Setup Git configurations

The following commands change your global configuration.
If you use different configurations, you might want to restore the previous
configurations after the build.

```
git config --global core.autocrlf false
git config --global core.eol lf
```

### Download the repository from GitHub

```
mkdir C:\work
cd C:\work
git clone https://github.com/google/mozc.git -b master --single-branch --recursive
```

### Download build tools

```
cd C:\work\mozc\src\third_party
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
python3 -m pip install six
```

## Build

### Setup Build system

If you have not set up the build system in your command prompt, you might need
to execute the setup command like this.

```
"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsamd64_x86.bat"
```

### Build Mozc

The following command builds Mozc without Qt based GUI tools.

```
cd C:\work\mozc\src
python build_mozc.py gyp --noqt --msvs_version=2019
python build_mozc.py build -c Release package
```

To build Mozc with GUI tools, you need to specify the Qt directory.

```
cd C:\work\mozc\src
python build_mozc.py gyp --qtdir=C:\Qt\Qt5.15.2\msvc2019
python build_mozc.py build -c Release package
```

The directory of Qt (`C:\Qt\Qt5.12.2\msvc2019` in this example) differs based on Qt version. If you specify `--noqt` option instead of `--qtdir=<dir to Qt>`, mozc\_tool will be built as a mock version, which does nothing.

If you need debug information, you can build debug version of Mozc as follows.

```
python build_mozc.py build -c Debug package
```

### Executables

You have release build binaries in `C:\work\mozc\src\out_win\Release` and `C:\work\mozc\src\out_win\Release_x64`.

### Clean up the Tree

To clean up the tree, execute the following. This will remove executables and intermediate files like object files, generated source files, project files, etc.

```
python build_mozc.py clean
```

### Installation and Uninstallation

Although the code repository covers source files of the official Google Japanese Input installer (see `win32/custom_action` and `win32/installer`), building Windows Installer package for OSS Mozc is not supported yet. You need to manually copy Mozc binaries and run a command as follows.

Note that Mozc now supports two input method APIs called IMM32 and TSF (Text Services Framework). Although you can register Mozc for both APIs at the same time, IMM32 is not recommended on Windows 8 and later.


---

## Install Mozc (32-bit)

Following files must be placed under %ProgramFiles%\Mozc.

  * `C:\work\mozc\src\out_win\Release\mozc_broker32.exe`
  * `C:\work\mozc\src\out_win\Release\mozc_cache_service.exe`
  * `C:\work\mozc\src\out_win\Release\mozc_renderer.exe`
  * `C:\work\mozc\src\out_win\Release\mozc_server.exe`
  * `C:\work\mozc\src\out_win\Release\mozc_tool.exe` (if you specified `--noqt` option)
  * `C:\work\mozc\src\out_win\ReleaseDynamic\mozc_tool.exe` (if you didn't specify `--noqt` option)
  * `C:\work\mozc\src\out_win\ReleaseDynamic\Qt5Core.dll` (not required if you specified `--noqt` option)
  * `C:\work\mozc\src\out_win\ReleaseDynamic\Qt5Gui.dll` (not required if you specified `--noqt` option)
  * `C:\work\mozc\src\out_win\ReleaseDynamic\Qt5Widgets.dll` (not required if you specified `--noqt` option)
  * `C:\work\mozc\src\out_win\ReleaseDynamic\platforms\qwindows.dll` (not required if you specified `--noqt` option)

`Qt5Core.dll`, `Qt5Gui.dll`, `Qt5Widgets.dll`, and `qwindows.dll` are not required if you specified `--noqt` option into the gyp command.

### Register Mozc for IMM32 into 32-bit environment

Following files must be placed under `%windir%\System32`.

  * `C:\work\mozc\src\out_win\Release\mozc_ja.ime`

Finally, you must run `mozc_broker32.exe` with administrator privilege to register IME module as follows.

```
"%ProgramFiles%\Mozc\mozc_broker32.exe" --mode=register_ime
```

### Register Mozc for TSF into 32-bit environment

Following file must be placed under `%ProgramFiles%\Mozc`.

  * `C:\work\mozc\src\out_win\Release\mozc_ja_tip32.dll`

Finally, you must run `regsvr32` with administrator privilege to register IME module as follows.

```
regsvr32 "%ProgramFiles%\Mozc\mozc_ja_tip32.dll"
```

---

## Uninstall Mozc (32-bit)

### Unregister Mozc for IMM32 from 32-bit environment

Run `mozc_broker32.exe` with administrator privilege to unregister IME module as follows.

```
"%ProgramFiles%\Mozc\mozc_broker32.exe" --mode=unregister_ime
```

Then delete the following file.

  * `%windir%\System32\mozc_ja.ime`

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

  * `C:\work\mozc\src\out_win\Release\mozc_broker32.exe`
  * `C:\work\mozc\src\out_win\Release\mozc_cache_service.exe`
  * `C:\work\mozc\src\out_win\Release\mozc_renderer.exe`
  * `C:\work\mozc\src\out_win\Release\mozc_server.exe`
  * `C:\work\mozc\src\out_win\Release\mozc_tool.exe` (if you specified `--noqt` option)
  * `C:\work\mozc\src\out_win\ReleaseDynamic\mozc\_tool.exe` (if you didn't specify `--noqt` option)
  * `C:\work\mozc\src\out_win\ReleaseDynamic\Qt5Core.dll` (not required if you specified `--noqt` option)
  * `C:\work\mozc\src\out_win\ReleaseDynamic\Qt5Gui.dll` (not required if you specified `--noqt` option)
  * `C:\work\mozc\src\out_win\ReleaseDynamic\Qt5Widgets.dll` (not required if you specified `--noqt` option)
  * `C:\work\mozc\src\out_win\ReleaseDynamic\platforms\qwindows.dll` (not required if you specified `--noqt` option)
  * `C:\work\mozc\src\out_win\Release_x64\mozc_broker64.exe`

`Qt5Core.dll`, `Qt5Gui.dll`, `Qt5Widgets.dll`, and `qwindows.dll` are not required if you specified `--noqt` option into the gyp command.

### Register Mozc for IMM32 into 64-bit environment

Following files must be placed under `%windir%\System32`.

  * `C:\work\mozc\src\out_win\Release_x64\mozc_ja.ime`

Following files must be placed under `%windir%\SysWOW64`.

  * `C:\work\mozc\src\out_win\Release\mozc_ja.ime`

Finally, you must run `mozc_broker64.exe` with administrator privilege to register IME module as follows.

```
"%ProgramFiles(x86)%\Mozc\mozc_broker64.exe" --mode=register_ime
```

### Register Mozc for TSF into 64-bit environment

Following file must be placed under `%ProgramFiles(x86)%\Mozc`.

  * `C:\work\mozc\src\out_win\Release\mozc_ja_tip32.dll`
  * `C:\work\mozc\src\out_win\Release_x64\mozc_ja_tip64.dll`

Finally, you must run `regsvr32` with administrator privilege to register IME module as follows.

```
regsvr32 "%ProgramFiles(x86)%\Mozc\mozc_ja_tip32.dll"
regsvr32 "%ProgramFiles(x86)%\Mozc\mozc_ja_tip64.dll"
```

---

## Uninstall Mozc (64-bit)

### Unregister Mozc for IMM32 from 64-bit environment

Run `mozc_broker64.exe` with administrator privilege to unregister IME module as follows.

```
"%ProgramFiles(x86)%\Mozc\mozc_broker64.exe" --mode=unregister_ime
```

Then delete the following files.

  * `%windir%\System32\mozc_ja.ime`
  * `%windir%\SysWOW64\mozc_ja.ime`

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
cd C:\work\mozc\src
python build_mozc.py gyp --noqt
python build_mozc.py runtests -c Release
```

Note that you can specify `--qtdir=` option instead of `--noqt` in GYP phase since currently there is no unit test that depends on Qt.
