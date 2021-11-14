# How to build Mozc in Docker

## Introduction
Docker containers are available to build Mozc binaries for Android JNI library and Linux desktop.

## System Requirements
Currently, only Ubuntu 20.04 is tested to host the Docker container to build Mozc.

## Set up Ubuntu 20.04 Docker container

```
curl -O https://raw.githubusercontent.com/google/mozc/master/docker/ubuntu20.04/Dockerfile
sudo docker build --rm -t $USER/mozc_ubuntu20.04 .
sudo docker run --interactive --tty --rm $USER/mozc_ubuntu20.04
```

### Hint
Don't forget to rebuild Docker container when Dockerfile is updated.

-----

## Build Mozc for Linux Desktop

```
bazel build package --config oss_linux -c opt
```

`package` is an alias to build:
* //server:mozc_server
* //gui/tool:mozc_tool
* //renderer:mozc_renderer
* //unix/ibus:ibus_mozc
* //unix/icons
* //unix/emacs:mozc_emacs_helper

### Install paths

| build path   | install path |
| ------------ | ------------ |
| bazel-bin/server/mozc_server           | /usr/lib/mozc/mozc_server |
| bazel-bin/gui/tool/mozc_tool           | /usr/lib/mozc/mozc_tool |
| bazel-bin/renderer/mozc_renderer       | /usr/lib/mozc_renderer |
| bazel-bin/unix/ibus/ibus_mozc          | /usr/lib/ibus-mozc/ibus-engine-mozc |
| bazel-bin/unix/emacs/mozc_emacs_helper | /usr/bin/mozc_emacs_helper |

### Unittests

```
bazel test ... --config oss_linux -c dbg -- -net/... -/third_party/...
```

* `...` means all targets under the current and sub directories.
* `--` means the end of the flags which start from `-`.
* `-<dir>/...` means exclusion of all targets under the `dir`.
  + `net` and `third_party` are not supported yet.

Here is a sampe command to run a specific test.

```
bazel test base:util_test --config oss_linux -c dbg
```

* `util_test` is defined in `base/BUILD.bazel`.

The `--test_arg=--logtostderr --test_output=all` flags shows the output of
unitests to stderr.

```
bazel test base:util_test --config oss_linux --test_arg=--logtostderr --test_output=all
```

### Build Mozc on other Linux environment

To build Mozc on other Linux environment rather than the supported Docker
environment, you might need to modify the following files.

* src/WORKSPACE - build dependencies.
* src/BUILD.ibus - build rules and include headers and libraries for IBus.
* src/BUILD.qt - build rules and include headers and libraries  for Qt.
* src/config.bzl - configuration of install paths, etc.
* src/.bazelrc - compiler flags, etc.


## Build Mozc library for Android:

Client code for Android apk is deprecated.
(the last revision with Android client code is
[afb03dd](https://github.com/google/mozc/commit/afb03ddfe72dde4cf2409863a3bfea160f7a66d8)).

The conversion engine for Android is built with Bazel.

```
bazel build package --config oss_android
```

`package` is an alias to build `android/jni:mozc_lib`.

-----

## Build Mozc for Linux Desktop with GYP (maintenance mode):

```
python3 build_mozc.py gyp
python3 build_mozc.py build -c Release package
```

`package` is an alias to build:
* //server:mozc_server
* //gui/tool:mozc_tool
* //renderer:mozc_renderer
* //unix/ibus:ibus_mozc


You can also run unittests as follows.

```
python3 build_mozc.py runtests -c Debug
```

### Differences between Bazel build and GYP build

GYP build is under maintenance mode. While the existing targets are supported
by both GYP and Bazel as much as possible, new targets will be supported by
Bazel only.

Targets only for Bazel:
* AUX dictionary (//data/dictionary_oss:aux_dictionary)
* Zip code conversion (//server:mozc_server)
* Qt-based candidate window (//renderer:mozc_renderer)
* Build rules for icons (//unix/icons)


## GYP Build configurations
In `python3 build_mozc.py gyp` step, there are two different styles to customize configurations.  One is `GYP_DEFINES` environment variable and the other is commandline option.

```
[GYP_DEFINES="..."] python3 build_mozc.py gyp [options]
```

### GYP_DEFINES
You can specify `GYP_DEFINES` environment variable to change variables in GYP files, which you can find many directories in Mozc's source tree.  [common.gypi](../src/gyp/common.gypi) is an example.
Here are examples of GYP variables that you may want to change for Linux desktop build.

  * `document_dir`: Directory path where Mozc's license file is placed
  * `ibus_mozc_path`: ibus-mozc executable path
  * `ibus_mozc_icon_path`: ibus-mozc icon path

Note that you can specify multiple GYP variables as follows.

```
GYP_DEFINES="ibus_mozc_path=/usr/lib/ibus-mozc/ibus-engine-mozc ibus_mozc_icon_path=/usr/share/ibus-mozc/product_icon.png document_dir=/usr/share/doc/mozc" python3 build_mozc.py gyp
```

### command line options
You can find many command line options as follows.
```
python3 build_mozc.py gyp --help
```
Here we show some notable options.

#### --noqt
You can use `--noqt` option to build Mozc without depending on Qt 5 library.

#### --server_dir
You can use `--server_dir` option to specify the directory name where `mozc_server` will be installed.

### Compile options
In `build_mozc.py build` step, you can specify build types (`Release` or `Debug`) and one or more build targets.  Please find each GYP file to see what build targets are defined.

```
python3 build_mozc.py build -c {Release, Debug} [gyp_path_1.gyp:gyp_target_name1] [gyp_path_2.gyp:gyp_target_name2]
```
