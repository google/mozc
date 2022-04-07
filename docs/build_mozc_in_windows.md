How to build Mozc in Windows
============================

# System Requirements

64-bit Windows is required to build Mozc for Windows. Mozc itself is expected to work on Windows 7 and later, including 32-bit Windows.

# Software Requirements

Building Mozc on Windows requires the following software.

  * [Visual Studio 2015 Community Edition](http://visualstudio.com/free), or any greater edition.
  * (optinal) [Qt 5](https://download.qt.io/official_releases/qt/)
    * Commercial version and LGPL version are available.
    * You must download msvs2015 32-bit version of Qt 5 since currently `mozc_tool.exe` needs to be built as a 32-bit executable.

# Get dependent prebuilt binaries

If you do not have `git`, `python3`, and `ninja` in your build environment, you can use prebuilt binaries in [depot\_tools](https://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools_tutorial.html).  You need to manually unzip `depot_tools.zip` and add the extracted directory into your `PATH`.

```
set PATH=%PATH%;c:\work\depot_tools
```

Then run `gclient` command twice so that dependent libraries can be installed automatically.

```
gclient
gclient
```

# Install python3 dependencies

```
python3 -m pip install six
```

# Get the Code

You can download Mozc source code as follows:

```
mkdir c:\work
cd c:\work
git clone -c core.autocrlf=false https://github.com/google/mozc.git -b master --single-branch --recursive
```

# Compilation

First, you'll need to generate Visual C++ project files using a tool called [GYP](https://chromium.googlesource.com/external/gyp).

```
cd c:\work\mozc\src
python build_mozc.py gyp --qtdir=c:\Qt\Qt5.6.2\5.6\msvc2015
```

The directory of Qt (`c:\Qt\Qt5.6.2\5.6\msvc2015` in this example) differs based on Qt version. If you specify `--noqt` option instead of `--qtdir=<dir to Qt>`, mozc\_tool will be built as a mock version, which does nothing.

You can also specify `--branding=GoogleJapaneseInput` option and `--wix_dir=<dir to WiX binaries>` option here to reproduce official Google Japanese Input binaries and installers.

Then, build Mozc binaries:

```
python build_mozc.py build -c Release package
```

If you need debug information, you can build debug version of Mozc as follows.

```
python build_mozc.py build -c Debug package
```

# Executables

You have release build binaries in `c:\work\mozc\src\out\Release` and  `c:\work\mozc\src\out\Release_x64`.

# Clean up the Tree

To clean up the tree, execute the following. This will remove executables and intermediate files like object files, generated source files, project files, etc.

```
python build_mozc.py clean
```

# Installation and Uninstallation

Although the code repository covers source files of the official Google Japanese Input installer (see `win32/custom_action` and `win32/installer`), building Windows Installer package for OSS Mozc is not supported yet. You need to manually copy Mozc binaries and run a command as follows.

Note that Mozc now supports two input method APIs called IMM32 and TSF (Text Services Framework). Although you can register Mozc for both APIs at the same time, IMM32 is not recommended on Windows 8 and later.


---

## Install Mozc (32-bit)

Following files must be placed under %ProgramFiles%\Mozc.

  * `C:\work\mozc\src\out\Release\mozc_broker32.exe`
  * `C:\work\mozc\src\out\Release\mozc_cache_service.exe`
  * `C:\work\mozc\src\out\Release\mozc_renderer.exe`
  * `C:\work\mozc\src\out\Release\mozc_server.exe`
  * `C:\work\mozc\src\out\Release\mozc_tool.exe` (if you specified `--noqt` option)
  * `C:\work\mozc\src\out\ReleaseDynamic\mozc_tool.exe` (if you didn't specify `--noqt` option)
  * `C:\work\mozc\src\out\ReleaseDynamic\Qt5Core.dll` (not required if you specified `--noqt` option)
  * `C:\work\mozc\src\out\ReleaseDynamic\Qt5Gui.dll` (not required if you specified `--noqt` option)
  * `C:\work\mozc\src\out\ReleaseDynamic\Qt5Widgets.dll` (not required if you specified `--noqt` option)
  * `C:\work\mozc\src\out\ReleaseDynamic\platforms\qwindows.dll` (not required if you specified `--noqt` option)

`Qt5Core.dll`, `Qt5Gui.dll`, `Qt5Widgets.dll`, and `qwindows.dll` are not required if you specified `--noqt` option into the gyp command.

### Register Mozc for IMM32 into 32-bit environment

Following files must be placed under `%windir%\System32`.

  * `C:\work\mozc\src\out\Release\mozc_ja.ime`

Finally, you must run `mozc_broker32.exe` with administrator privilege to register IME module as follows.

```
"%ProgramFiles%\Mozc\mozc_broker32.exe" --mode=register_ime
```

### Register Mozc for TSF into 32-bit environment

Following file must be placed under `%ProgramFiles%\Mozc`.

  * `C:\work\mozc\src\out\Release\mozc_ja_tip32.dll`

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

  * `C:\work\mozc\src\out\Release\mozc_broker32.exe`
  * `C:\work\mozc\src\out\Release\mozc_cache_service.exe`
  * `C:\work\mozc\src\out\Release\mozc_renderer.exe`
  * `C:\work\mozc\src\out\Release\mozc_server.exe`
  * `C:\work\mozc\src\out\Release\mozc_tool.exe` (if you specified `--noqt` option)
  * `C:\work\mozc\src\out\ReleaseDynamic\mozc\_tool.exe` (if you didn't specify `--noqt` option)
  * `C:\work\mozc\src\out\ReleaseDynamic\Qt5Core.dll` (not required if you specified `--noqt` option)
  * `C:\work\mozc\src\out\ReleaseDynamic\Qt5Gui.dll` (not required if you specified `--noqt` option)
  * `C:\work\mozc\src\out\ReleaseDynamic\Qt5Widgets.dll` (not required if you specified `--noqt` option)
  * `C:\work\mozc\src\out\ReleaseDynamic\platforms\qwindows.dll` (not required if you specified `--noqt` option)
  * `C:\work\mozc\src\out\Release_x64\mozc_broker64.exe`

`Qt5Core.dll`, `Qt5Gui.dll`, `Qt5Widgets.dll`, and `qwindows.dll` are not required if you specified `--noqt` option into the gyp command.

### Register Mozc for IMM32 into 64-bit environment

Following files must be placed under `%windir%\System32`.

  * `C:\work\mozc\src\out\Release_x64\mozc_ja.ime`

Following files must be placed under `%windir%\SysWOW64`.

  * `C:\work\mozc\src\out\Release\mozc_ja.ime`

Finally, you must run `mozc_broker64.exe` with administrator privilege to register IME module as follows.

```
"%ProgramFiles(x86)%\Mozc\mozc_broker64.exe" --mode=register_ime
```

### Register Mozc for TSF into 64-bit environment

Following file must be placed under `%ProgramFiles(x86)%\Mozc`.

  * `C:\work\mozc\src\out\Release\mozc_ja_tip32.dll`
  * `C:\work\mozc\src\out\Release_x64\mozc_ja_tip64.dll`

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


# Run unit tests

You can run unit tests as follows.

```
cd c:\work\mozc\src
python build_mozc.py gyp --noqt
python build_mozc.py runtests -c Release
```

Note that you can specify `--qtdir=` option instead of `--noqt` in GYP phase since currently there is no unit test that depends on Qt.
