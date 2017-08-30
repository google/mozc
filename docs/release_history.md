Release History
===============

2.19.2644.102 - 2.20.2673.102 / *2016-10-20* - *2016-12-08*
--------------------------------------------------
You can check out Mozc [2.20.2673.102](https://github.com/google/mozc/commit/280e38fe3d9db4df52f0713acf2ca65898cd697a) as follows.

```
git clone https://github.com/google/mozc.git -b master --single-branch
cd mozc
git checkout 280e38fe3d9db4df52f0713acf2ca65898cd697a
git submodule update --init --recursive
```

Summary of changes between [2.19.2644.102](https://github.com/google/mozc/commit/c19cc87a399bae0863b76daece950b8bb1be8175) and [2.20.2673.102](https://github.com/google/mozc/commit/280e38fe3d9db4df52f0713acf2ca65898cd697a) as follows.

  * Third party libraries:
    * None.
  * Build related changes:
    * `--qtver` GYP build option was removed ([280e38f](https://github.com/google/mozc/commit/280e38fe3d9db4df52f0713acf2ca65898cd697a)).
    * Mozc for macOS now uses macOS 10.11 SDK by default ([b2a74bb](https://github.com/google/mozc/commit/b2a74bb1bab6cc3de8611b7679b4c79f45d8ddb3)).
  * Major changes:
    * `src/data/installer/credits_ja.html` was removed ([2ec6c8f](https://github.com/google/mozc/commit/2ec6c8f9dc1478f4d47739ee2d017d02b954b75a)).
    * Mozc for macOS now generates 64-bit executables.  32-bit machine is no longer supported on macOS.
    * Mozc for Android now has more translations ([d914458](https://github.com/google/mozc/commit/d91445866b2b574a35ce1f79894403e26f4b5ae2)).
  * Fixed issues:
    * [#187](https://github.com/google/mozc/issues/187): build_mozc.py always generates 32 bit binaries on 64 bit OSX
    * [#327](https://github.com/google/mozc/issues/327): Switch to Qt5 from Qt4
    * [#348](https://github.com/google/mozc/issues/348): DirectWrite may fail to render text in certain enviromnents
    * [#391](https://github.com/google/mozc/issues/391): ImportError: gen_zip_code_seed.py
    * [#399](https://github.com/google/mozc/issues/399): OK/Cancel buttons on Mozc key binding editor dialog cannot be clicked on Windows
    * [#400](https://github.com/google/mozc/issues/400): Close icon on GUI dialogs do not work on Windows
  * Total commits:
    * [30 commits](https://github.com/google/mozc/compare/c19cc87a399bae0863b76daece950b8bb1be8175%5E...280e38fe3d9db4df52f0713acf2ca65898cd697a).


2.18.2613.102 - 2.19.2643.102 / *2016-09-15* - *2016-10-18*
--------------------------------------------------
You can check out Mozc [2.19.2643.102](https://github.com/google/mozc/commit/f5dcadf0dee0382612398965adc2c110ef027d9c) as follows.

```
git clone https://github.com/google/mozc.git -b master --single-branch
cd mozc
git checkout f5dcadf0dee0382612398965adc2c110ef027d9c
git submodule update --init --recursive
```

Summary of changes between [2.18.2613.102](https://github.com/google/mozc/commit/f76c304164e54414b5310010bc26e27b37764822) and [2.19.2643.102](https://github.com/google/mozc/commit/f5dcadf0dee0382612398965adc2c110ef027d9c) as follows.

  * Third party libraries:
    * protobuf: [e8ae137 -> c44ca26](https://github.com/google/protobuf/compare/e8ae137c96444ea313485ed1118c5e43b2099cf1...c44ca26fe89ed8a81d3ee475a2ccc1797141dbce)
    * Dropped dependency on [fonttools](https://github.com/googlei18n/fonttools)
  * Build related changes:
    * `--qtver=5` GYP build option is implicitly assumed on macOS and Linux builds ([f76c304](https://github.com/google/mozc/commit/f76c304164e54414b5310010bc26e27b37764822)). On Windows, `--qtver=4` is still the default.
  * Major changes:
    * Mozc for macOS now supports 10.12 as a runtime environment.
    * Mozc for Android now uses on-device font to render keytop icons ([f5dcad](https://github.com/google/mozc/commit/f5dcadf0dee0382612398965adc2c110ef027d9c)).
  * Fixed issues:
    * [#263](https://github.com/google/mozc/issues/263): Incorrect position in voiced sound marks on the key pad in Android
    * [#384](https://github.com/google/mozc/issues/384): HUAWEI P9 lite does not show MozcView.
    * [#388](https://github.com/google/mozc/issues/388): Having multiple abbreviation user dictionary entries with the same reading should be supported
    * [#389](https://github.com/google/mozc/issues/389): Emoticon user dictionary entry should not be treated a content word
  * Total commits:
    * [39 commits](https://github.com/google/mozc/compare/f76c304164e54414b5310010bc26e27b37764822%5E...f5dcadf0dee0382612398965adc2c110ef027d9c).


2.17.2532.102 - 2.18.2612.102 / *2016-03-15* - *2016-09-13*
--------------------------------------------------
You can check out Mozc [2.18.2612.102](https://github.com/google/mozc/commit/2315f957d1785130c2ed196e141a330b0857b065) as follows.

```
git clone https://github.com/google/mozc.git -b master --single-branch
cd mozc
git checkout 2315f957d1785130c2ed196e141a330b0857b065
git submodule update --init --recursive
```

Summary of changes between [2.17.2532.102](https://github.com/google/mozc/commit/09b47c3b5a6418e745a809cc393a267e8a637740) and [2.18.2612.102](https://github.com/google/mozc/commit/2315f957d1785130c2ed196e141a330b0857b065) as follows.

  * Third party libraries:
    * protobuf: [d5fb408 -> e8ae137](https://github.com/google/protobuf/compare/d5fb408ddc281ffcadeb08699e65bb694656d0bd...e8ae137c96444ea313485ed1118c5e43b2099cf1)
    * GYP: [e2e928b -> 4ec6c4e](https://chromium.googlesource.com/external/gyp/+log/e2e928bacd07fead99a18cb08d64cb24e131d3e5..4ec6c4e3a94bd04a6da2858163d40b2429b8aad1)
    * breakpad: [d2904bb -> 85b27e4](https://chromium.googlesource.com/breakpad/breakpad/+log/d2904bb42181bc32c17b26ac4a0604c0e57473cc..85b27e4a692b803dcd493ea0a9ce3828af6b82bd)
    * Dropped dependency on [zlib](https://github.com/madler/zlib)
  * Build related changes:
    * Renamed `src/mozc_version_template.txt` to `src/data/version/mozc_version_template.bzl`
    * Reference build environment now uses Ubuntu 14.04.5 ([a7cbf72](https://github.com/google/mozc/commit/a7cbf72341f5dc5d7181fecd10cb48838d64ef4c))
    * Reference build environment now uses Ninja 1.7.1 ([d2bc62b](https://github.com/google/mozc/commit/d2bc62b4de0a583355d594f66e312022e3c3deec))
    * Removed `--android_compiler` GYP option ([5ce7fa6](https://github.com/google/mozc/commit/5ce7fa6214a00c95edb2b4ce0bbd980b219b18c7))
    * Android build requires Android NDK r12b ([5ce7fa6](https://github.com/google/mozc/commit/5ce7fa6214a00c95edb2b4ce0bbd980b219b18c7))
  * Major changes:
    * Improved Store Apps compatibility on Windows ([0488082](https://github.com/google/mozc/commit/048808221cae6180c7a6ae6623e13c58490f30ed))
  * Fixed issues:
    * `NPE` in `UserDictionaryToolActivity.onPostResume` on Android ([09b47c3](https://github.com/google/mozc/commit/09b47c3b5a6418e745a809cc393a267e8a637740))
    * [#273](https://github.com/google/mozc/issues/273): Compilation errors in Android arm64 and mips64 build
    * [#373](https://github.com/google/mozc/issues/373): Unexpected size bloat of the APK
    * [#374](https://github.com/google/mozc/issues/374): Duplicate candidates after Undo
    * [#375](https://github.com/google/mozc/issues/375): `90-` is suggested from `090-`
    * [#376](https://github.com/google/mozc/issues/376): Suggestion-only user dictionary entry may not work
    * [#377](https://github.com/google/mozc/issues/377): Abbreviation user dictionary entry may not work
    * [#378](https://github.com/google/mozc/issues/378): Suppression word may not work
    * [#379](https://github.com/google/mozc/issues/379): Single character noun user dictionary entry may not work
    * [#380](https://github.com/google/mozc/issues/380): Dependency on `dictionary/pos_matcher.h` from `session/session_server.cc` is missing in GYP rules
    * [#382](https://github.com/google/mozc/issues/382): Fix typo
  * Total commits:
    * [84 commits](https://github.com/google/mozc/compare/09b47c3b5a6418e745a809cc393a267e8a637740%5E...2315f957d1785130c2ed196e141a330b0857b065).


2.17.2405.102 - 2.17.2531.102
--------------------------------------------------
You can check out Mozc [2.17.2531.102](https://github.com/google/mozc/commit/120d6a1ba6d9bad10842c6531728fc1dd8bbf731) as follows.

```
git clone https://github.com/google/mozc.git -b master --single-branch
cd mozc
git checkout 120d6a1ba6d9bad10842c6531728fc1dd8bbf731
git submodule update --init --recursive
```

Summary of changes between [2.17.2405.102](https://github.com/google/mozc/commit/b0259d5b1dd92f5c3bc4cc9e2793649424acda87) and [2.17.2531.102](https://github.com/google/mozc/commit/120d6a1ba6d9bad10842c6531728fc1dd8bbf731) as follows.

  * Third party libraries:
    * protobuf: [1a59a71 -> d5fb408](https://github.com/google/protobuf/compare/1a59a715dc5fa584340197aac0811ba3de9850b5...d5fb408ddc281ffcadeb08699e65bb694656d0bd)
  * Build related changes:
    * Building Mozc for Windows requires Visual Studio 2015 update 3.
    * `--qtdir` option is no longer supported in Linux desktop build ([d003076](https://github.com/google/mozc/commit/d00307617d5922769462ba066d891a72f8ff82ea)).
  * Major changes:
    * Updated system dictionary.
    * Removed several Shift-JIS-based normalizations on Windows ([26241b0](https://github.com/google/mozc/commit/26241b046c1f8fe55c4994d664ea10e749cff62c)).
    * Mozc for Windows requires SSE2 even on 32-bit environment.
    * Mozc for Windows supports Windows 7 SP1 and later only.
    * Mozc for macOS supports macOS 10.9 and later only.
    * Mozc for desktop platforms (Windows, macOS, desktop Linux) supports Qt5 behind `--qtver=5` GYP option.
  * Fixed issues:
    * Fix OOM when importing too large dictionary file on Android ([5c859ae](https://github.com/google/mozc/commit/5c859ae8a7b1854e78af2c81330b75bac0e9532a))
    * [#298](https://github.com/google/mozc/issues/298): Fix NPE on Samsung devices on showing toast
    * [#315](https://github.com/google/mozc/issues/315): Switch to Visual C++ 2015
    * [#372](https://github.com/google/mozc/issues/372): Discontinue the support of Windows Vista
  * Total commits:
    * [128 commits](https://github.com/google/mozc/compare/b0259d5b1dd92f5c3bc4cc9e2793649424acda87%5E...120d6a1ba6d9bad10842c6531728fc1dd8bbf731).


2.17.2355.102 - 2.17.2404.102
--------------------------------------------------
You can check out Mozc [2.17.2404.102](https://github.com/google/mozc/commit/73a8154b79b0b8db6cf8e11d6f1e750709c17518) as follows.

```
git clone https://github.com/google/mozc.git -b master --single-branch
cd mozc
git checkout 73a8154b79b0b8db6cf8e11d6f1e750709c17518
git submodule update --init --recursive
```

Summary of changes between [2.17.2355.102](https://github.com/google/mozc/commit/f1d68857831cc05a435184c375d5ab64438ed14a) and [2.17.2404.102](https://github.com/google/mozc/commit/73a8154b79b0b8db6cf8e11d6f1e750709c17518) as follows.

  * Third party libraries:
    * protobuf: [172019c -> 1a59a71](https://github.com/google/protobuf/compare/172019c40bf548908ab09bfd276074c929d48415...1a59a715dc5fa584340197aac0811ba3de9850b5)
  * Build related changes:
    * Building Mozc requires [protobuf](https://github.com/google/protobuf) 3.0 or later.
    * `--android_stl` GYP option is removed in Android build. You cannot use GNU STL to build Mozc for Android anymore.
  * Major changes:
    * None.
  * Fixed issues:
    * [#369](https://github.com/google/mozc/issues/369): Unexpected software keyboard layout can be chosen
    * [#370](https://github.com/google/mozc/issues/370): Mozc keeps crashing on Android N Developer Preview 5
    * [#371](https://github.com/google/mozc/issues/371): Shortcut word in personal dictionary should not be used for multi segment conversion
  * Total commits:
    * [50 commits](https://github.com/google/mozc/compare/f1d68857831cc05a435184c375d5ab64438ed14a%5E...73a8154b79b0b8db6cf8e11d6f1e750709c17518).


2.17.2323.102 - 2.17.2354.102
--------------------------------------------------
You can check out Mozc [2.17.2354.102](https://github.com/google/mozc/commit/02d3afb5bdfda3dde75d3c196960ee96c9ba918a) as follows.

```
git clone https://github.com/google/mozc.git -b master --single-branch
cd mozc
git checkout 02d3afb5bdfda3dde75d3c196960ee96c9ba918a
git submodule update --init --recursive
```

Summary of changes between [2.17.2323.102](https://github.com/google/mozc/commit/bf51ad0a8dbe895f4c2daf9c27b6262ecff46855) and [2.17.2354.102](https://github.com/google/mozc/commit/02d3afb5bdfda3dde75d3c196960ee96c9ba918a) as follows.

  * Third party libraries:
    * None.
  * Build related changes:
    * None.
  * Major changes:
    * [2.17.2323.102](https://github.com/google/mozc/commit/bf51ad0a8dbe895f4c2daf9c27b6262ecff46855) and later commits in OSS repository preserve the original CL commit date in Google internal repository.
      * Consider to specify `--topo-order` option to `git log` to see commits in the actual commit order.
    * Multiple performance improvements in Android.
  * Fixed issues:
    * None.
  * Total commits:
    * [32 commits](https://github.com/google/mozc/compare/bf51ad0a8dbe895f4c2daf9c27b6262ecff46855%5E...02d3afb5bdfda3dde75d3c196960ee96c9ba918a).


2.17.2314.102 - 2.17.2322.102 / *2016-01-10* - *2016-03-12*
--------------------------------------------------
You can check out Mozc [2.17.2322.102](https://github.com/google/mozc/commit/9b4c9e0e6764cca0e52b8d076165a4f1278effd4) as follows.

```
git clone https://github.com/google/mozc.git -b master --single-branch
cd mozc
git checkout 2628af6995dbbbb9ccdb52d1160db1dbd5ed3bae
git submodule update --init --recursive
```

Summary of changes between [2.17.2314.102](https://github.com/google/mozc/commit/d938fbb58c62f7bd86fce28267fbabffd1f55f66) and [2.17.2322.102](https://github.com/google/mozc/commit/9b4c9e0e6764cca0e52b8d076165a4f1278effd4) as follows.

  * Third party libraries:
    * googletest: [1d53731 -> 82b11b8](https://github.com/google/googletest/compare/1d53731f2c210557caab5660dbe2c578dce6114f...82b11b8cfcca464c2ac74b623d04e74452e74f32)
    * WTL: 9.0.4140 -> 9.1.5321
  * Build related changes:
    * Building macOS binaries now requires [Ninja](https://github.com/ninja-build/ninja) instead of `xcodebuild`.
  * Major changes:
    * None.
  * Fixed issues:
    * [#247](https://github.com/google/mozc/issues/247): Use ninja to build Mac binaries
    * [#355](https://github.com/google/mozc/issues/355): Native resource leak due to the `missing pthread_detach` call in `mozc::Thread::Detach`
    * [#361](https://github.com/google/mozc/issues/361): `ImmSetCandidateWindow()` with `CFS_EXCLUDE` isn't supported on Win Vista and Win7
  * Total commits:
    * [15 commits](https://github.com/google/mozc/compare/d938fbb58c62f7bd86fce28267fbabffd1f55f66%5E...9b4c9e0e6764cca0e52b8d076165a4f1278effd4).


2.17.2288.102 - 2.17.2313.102 / *2016-01-03* - *2016-01-10*
--------------------------------------------------
You can check out Mozc [2.17.2313.102](https://github.com/google/mozc/commit/2628af6995dbbbb9ccdb52d1160db1dbd5ed3bae) as follows.

```
git clone https://github.com/google/mozc.git -b master --single-branch
cd mozc
git checkout 2628af6995dbbbb9ccdb52d1160db1dbd5ed3bae
git submodule update --init --recursive
```

Summary of changes between [2.17.2288.102](https://github.com/google/mozc/commit/a86c7d014ac7bf36e06c5567c92ef515b3780783) and [2.17.2313.102](https://github.com/google/mozc/commit/2628af6995dbbbb9ccdb52d1160db1dbd5ed3bae) as follows.

  * Third party libraries:
    * None.
  * Build related changes:
    * None.
  * Major changes:
    * Update system dictionary.
    * Status icons for OS X are updated with Noto font.
  * Fixed issues:
    * [#344](https://github.com/google/mozc/issues/344): Support ```icon_prop_key``` entry in ibus-mozc
    * [#345](https://github.com/google/mozc/issues/345): Mozc for Android keeps crashing
    * [#347](https://github.com/google/mozc/issues/347): Software keyboard is not rendered correctly on Android 6 if non-material theme is selected
    * [#350](https://github.com/google/mozc/issues/350): Status icon is not updated when using Windows Store Apps in desktop mode on Windows 10
    * [#351](https://github.com/google/mozc/issues/351): Mozc cannot be activated in the search box on the task bar when configured to be the default IME on Windows 10
    * Following issues are not completely fixed yet, but at least worked around.
      * [#348](https://github.com/google/mozc/issues/348): DirectWrite may fail to render text in certain enviromnents
      * [#349](https://github.com/google/mozc/issues/349): Word suggestion can be unexpectedly suppressed on Chromium
  * Total commits:
    * [29 commits](https://github.com/google/mozc/compare/a86c7d014ac7bf36e06c5567c92ef515b3780783%5E...2628af6995dbbbb9ccdb52d1160db1dbd5ed3bae).


2.17.2285.102 - 2.17.2287.102 / *2016-01-01* - *2016-01-02*
--------------------------------------------------
You can check out Mozc [2.17.2287.102](https://github.com/google/mozc/commit/ab4569e73bca8d2375262d243f362c7b848646da) as follows.

```
git clone https://github.com/google/mozc.git -b master --single-branch
cd mozc
git checkout ab4569e73bca8d2375262d243f362c7b848646da
git submodule update --init --recursive
```

Summary of changes between [2.17.2285.102](https://github.com/google/mozc/commit/4a370c6ea6f003be99e7837713a939a68b75aeae) and [2.17.2287.102](https://github.com/google/mozc/commit/ab4569e73bca8d2375262d243f362c7b848646da) as follows.

  * Third party libraries:
    * None.
  * Build related changes:
    * None.
  * Major changes:
    * None.
  * Fixed issues:
    * None.
  * Total commits:
    * [4 commits](https://github.com/google/mozc/compare/4a370c6ea6f003be99e7837713a939a68b75aeae%5E...ab4569e73bca8d2375262d243f362c7b848646da).


2.17.2241.102 - 2.17.2284.102 / *2015-11-15* - *2015-12-31*
--------------------------------------------------
You can check out Mozc [2.17.2284.102](https://github.com/google/mozc/commit/be24638ab360c39995ab2c10e34ab9b269e39dac) as follows.

```
git clone https://github.com/google/mozc.git -b master --single-branch
cd mozc
git checkout be24638ab360c39995ab2c10e34ab9b269e39dac
git submodule update --init --recursive
```

Summary of changes between [2.17.2241.102](https://github.com/google/mozc/commit/a54fee0095ccc618e6aeb07822fa744f9fcb4fc1) and [2.17.2284.102](https://github.com/google/mozc/commit/be24638ab360c39995ab2c10e34ab9b269e39dac) as follows.

  * Third party libraries:
    * fontTools: [5ba7d98 -> 8724513](https://github.com/googlei18n/fonttools/compare/5ba7d98a4153fad57258fca23b0bcb238717aec3...8724513a67f954eac56eeb77ced12e27d7c02b6b)
  * Build related changes:
    * Reference Dockerfile for Fedora now uses Fedora 23 base image.
    * Default ```SDKROOT``` for OS X build is switched from ```macosx10.8``` to ```macosx10.9```.
  * Major changes:
    * ```CalculatorRewriter``` is now triggered not only by inputs end with ```=``` but also by inputs start with ```=```.  For instance, now ```=1+1``` triggers ```CalculatorRewriter```.  See the commit message of [5d423b0b](https://github.com/google/mozc/commit/5d423b0ba6989481ad2474c0eaf8c387a2bdfcc9) and its unittests as for how it works.
    * Performance improvements in LOUDS.  See commits [3591f5e7](https://github.com/google/mozc/commit/3591f5e77d8bfb0ba6f1ac839b69eb9e7aa265c9) and [cac14650](https://github.com/google/mozc/commit/cac146508d32fcce1ecfec8d038f63f588ed13c4) for details.
  * Fixed issues:
    * [#317](https://github.com/google/mozc/issues/317): session_handler_scenario_test is flaky in Linux build on Travis-CI
    * [#341](https://github.com/google/mozc/issues/341): ```1d*=``` should not trigger language-aware rewriter
  * Total commits:
    * [48 commits](https://github.com/google/mozc/compare/a54fee0095ccc618e6aeb07822fa744f9fcb4fc1%5E...be24638ab360c39995ab2c10e34ab9b269e39dac).


2.17.2124.102 - 2.17.2240.102 / *2015-09-20* - *2015-11-15*
--------------------------------------------------
You can check out Mozc [2.17.2240.102](https://github.com/google/mozc/commit/95de40fa884d693172605e7283ec82233a908b29) as follows.

```
git clone https://github.com/google/mozc.git -b master --single-branch
cd mozc
git checkout 95de40fa884d693172605e7283ec82233a908b29
git submodule update --init --recursive
```

Summary of changes between [2.17.2124.102](https://github.com/google/mozc/commit/0943e518ebff9ddd9390d0ec29509cb0096ac240) and [2.17.2240.102](https://github.com/google/mozc/commit/95de40fa884d693172605e7283ec82233a908b29) as follows.

  * Third party libraries:
    * gyp: [cdf037c1 -> e2e928ba](https://chromium.googlesource.com/external/gyp/+log/cdf037c1edc0ba3b5d25f8e3973661efe00980cc..e2e928bacd07fead99a18cb08d64cb24e131d3e5)
    * zinnia: [44dddcf9 -> 814a49b0](https://github.com/taku910/zinnia/compare/44dddcf96c0970a806d666030295706f45cbd045...814a49b031709b34d23898bce47f08dc1b554ec8)
    * zlib: [50893291](https://github.com/madler/zlib/commit/50893291621658f355bc5b4d450a8d06a563053d) was added to submodules for NaCl build.
  * Build related changes:
    * Linux-only build option ```-j```/```--jobs``` was deprecated by [b393fbdc](https://github.com/google/mozc/commit/b393fbdc346a5243ad35eb559d4468a274f2d2d2).  See its commit log on how to work around it.
    * Pepper 45 SDK is required to build Mozc for NaCl.
    * Docker directory id moved from ```mozc/src/docker/``` to ```mozc/docker/``` by [cfe9a2a5](https://github.com/google/mozc/commit/cfe9a2a5c7576a01fdbbadca43760496a9405ece).
    * Enabled continuous build for Android, NaCl, and Linux-desktop on [Travis CI](https://travis-ci.org).
    * Enabled continuous test for OS X and Linux-desktop on [Travis CI](https://travis-ci.org).
    * ```REGISTER_MODULE_INITIALIZER```, ```REGISTER_MODULE_RELOADER```, ```REGISTER_MODULE_SHUTDOWN_HANDLER```, and ```REGISTER_MODULE_FINALIZER``` were deprecated since they are known as bug-prone.  Deprecating them allows us to reduce the number of use of ```Singleton<T>```, which is also known as bug-prone.
    * [#320](https://github.com/google/mozc/pull/320): ```InitGoogle``` was renamed to ```mozc::InitMozc``` and now declared in ```base/init_mozc.h```.  If you have relied on ```InitGoogle```, then you need to 1) include ```base/init_mozc.h``` and 2) replace ```InitGoogle``` with ```mozc::InitMozc```.
  * Major changes:
    * ```DateRewriter``` is now able to handle 3-digit.  For instance, when converting ```123``` you will see additional candidates such as ```1:23``` and ```01/23```.  See the commit message of [f2cc056f](https://github.com/google/mozc/commit/f2cc056fd289bb498703a451b163eb73de217c91) and its unittests for details.
  * Known issues:
    * [#263](https://github.com/google/mozc/issues/263): Voiced sound marks on the key pad is not placed at correct position in Android
    * [#273](https://github.com/google/mozc/issues/273): Compilation errors in Android arm64 and mips64 build
    * [#317](https://github.com/google/mozc/issues/317): session_handler_scenario_test is flaky in Linux build on Travis-CI
  * Fixed issues:
    * [#27](https://github.com/google/mozc/issues/27): build fail of ```base/iconv.cc```, FreeBSD
    * [#219](https://github.com/google/mozc/issues/219): Deprecate ```base/scoped_ptr.h```
    * [#252](https://github.com/google/mozc/issues/252): Remove dependency on iconv
    * [#328](https://github.com/google/mozc/issues/328): Partial commit clears remaining composing text in some cases
    * [#331](https://github.com/google/mozc/issues/331): Predictions on mobile can be too different from conversion result
    * [#332](https://github.com/google/mozc/issues/332): Clearing user dictionary on the preference app will not take effect immediately
  * Total commits:
    * [154 commits](https://github.com/google/mozc/compare/0943e518ebff9ddd9390d0ec29509cb0096ac240%5E...95de40fa884d693172605e7283ec82233a908b29).


2.17.2112.102 - 2.17.2123.102 / *2015-09-05* - *2015-09-19*
--------------------------------------------------
You can check out Mozc [2.17.2123.102](https://github.com/google/mozc/commit/e398317a086a78c0cf0004505eb8f56586e925b2) as follows.

```
git clone https://github.com/google/mozc.git -b master --single-branch
cd mozc
git checkout e398317a086a78c0cf0004505eb8f56586e925b2
git submodule update --init --recursive
```

Summary of changes between [2.17.2112.102](https://github.com/google/mozc/commit/25ae18a0ed595e5fee4bf546f21fbde2386a3da8) and [2.17.2123.102](https://github.com/google/mozc/commit/e398317a086a78c0cf0004505eb8f56586e925b2) as follows.

  * Third party libraries:
    * breakpad: [962f1b0e (r1391) -> d2904bb4 (r1419)](https://chromium.googlesource.com/breakpad/breakpad/+log/962f1b0e60eca939232dc0d46780da4fdbbcfd85%5E..d2904bb42181bc32c17b26ac4a0604c0e57473cc/)
    * gtest: [102b5048 (r700) -> 1d53731f (r707)](https://github.com/google/googletest/compare/102b50483a4b515a94a5b1c75db468eb071cf172%5E...1d53731f2c210557caab5660dbe2c578dce6114f)
    * gmock: [61adbcc5 (r501) -> d478a1f4 (r513)](https://github.com/google/googlemock/compare/61adbcc5c6b8e0385e3e2bf4262771d20a375002%5E...d478a1f46d51ac2baa3f3b3896139897f24dc2d1)
    * zinnia: [b84ad858 (0.0.4) -> 44dddcf9 (0.0.6)](https://github.com/taku910/zinnia/compare/7bdc645d7212c51d4bba234acea9ae0c6da2bbb8...44dddcf96c0970a806d666030295706f45cbd045)
    * Repository URL changes:
      * [GoogleCode] googlemock -> [GitHub] google/googlemock
      * [GoogleCode] googletest -> [GitHub] google/googletest
      * [GoogleCode] google-breakpad -> chromium.googlesource.com/breakpad/breakpad
      * [GoogleCode] japanese-usage-dictionary -> [GitHub] hiroyuki-komatsu/japanese-usage-dictionary
      * [SourceForge] zinnia -> [GitHub] taku910/zinnia
    * `src/DEPS` was deprecated and removed.  We use `git submodule` to track and check out dependent third party source code.
    * WTL is directly imported under `src/third_party` so as not to depend on subversion.
  * Build related changes:
    * Zinnia is now built from source and linked statically by default.  To link to system-installed Zinnia, specify `GYP_DEFINES="use_libzinnia=1"`.  Note that `build_mozc.py gyp --use_zinnia` is also deprecated.
  * Major changes:
    * Windows build now supports hand-writing with Zinnia.
  * Known issues:
    * [#263](https://github.com/google/mozc/issues/263): Voiced sound marks on the key pad is not placed at correct position in Android
    * [#273](https://github.com/google/mozc/issues/273): Compilation errors in Android arm64 and mips64 build
  * Fixed issues:
    * [#299](https://github.com/google/mozc/issues/299): Stop depending on subversion repositories in DEPS file
    * [#300](https://github.com/google/mozc/pull/300): Replace gclient/DEPS with git sub-modules
  * Total commits:
    * [16 commits](https://github.com/google/mozc/compare/25ae18a0ed595e5fee4bf546f21fbde2386a3da8%5E...e398317a086a78c0cf0004505eb8f56586e925b2).


2.17.2094.102 - 2.17.2111.102 / *2015-05-10* - *2015-08-15*
--------------------------------------------------
You can check out Mozc [2.17.2111.102](https://github.com/google/mozc/commit/d7b6196aeac52dd908ca051ba65e97b389f4503a) as follows.

```
gclient sync --revision=d7b6196aeac52dd908ca051ba65e97b389f4503a
```

Summary of changes between [2.17.2094.102](https://github.com/google/mozc/commit/c57a78e2b84880718f2621b9e8e4791419bee923) and [2.17.2111.102](https://github.com/google/mozc/commit/d7b6196aeac52dd908ca051ba65e97b389f4503a).

  * DEPS changes:
    * none
  * Build related changes:
    * Android build requires NDK r10e.
    * `*.proto` files are moved to `src/protocol/` to simplify build dependency.  Downstream projects may need to update include path and/or `.gyp` file accordingly.
    * Commit hashes between 2.17.2098.102 and 2.17.2106.102 were once changed [#292](https://github.com/google/mozc/issues/292).
    * Possible build failures in releases from 2.17.2099.102 (dbe800583e5676896ce603494ef3b306f38f7b85) to 2.17.2106.102 (3648b9bf06d5d9b36bed2425640bfd18ae05b588) due to [#295](https://github.com/google/mozc/issues/295).
  * Major changes:
    * ibus-mozc no longer enables `undo-commit` unless `IBUS_CAP_SURROUNDING_TEXT` is specified (0796f5143400e2beb3d18156ae426f8ce06b0c0d).
    * ibus-mozc no longer tries to align suggestion window to the left edge of the composing text (9fbcdd5e27cf26ff16d72bd2d92f269334912ede).
  * Known issues:
    * [#263](https://github.com/google/mozc/issues/263): Voiced sound marks on the key pad is not placed at correct position in Android
    * [#273](https://github.com/google/mozc/issues/273): Compilation errors in Android arm64 and mips64 build
  * Fixed issues:
    * [#243](https://github.com/google/mozc/issues/243): ibus predict window is shown at the previous cursor position
      * [Mozilla Bug 1120851](https://bugzilla.mozilla.org/show_bug.cgi?id=1120851): Candidate window sometimes doesn't set correct position with ibus + mozc when starting composition
    * [#254](https://github.com/google/mozc/issues/254): Preedit and candidate changes buffer modification flag
    * [#291](https://github.com/google/mozc/issues/291): Fix a typo
    * [#295](https://github.com/google/mozc/issues/295): Possible build failure due to missing dependency on `commands_proto` from `key_info_util`
    * [#296](https://github.com/google/mozc/issues/296): ibus-mozc should enable undo-commit if and only if `IBUS_CAP_SURROUNDING_TEXT` is set
  * Total commits:
    * [24 commits](https://github.com/google/mozc/compare/c57a78e2b84880718f2621b9e8e4791419bee923...d7b6196aeac52dd908ca051ba65e97b389f4503a).


2.17.2073.102 - 2.17.2095.102 / *2015-04-11* - *2015-05-10*
--------------------------------------------------
You can check out Mozc [2.17.2095.102](https://github.com/google/mozc/commit/321e0656b0f2e233ab1c164bd86c58568c9e92f2) as follows.

```
gclient sync --revision=321e0656b0f2e233ab1c164bd86c58568c9e92f2
```

Summary of changes between [2.17.2073.102](https://github.com/google/mozc/commit/0556a8bd57014f05583bc001d57b4b64aac00a47) and [2.17.2095.102](https://github.com/google/mozc/commit/321e0656b0f2e233ab1c164bd86c58568c9e92f2).

  * DEPS changes:
    * GYP repository is switched from code.google.com to chromium.googlesource.com.
    * ZLib repository is switched from src.chromium.org to github.com/madler/zlib.
  * Build related changes:
    * Reference build Docker image is switched from Ubuntu 14.04.1 to Ubuntu 14.04.2.
    * Fix build breakage in Android since [2.16.2072.102](https://github.com/google/mozc/commit/20c1c08d7d4e89530e3e42db3476d682981c2b68).
    * Add Dockerfile based on Fedora 21 to build Mozc for Android, NaCl, and Linux desktop.
    * Continuous build is available for OS X and Windows.
      * OS X: [Travis CI](https://travis-ci.org/google/mozc/)
      * Windows: [AppVeyor](https://ci.appveyor.com/project/google/mozc)
  * Major changes:
    * Update system dictionary.
    * Support rule-based zero query suggestion in [2.16.2080.102](https://github.com/google/mozc/commit/988392a0c821494fee2d90090cdca4c3c98bcf83).
  * Known issues:
    * [#263](https://github.com/google/mozc/issues/263): Voiced sound marks on the key pad is not placed at correct position in Android
    * [#273](https://github.com/google/mozc/issues/273): Compilation errors in Android arm64 and mips64 build
  * Fixed issues:
    * none
  * Total commits:
    * [28 commits](https://github.com/google/mozc/compare/20c1c08d7d4e89530e3e42db3476d682981c2b68...321e0656b0f2e233ab1c164bd86c58568c9e92f2).


2.16.2038.102 - 2.16.2072.102 / *2015-01-31* - *2015-03-15*
--------------------------------------------------
You can check out Mozc [2.16.2072.102](https://github.com/google/mozc/commit/20c1c08d7d4e89530e3e42db3476d682981c2b68) as follows.

```
gclient sync --revision=20c1c08d7d4e89530e3e42db3476d682981c2b68
```

Summary of changes between [2.16.2038.102](https://github.com/google/mozc/commit/6895df10f02dafb86150da8a3cc65f051f70e054) and [2.16.2072.102](https://github.com/google/mozc/commit/20c1c08d7d4e89530e3e42db3476d682981c2b68).

  * DEPS changes:
    * none
  * Build related changes:
    * [#286](https://github.com/google/mozc/issues/286): Clang 3.4 on Ubuntu 14.04 is used when building host binaries Mozc in Android, NaCl, and Linux desktop builds.  See [#286](https://github.com/google/mozc/issues/286) about why we have switched back to Clang 3.4 from Clang 3.5 on Ubuntu 14.04.
    * Pepper 40 SDK is required to build Mozc for NaCl.
    * Android 5.1 Lollipop SDK (or higher) is required to build Mozc for Android.
  * Major changes:
    * Target API level of Android binaries are incremented to 22, that is, `Build.VERSION_CODES.LOLLIPOP_MR1` a.k.a. Android 5.1.
    * LOUDS Trie engine was rewritten for better performance and maintainability.
    * `python build_mozc.py runtests` is now supported in Windows.
  * Known issues:
    * [#263](https://github.com/google/mozc/issues/263): Voiced sound marks on the key pad is not placed at correct position in Android
    * [#273](https://github.com/google/mozc/issues/273): Compilation errors in Android arm64 and mips64 build
  * Fixed issues:
    * [#286](https://github.com/google/mozc/issues/286): FIX: Build fails if clang-3.5 package is used in Ubuntu 14.04
  * Total commits:
    * [33 commits](https://github.com/google/mozc/compare/091dc3bafa1645432dd9b8ba1ba0f77645d39c1a...20c1c08d7d4e89530e3e42db3476d682981c2b68).


2.16.2021.102 - 2.16.2037.102 / *2015-01-24* - *2015-01-25*
--------------------------------------------------
You can check out Mozc [2.16.2037.102](https://github.com/google/mozc/commit/091dc3bafa1645432dd9b8ba1ba0f77645d39c1a) as follows.

```
gclient sync --revision=091dc3bafa1645432dd9b8ba1ba0f77645d39c1a
```

Summary of changes between [2.16.2021.102](https://github.com/google/mozc/commit/f78dad8d2c16d77f20577f04c2fa95ed85c386cb) and [2.16.2037.102](https://github.com/google/mozc/commit/091dc3bafa1645432dd9b8ba1ba0f77645d39c1a).

  * DEPS changes:
    * none
  * Build related changes:
    * libc++ is used by default to build Android target binaries.
    * [#276](https://github.com/google/mozc/issues/276): Clang 3.5 is now required to build Mozc for Android, NaCl, and Linux.
    * Visual C++ 2013 is required to build Mozc for Windows.
  * Major changes:
    * [#277](https://github.com/google/mozc/issues/277): Mozc for Windows now requires Windows Vista SP2 and later. Mozc [2.16.2034.102](https://github.com/google/mozc/commit/389932c227827de7fcd17a217de96c5b5a838672) is the last version that can run on Windows XP and Windows 2003 Server.
  * Known issues:
    * [#263](https://github.com/google/mozc/issues/263): Voiced sound marks on the key pad is not placed at correct position in Android
    * [#273](https://github.com/google/mozc/issues/273): Compilation errors in Android arm64 and mips64 build
  * Fixed issues:
    * [#274](https://github.com/google/mozc/issues/274): FIX: Inconsistency between suggestion candidates and conversion candidates
    * [#275](https://github.com/google/mozc/issues/275): FIX: Learning algorithm is sometimes too aggressive when punctuation is committed
    * [#276](https://github.com/google/mozc/issues/276): FIX: Require Clang to build Linux host binaries
    * [#277](https://github.com/google/mozc/issues/277): FIX: Discontinue the support of Windows XP/2003 Server
  * Total commits:
    * [17 commits](https://github.com/google/mozc/compare/5c96a77a0454f5877153d18d8a7ca5a5ddfb964b...091dc3bafa1645432dd9b8ba1ba0f77645d39c1a).


2.16.2008.102 - 2.16.2020.102 / *2015-01-01* - *2015-01-18*
--------------------------------------------------
You can check out Mozc [2.16.2020.102](https://github.com/google/mozc/commit/5c96a77a0454f5877153d18d8a7ca5a5ddfb964b) as follows.

```
gclient sync --revision=5c96a77a0454f5877153d18d8a7ca5a5ddfb964b
```

Summary of changes between [2.16.2008.102](https://github.com/google/mozc/commit/60de3075dde2ff1903aa820a7f9110455e3091c7) and [2.16.2020.102](https://github.com/google/mozc/commit/5c96a77a0454f5877153d18d8a7ca5a5ddfb964b).

  * DEPS changes:
    * protobuf: [bba83652e1be610bdb7ee1566ad18346d98b843c -> 172019c40bf548908ab09bfd276074c929d48415](https://github.com/google/protobuf/compare/172019c40bf548908ab09bfd276074c929d48415...bba83652e1be610bdb7ee1566ad18346d98b843c) (downgrading)
  * Build related changes:
    * Ubuntu 14.04 is used as the reference build/test environment for Android, `NaCl`, and Linux.  Hereafter we will not make sure that Mozc can be built on Ubuntu 12.04.
  * Known issues:
    * [#263](https://github.com/google/mozc/issues/263): Voiced sound marks on the key pad is not placed at correct position in Android
    * [#273](https://github.com/google/mozc/issues/273): Compilation errors in Android arm64 and mips64 build
  * Fixed issues:
    * [#265](https://github.com/google/mozc/issues/265): FIX: All resources are not released in Service.onDestory
    * [#266](https://github.com/google/mozc/issues/266): FIX: Many emojis are suggested from space
    * [#267](https://github.com/google/mozc/issues/267): FIX: Noisy candidate "itsumo" due to language aware conversion
    * [#269](https://github.com/google/mozc/issues/269): FIX: BuildInDocker fails when building for Android
    * [#271](https://github.com/google/mozc/issues/271): FIX: Runtime CHECK failure on Windows: protobuf/src/google/protobuf/descriptor.cc:1018
    * [#272](https://github.com/google/mozc/issues/272): FIX: `AssertionError` in `gen_zip_code_seed.py`
  * Total commits:
    * [13 commits](https://github.com/google/mozc/compare/1ffe8c9b56798baf6cac68a6dd6d539e0ccaad82...5c96a77a0454f5877153d18d8a7ca5a5ddfb964b).


2.16.2004.102 - 2.16.2007.102 / *2014-12-22* - *2014-12-24*
--------------------------------------------------
You can check out Mozc [2.16.2007.102](https://github.com/google/mozc/commit/1ffe8c9b56798baf6cac68a6dd6d539e0ccaad82) as follows.

```
gclient sync --revision=1ffe8c9b56798baf6cac68a6dd6d539e0ccaad82
```

Summary of changes between [2.16.2004.102](https://github.com/google/mozc/commit/70aa0ddaf4a1e57daccb10797d3afee433f174f6) and [2.16.2007.102](https://github.com/google/mozc/commit/1ffe8c9b56798baf6cac68a6dd6d539e0ccaad82).

  * DEPS changes:
    * fontTools: initial import as of [5ba7d98a4153fad57258fca23b0bcb238717aec3](https://github.com/googlei18n/fonttools/compare/a8f3feacb0e197c00f3f1c236777748a4dc6cf64...5ba7d98a4153fad57258fca23b0bcb238717aec3)
  * Build related changes:
    * Android build requires Android-21 SDK
  * Major changes:
    * Enable Material Theme on Android
    * Support floating window and floating mode indicator on Android 5.0 and later when physical keyboard is attached
    * Improve accessibility support on Android
  * Known issues:
    * [#263](https://github.com/google/mozc/issues/263): Voiced sound marks on the key pad is not placed at correct position in Android
  * Total commits:
    * [4 commits](https://github.com/google/mozc/compare/fe635d73050960cdfdb31a11dc3d08f636e14d49...1ffe8c9b56798baf6cac68a6dd6d539e0ccaad82).


2.16.1918.102 - 2.16.2003.102 / *2014-11-09* - *2014-12-21*
--------------------------------------------------
You can check out Mozc [2.16.2003.102](https://github.com/google/mozc/commit/fe635d73050960cdfdb31a11dc3d08f636e14d49) as follows.

```
gclient sync --revision=fe635d73050960cdfdb31a11dc3d08f636e14d49
```

Summary of changes between [2.16.1918.102](https://github.com/google/mozc/commit/b729086960878ccca5f2229a4fc9701e84093583) and [2.16.2003.102](https://github.com/google/mozc/commit/fe635d73050960cdfdb31a11dc3d08f636e14d49).

  * DEPS changes:
    * gtest: [r692 -> r700](https://code.google.com/p/googletest/source/list?start=700&num=9)
    * gmock: [r485 -> r501](https://code.google.com/p/googlemock/source/list?start=501&num=17)
    * gyp: [r1987 -> r2012](https://code.google.com/p/gyp/source/list?start=2012&num=26)
    * protobuf: [172019c40bf548908ab09bfd276074c929d48415 -> bba83652e1be610bdb7ee1566ad18346d98b843c](https://github.com/google/protobuf/compare/172019c40bf548908ab09bfd276074c929d48415...bba83652e1be610bdb7ee1566ad18346d98b843c)
  * Build related changes:
    * Android build requires NDK r10d
    * [#259](https://github.com/google/mozc/issues/259): Android build supports arm64/mips64/x86-64
    * [#260](https://github.com/google/mozc/issues/260): Android build supports Clang 3.5 and libc++
    * Versioning scheme for Android is changed. See r439 (on Google Code) for details.
    * Build time dependency on libzinnia-dev is removed from Android and NaCl builds
  * Major changes:
    * Android 2.1 - Android 3.2 are no longer supported
    * armeabi-v7a is always enabled in arm 32-bit build for Android
    * Updated main dictionary, Emoji dictionary, emoticon dictionary, and single kanji dictionary
  * Fixed issues:
    * [#248](https://github.com/google/mozc/issues/248): FIX: IME crashes when using US International hardware keyboard
    * [#255](https://github.com/google/mozc/issues/255): FIX: ibus-mozc + XIM: preedit text is not cleared after preedit commit triggered by focus change
    * [#257](https://github.com/google/mozc/issues/257): FIX: Entering symbol view causes NPE when "Switch Access" accessibility mode is enabled
    * [#261](https://github.com/google/mozc/issues/261): FIX: An empty word can be suggested in the candidate list
  * Total commits:
    * [87 commits](https://github.com/google/mozc/compare/026d814598ba223e3becc638b01c79935ea98ee2...fe635d73050960cdfdb31a11dc3d08f636e14d49).

---

Release History of Mozc 1.X
---------------------------

##1.0.558.102 - 1.15.1917.102 / *2010-12-09* - *2014-11-03*

**TODO: Import previous release notes here.**

Summary of changes between [1.0.558.102](https://github.com/google/mozc/commit/664029b064d23e0520309ec09d89ea5013783ce6) and [1.15.1917.102](https://github.com/google/mozc/commit/026d814598ba223e3becc638b01c79935ea98ee2).
  * Total commits:
    * [161 commits](https://github.com/google/mozc/compare/cae073cc74bc31625a659eb91e95d557cb2a6428...026d814598ba223e3becc638b01c79935ea98ee2).

---

Release History of Mozc 0.X
---------------------------

##0.11.347.100 - 0.13.523.102 / *2010-05-10* - *2010-11-02*

**TODO: Import previous release notes here.**

Summary of changes between [0.11.347.100](https://github.com/google/mozc/commit/0fdb7a7b04bdbbc640058e1856b278e668a69b1e) and [0.13.523.102](https://github.com/google/mozc/commit/cae073cc74bc31625a659eb91e95d557cb2a6428).
  * Total commits:
    * [17 commits](https://github.com/google/mozc/compare/1d6e951d92680d30e1a41c16e8fa74eed4039098...cae073cc74bc31625a659eb91e95d557cb2a6428).
