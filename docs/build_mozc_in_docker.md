# How to build Mozc in Docker

[![Linux](https://github.com/google/mozc/actions/workflows/linux.yaml/badge.svg)](https://github.com/google/mozc/actions/workflows/linux.yaml)
[![Android lib](https://github.com/google/mozc/actions/workflows/android.yaml/badge.svg)](https://github.com/google/mozc/actions/workflows/android.yaml)

## Summary

If you are not sure what the following commands do, please check the descriptions below
and make sure the operations before running them.

```
curl -O https://raw.githubusercontent.com/google/mozc/master/docker/ubuntu20.04/Dockerfile
docker build --rm -tag mozc_ubuntu20.04 .
docker create --interactive --tty --name mozc_build mozc_ubuntu20.04

docker start mozc_build
docker exec mozc_build bazel build package --config oss_linux -c opt
docker cp mozc_build:/home/mozc_builder/work/mozc/src/bazel-bin/unix/mozc.zip .
```

## Introduction
Docker containers are available to build Mozc binaries for Android JNI library and Linux desktop.

## System Requirements
Currently, only Ubuntu 20.04 is tested to host the Docker container to build Mozc.

* [Dockerfile](https://github.com/google/mozc/blob/master/docker/ubuntu20.04/Dockerfile) for Ubuntu 20.04

## Build in Docker

### Set up Ubuntu 20.04 Docker container

```
curl -O https://raw.githubusercontent.com/google/mozc/master/docker/ubuntu20.04/Dockerfile
docker build --rm -tag mozc_ubuntu20.04 .
docker create --interactive --tty --name mozc_build mozc_ubuntu20.04
```

You may need to execute `docker` with `sudo` (e.g. `sudo docker build ...`).

Notes
* `mozc_ubuntu20.04` is a Docker image name (customizable).
* `mozc_build` is a Docker container name (customizable).
* Don't forget to rebuild Docker container when Dockerfile is updated.


### Build Mozc in Docker container

```
docker start mozc_build
docker exec mozc_build bazel build package --config oss_linux -c opt
docker cp mozc_build:/home/mozc_builder/work/mozc/src/bazel-bin/unix/mozc.zip .
```

`mozc.zip` contains built files.

Notes
* You might want to execute `docker stop` after build.
* `mozc_build` is the Docker container name created in the above section.

-----

## Build Mozc for Linux Desktop

```
bazel build package --config oss_linux -c opt
```

Note: You might want to execute `docker start --interactive mozc_build`
to enter the docker container before the above command.

`package` builds Mozc binaries and locates them into `mozc.zip` as follows.

| build rule                     | install path |
| ------------------------------ | ------------ |
| //server:mozc_server           | /usr/lib/mozc/mozc_server |
| //gui/tool:mozc_tool           | /usr/lib/mozc/mozc_tool |
| //renderer:mozc_renderer       | /usr/lib/mozc/mozc_renderer |
| //unix/ibus/ibus_mozc          | /usr/lib/ibus-mozc/ibus-engine-mozc |
| //unix/ibus:gen_mozc_xml       | /usr/share/ibus/component/mozc.xml |
| //unix:icons                   | /usr/share/ibus-mozc/... |
| //unix:icons                   | /usr/share/icons/mozc/... |
| //unix/emacs:mozc.el           | /usr/share/emacs/site-lisp/emacs-mozc/mozc.el |
| //unix/emacs:mozc_emacs_helper | /usr/bin/mozc_emacs_helper |

Install paths are configurable by modifying
[src/config.bzl](https://github.com/google/mozc/blob/master/src/config.bzl).


### Unittests

```
bazel test ... --config oss_linux -c dbg -- -net/... -third_party/...
```

* `...` means all targets under the current and subdirectories.
* `--` means the end of the flags which start from `-`.
* `-<dir>/...` means exclusion of all targets under the `dir`.
  + `net` and `third_party` are not supported yet.

Here is a sample command to run a specific test.

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

Note: This section is not about our officially supported build process.

To build Mozc on other Linux environment rather than the supported Docker
environment, you might need to modify the following files.

* src/config.bzl - configuration of install paths, etc.
* src/.bazelrc - compiler flags, etc.
* src/WORKSPACE.bazel - build dependencies.

Tips: the following command makes the specified file untracked by Git.
```
git update-index --assume-unchanged src/config.bzl
```

This command reverts the above change.
```
git update-index --no-assume-unchanged src/config.bzl
```

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

⚠️ The GYP build will stop supporting the IBus build.
* https://github.com/google/mozc/issues/567

⚠️ The GYP build no longer support the GTK candidate window build.
* https://github.com/google/mozc/issues/567

To keep using the GYP build without GTK candidate window at this moment,
please add the `--use_gyp_for_ibus_build` and `--no_gtk_build` flags
to build_mozc.py.

```
python3 build_mozc.py gyp
python3 build_mozc.py build -c Release package --use_gyp_for_ibus_build --no_gtk_build
```

`package` is an alias to build:
* //server:mozc_server
* //gui/tool:mozc_tool
* //unix/ibus:ibus_mozc


You can also run unittests as follows.

```
python3 build_mozc.py runtests -c Debug
```

### Differences between Bazel build and GYP build

GYP build is under maintenance mode. New features might be supported by
Bazel only, and some features might be dropped as a trade-off to accept PRs.

Targets only for Bazel:
* AUX dictionary (//data/dictionary_oss:aux_dictionary)
* Filtered dictionary (//data/dictionary_oss:filtered_dictionary)
* SVS character input instead of CJK compatibility ideographs (//rewriter:single_kanji_rewriter)
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
