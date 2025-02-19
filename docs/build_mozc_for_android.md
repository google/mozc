# How to build `libmozc.so` for Android

[![Android lib](https://github.com/google/mozc/actions/workflows/android.yaml/badge.svg)](https://github.com/google/mozc/actions/workflows/android.yaml)

## Summary

If you are not sure what the following commands do, please check the descriptions below
and make sure the operations before running them.

```
git clone https://github.com/google/mozc.git
cd mozc/src

export PYTHON_VENV_ROOT=${PWD}/python-venv
python3 -m venv ${PYTHON_VENV_ROOT}
source ${PYTHON_VENV_ROOT}/bin/activate
python3 -m pip install requests

python3 build_tools/update_deps.py

bazelisk build package --config oss_android --config release_build
```

Client code for Android apk is deprecated.
(the last revision with Android client code is
[afb03dd](https://github.com/google/mozc/commit/afb03ddfe72dde4cf2409863a3bfea160f7a66d8)).

## Setup

### System Requirements

Currently macOS and Linux are tested to build `libmozc.so` for Android.

### Software Requirements

Building `libmozc.so` for Android requires the following software.

 * [bazelisk](https://github.com/bazelbuild/bazelisk)
 * C++ compiler (GCC or clang)
 * Python 3.9 or later with the following pip module.
   * `requests`

## Build instructions

### Get the Code

You can download Mozc source code as follows:

```
git clone https://github.com/google/mozc.git
cd mozc/src
```

Hereafter you can do all the operations without changing directory.

### Set up and enable Python virtual environment

The following commands set up Python virtual environment under `mozc/src/python-venv`.

```
export PYTHON_VENV_ROOT=${PWD}/python-venv
python3 -m venv ${PYTHON_VENV_ROOT}
source ${PYTHON_VENV_ROOT}/bin/activate
python3 -m pip install requests
```

Using `mozc/src/python-venv` as the virtual environment location is not mandatory. Any other location should also work.

### Check out additional build dependencies

```
python build_tools/update_deps.py
```

In this step, additional build dependencies will be downloaded.

  * [Android NDK r28](https://github.com/android/ndk/wiki/Home/24fe2d7ee3591346e0e8ae615977a15c0a4fba40#ndk-r28)
  * [git submodules](../.gitmodules)

### Build `libmozc.so` for Android

```
bazelisk build package --config oss_android --config release_build
```

`package` is an alias to build `android/jni:native_libs`.
