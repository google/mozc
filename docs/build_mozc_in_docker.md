How to build Mozc in Docker
===========================

# Introduction
Docker containers are available to build Mozc binaries for Android, NaCl, and Linux desktop.

# System Requirements
Currently, only Ubuntu 14.04 is tested to host the Docker container to build Mozc. See [official document](https://docs.docker.com/engine/installation/linux/docker-ce/ubuntu/) to set up Docker for Ubuntu 14.04.

## Set up Ubuntu 14.04 Docker container

```
mkdir ubuntu14.04 && cd ubuntu14.04
curl -O https://raw.githubusercontent.com/google/mozc/master/docker/ubuntu14.04/Dockerfile
sudo docker build --rm -t $USER/mozc_ubuntu14.04 .
sudo docker run --interactive --tty --rm $USER/mozc_ubuntu14.04
```

## Set up Fedora 23 Docker container
Fedora 23 container is also provided just for your reference.

Building Mozc for Android is not supported on Fedora 23 due to the lack of OpenJDK 1.7 support.  See [Red Hat Bugzilla – Bug 1190137](https://bugzilla.redhat.com/show_bug.cgi?id=1190137) for details.
```
mkdir fedora23 && cd fedora23
curl -O https://raw.githubusercontent.com/google/mozc/master/docker/fedora23/Dockerfile
sudo docker build --rm -t $USER/mozc_fedora23 .
sudo docker run --interactive --tty --rm $USER/mozc_fedora23
```

### Hint
Don't forget to rebuild Docker container when Dockerfile is updated.

# Build in the container
Before explaining detailed build configurations and options, let's walk through the simplest cases to see how it looks like.

### Build Mozc for Android:

```
python build_mozc.py gyp --target_platform=Android
python build_mozc.py build -c Debug android/android.gyp:apk
```

### Build Mozc for NaCl:

```
python build_mozc.py gyp --target_platform=NaCl --nacl_sdk_root=$NACL_SDK_ROOT
python build_mozc.py build -c Release chrome/nacl/nacl_extension.gyp:nacl_mozc
```

### Build Mozc for Linux Desktop:

```
python build_mozc.py gyp --target_platform=Linux
python build_mozc.py build -c Release unix/ibus/ibus.gyp:ibus_mozc unix/emacs/emacs.gyp:mozc_emacs_helper server/server.gyp:mozc_server gui/gui.gyp:mozc_tool renderer/renderer.gyp:mozc_renderer
```

You can also run unittests as follows.

```
python build_mozc.py runtests -c Debug
```

## Build configurations
In `python build_mozc.py gyp` step, there are two different styles to customize configurations.  One is `GYP_DEFINES` environment variable and the other is commandline option.

```
[GYP_DEFINES="..."] python build_mozc.py gyp [options]
```

### GYP_DEFINES
You can specify `GYP_DEFINES` environment variable to change variables in GYP files, which you can find many directories in Mozc's source tree.  [common.gypi](../src/gyp/common.gypi) is an example.
Here are examples of GYP variables that you may want to change for Linux desktop build.

  * `document_dir`: Directory path where Mozc's license file is placed
  * `ibus_mozc_path`: ibus-mozc executable path
  * `ibus_mozc_icon_path`: ibus-mozc icon path
  * `zinnia_model_file`: Zinnia's model data path

Note that you can specify multiple GYP variables as follows.

```
GYP_DEFINES="ibus_mozc_path=/usr/lib/ibus-mozc/ibus-engine-mozc ibus_mozc_icon_path=/usr/share/ibus-mozc/product_icon.png document_dir=/usr/share/doc/mozc zinnia_model_file=/usr/share/zinnia/model/tomoe/handwriting-ja.model" python build_mozc.py gyp
```

### command line options
You can find many command line options as follows.  
```
python build_mozc.py gyp --help
```
Here we show some notable options.

#### --target_platform
You can use `--target_platform` option to specify the target OS on which Mozc will run.  Following options are available.

  * `Android`
  * `NaCl`
  * `Linux` (default)

If you don't specify this option, `--target_platform=Linux` will be used implicitly.

#### --noqt (Linux desktop target only)
You can use `--noqt` option to build Mozc without depending on Qt 5 library.

#### --server_dir (Linux desktop target only)
You can use `--server_dir` option to specify the directory name where `mozc_server` will be installed.

## Compile options
In `build_mozc.py build` step, you can specify build types (`Release` or `Debug`) and one or more build targets.  Please find each GYP file to see what build targets are defined.

```
python build_mozc.py build -c {Release, Debug} [gyp_path_1.gyp:gyp_target_name1] [gyp_path_2.gyp:gyp_target_name2] 
```

## Android specific topics

### Application package name

**CAUTION**: Currently the application package name is fixed (org.mozc.android.inputmethod.japanese). Don't publish the built package. If you want to publish, specify `--android_application_id` to `build_mozc.py gyp` command and manually update related files.

### zipalign

Android version of Mozc may not run if the APK file is not aligned with [zipalign](https://developer.android.com/tools/help/zipalign.html) command.
