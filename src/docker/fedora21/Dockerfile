# Copyright 2010-2015, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

FROM fedora:21

# Package installation
RUN yum -y update
## Common packages for linux build environment
RUN yum install -y clang libstdc++-static python git curl bzip2 unzip
## Packages for linux desktop version
RUN yum install -y ibus-devel glib2-devel qt-devel zinnia-tomoe-ja gtk2-devel libxcb-devel
## Packages for Android
RUN yum install -y java-1.7.0-openjdk-devel jsr-305 ant glibc.i686 glibc-devel.i686 libstdc++.i686 ncurses-devel.i686 zlib-devel.i686 zip
## For emacsian
RUN yum install -y emacs

ENV HOME /home/mozc_builder
RUN useradd --create-home --shell /bin/bash --base-dir /home mozc_builder
USER mozc_builder

# SDK setup
RUN mkdir -p /home/mozc_builder/work
WORKDIR /home/mozc_builder/work

## Android SDK/NDK
RUN curl -L http://dl.google.com/android/ndk/android-ndk-r10e-linux-x86_64.bin -O && chmod u+x android-ndk-r10e-linux-x86_64.bin && ./android-ndk-r10e-linux-x86_64.bin && rm android-ndk-r10e-linux-x86_64.bin
RUN curl -L http://dl.google.com/android/android-sdk_r24.1.2-linux.tgz | tar -zx
ENV ANDROID_NDK_HOME /home/mozc_builder/work/android-ndk-r10e
ENV ANDROID_HOME /home/mozc_builder/work/android-sdk-linux
ENV PATH $PATH:${ANDROID_HOME}/tools:${ANDROID_HOME}/platform-tools:${ANDROID_NDK_HOME}
RUN echo y | android update sdk --all --force --no-ui --filter android-22
RUN echo y | android update sdk --all --force --no-ui --filter build-tools-22.0.0
RUN echo y | android update sdk --all --force --no-ui --filter extra-android-support
RUN echo y | android update sdk --all --force --no-ui --filter platform-tool

## NaCl SDK
RUN curl -LO http://storage.googleapis.com/nativeclient-mirror/nacl/nacl_sdk/nacl_sdk.zip && unzip nacl_sdk.zip && rm nacl_sdk.zip
RUN cd nacl_sdk && ./naclsdk install pepper_40
ENV NACL_SDK_ROOT /home/mozc_builder/work/nacl_sdk/pepper_40

## depot_tools for Ninja prebuilt
RUN git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
ENV PATH $PATH:/home/mozc_builder/work/depot_tools

# check out Mozc source with submodules
RUN git clone https://github.com/google/mozc.git --recursive

WORKDIR /home/mozc_builder/work/mozc/src
ENTRYPOINT bash
