# How to build Mozc in Docker

[![Linux](https://github.com/google/mozc/actions/workflows/linux.yaml/badge.svg)](https://github.com/google/mozc/actions/workflows/linux.yaml)
[![Android lib](https://github.com/google/mozc/actions/workflows/android.yaml/badge.svg)](https://github.com/google/mozc/actions/workflows/android.yaml)

## Summary

If you are not sure what the following commands do, please check the descriptions below
and make sure the operations before running them.

```
curl -O https://raw.githubusercontent.com/google/mozc/master/docker/ubuntu24.04/Dockerfile
docker build --rm --tag mozc_ubuntu24.04 .
docker create --interactive --tty --name mozc_build mozc_ubuntu24.04

docker start mozc_build
docker exec mozc_build bazelisk build package --config oss_linux --config release_build
docker cp mozc_build:/home/mozc_builder/work/mozc/src/bazel-bin/unix/mozc.zip .
```

## Introduction
Docker containers are available to build Mozc binaries for Android JNI library and Linux desktop.

## System Requirements
Currently, only Ubuntu 24.04 is tested to host the Docker container to build Mozc.

* [Dockerfile](https://github.com/google/mozc/blob/master/docker/ubuntu24.04/Dockerfile) for Ubuntu 24.04

## Build in Docker

### Set up Ubuntu 24.04 Docker container

```
curl -O https://raw.githubusercontent.com/google/mozc/master/docker/ubuntu24.04/Dockerfile
docker build --rm --tag mozc_ubuntu24.04 .
docker create --interactive --tty --name mozc_build mozc_ubuntu24.04
```

You may need to execute `docker` with `sudo` (e.g. `sudo docker build ...`).

Notes
* `mozc_ubuntu24.04` is a Docker image name (customizable).
* `mozc_build` is a Docker container name (customizable).
* Don't forget to rebuild Docker container when Dockerfile is updated.


### Build Mozc in Docker container

```
docker start mozc_build
docker exec mozc_build bazelisk build package --config oss_linux --config release_build
docker cp mozc_build:/home/mozc_builder/work/mozc/src/bazel-bin/unix/mozc.zip .
```

`mozc.zip` contains built files.

Notes
* You might want to execute `docker stop` after build.
* `mozc_build` is the Docker container name created in the above section.

-----

## Build Mozc for Linux Desktop

```
bazelisk build package --config oss_linux --config release_build
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
bazelisk test ... --config oss_linux --build_tests_only -c dbg
```

* `...` means all targets under the current and subdirectories.


### Run tests under the specific directories

```
bazelisk test base/... composer/... --config oss_linux --build_tests_only -c dbg
```

* `<dir>/...` means all targets under the `<dir>/` directory.


### Run tests without the specific directories

```
bazelisk test ... --config oss_linux --build_tests_only -c dbg -- -base/...
```

* `--` means the end of the flags which start from `-`.
* `-<dir>/...` means exclusion of all targets under the `dir`.


### Run the specific test

```
bazelisk test base:util_test --config oss_linux -c dbg
```

* `util_test` is defined in `base/BUILD.bazel`.

### Output logs to stderr

```
bazelisk test base:util_test --config oss_linux --test_arg=--stderrthreshold=0 --test_output=all
```

*   The `--test_arg=--stderrthreshold=0 --test_output=all` flags shows the
    output of unitests to stderr.

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
[Dockerfile](https://github.com/google/mozc/blob/master/docker/ubuntu24.04/Dockerfile)
and
[GitHub Actions](https://github.com/google/mozc/blob/master/.github/workflows/linux.yaml).

### Build configurations

To build Mozc on other Linux environment rather than the supported Docker
environment, you might need to modify the following files.

* src/config.bzl - configuration of install paths, etc.
* src/.bazelrc - compiler flags, etc.
* src/MODULE.bazel - build dependencies.

Tips: the following command makes the specified file untracked by Git.
```
git update-index --assume-unchanged src/config.bzl
```

This command reverts the above change.
```
git update-index --no-assume-unchanged src/config.bzl
```

### Forcing reconfigure external dependencies

You may have some build errors when you update build environment or configurations.
In that case, try the following command to [refetch external repositories](https://bazel.build/extending/repo#forcing_refetch_of_external_repositories).

```
bazelisk sync --configure
```

If the issue persists, also try the following command to [clean Bazel's build cache](https://bazel.build/docs/user-manual#clean)

```
bazelisk clean --expunge
```

## Build Mozc library for Android:

Client code for Android apk is deprecated.
(the last revision with Android client code is
[afb03dd](https://github.com/google/mozc/commit/afb03ddfe72dde4cf2409863a3bfea160f7a66d8)).

The conversion engine for Android is built with Bazel.

```
bazelisk build package --config oss_android
```

`package` is an alias to build `android/jni:native_libs`.

We have tested Android NDK r27. The former versions may not work.

-----

## Build Mozc for Linux Desktop with GYP (deprecated):

⚠️ The GYP build is deprecated and no longer supported.

Please check the previous version for more information.
https://github.com/google/mozc/blob/2.29.5374.102/docs/build_mozc_in_docker.md#build-mozc-for-linux-desktop-with-gyp-maintenance-mode
