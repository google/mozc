# How to build Mozc in Docker

[![Linux](https://github.com/google/mozc/actions/workflows/linux.yaml/badge.svg)](https://github.com/google/mozc/actions/workflows/linux.yaml)
[![Android lib](https://github.com/google/mozc/actions/workflows/android.yaml/badge.svg)](https://github.com/google/mozc/actions/workflows/android.yaml)

## Summary

If you are not sure what the following commands do, please check the descriptions below
and make sure the operations before running them.

```
curl -O https://raw.githubusercontent.com/google/mozc/master/docker/ubuntu22.04/Dockerfile
docker build --rm --tag mozc_ubuntu22.04 .
docker create --interactive --tty --name mozc_build mozc_ubuntu22.04

docker start mozc_build
docker exec mozc_build bazel build package --config oss_linux -c opt
docker cp mozc_build:/home/mozc_builder/work/mozc/src/bazel-bin/unix/mozc.zip .
```

## Introduction
Docker containers are available to build Mozc binaries for Android JNI library and Linux desktop.

## System Requirements
Currently, only Ubuntu 22.04 is tested to host the Docker container to build Mozc.

* [Dockerfile](https://github.com/google/mozc/blob/master/docker/ubuntu22.04/Dockerfile) for Ubuntu 22.04

## Build in Docker

### Set up Ubuntu 22.04 Docker container

```
curl -O https://raw.githubusercontent.com/google/mozc/master/docker/ubuntu22.04/Dockerfile
docker build --rm --tag mozc_ubuntu22.04 .
docker create --interactive --tty --name mozc_build mozc_ubuntu22.04
```

You may need to execute `docker` with `sudo` (e.g. `sudo docker build ...`).

Notes
* `mozc_ubuntu22.04` is a Docker image name (customizable).
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


### Unit tests

#### Run all tests

```
bazel test ... --config oss_linux --build_tests_only -c dbg
```

* `...` means all targets under the current and subdirectories.


### Run tests under the specific directories

```
bazel test base/... composer/... --config oss_linux --build_tests_only -c dbg
```

* `<dir>/...` means all targets under the `<dir>/` directory.


### Run tests without the specific directories

```
bazel test ... --config oss_linux --build_tests_only -c dbg -- -base/...
```

* `--` means the end of the flags which start from `-`.
* `-<dir>/...` means exclusion of all targets under the `dir`.


### Run the specific test

```
bazel test base:util_test --config oss_linux -c dbg
```

* `util_test` is defined in `base/BUILD.bazel`.

### Output logs to stderr

```
bazel test base:util_test --config oss_linux --test_arg=--logtostderr --test_output=all
```

* The `--test_arg=--logtostderr --test_output=all` flags shows the output of
unitests to stderr.


## Build Mozc on other Linux environment

Note: This section is not about our officially supported build process.

### Software requirements

* Python: 3.7 or later
* Ibus: 1.5.4 or later
  * libglib
* Qt6: 6.2.5 or later, or Qt 6.2.x with working around [QTBUG-86080](https://bugreports.qt.io/browse/QTBUG-86080) by yourself
  * libgl

You may also need other libraries.
See the configurations of
[Dockerfile](https://github.com/google/mozc/blob/master/docker/ubuntu22.04/Dockerfile)
and
[GitHub Actions](https://github.com/google/mozc/blob/master/.github/workflows/linux.yaml).

### Build configurations

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

### Clean build cache

You may have some build errors when you update build environment or configurations.
In that case, cleaing the build cache possibly addresses the problem.

```
bazel clean
```

To clean the cache deeply, add the `--expunge` option.

```
bazel clean --expunge
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

⚠️ The GYP build no longer support the Ibus client and the GTK candidate window.
* https://github.com/google/mozc/issues/567

To keep using the GYP build without the Ibus client and the GTK candidate window
at this moment, please add the `--no_ibus_build` and `--no_gtk_build` flags
to build_mozc.py.

```
python3 build_mozc.py gyp
python3 build_mozc.py build -c Release package --no_ibus_build --no_gtk_build
```

`package` is an alias to build:
* //server:mozc_server
* //gui/tool:mozc_tool


You can also run unittests as follows.

```
python3 build_mozc.py runtests -c Debug
```

### Differences between Bazel build and GYP build

GYP build is under maintenance mode. New features might be supported by
Bazel only, and some features might be dropped as a trade-off to accept PRs.

Targets only for Bazel:
* Ibus client (//unix/ibus)
* AUX dictionary (//data/dictionary_oss:aux_dictionary)
* Filtered dictionary (//data/dictionary_oss:filtered_dictionary)
* SVS character input instead of CJK compatibility ideographs (//rewriter:single_kanji_rewriter)
* Zip code conversion (//server:mozc_server)
* Qt-based candidate window (//renderer:mozc_renderer)
* Build rules for icons (//unix/icons)


## GYP Build configurations

For the build configurations, please check the previous version.
https://github.com/google/mozc/blob/2.28.4880.102/docs/build_mozc_in_docker.md#gyp-build-configurations
