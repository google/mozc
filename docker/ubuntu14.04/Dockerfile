# Copyright 2010-2018, Google Inc.
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

FROM ubuntu:14.04.4

ENV DEBIAN_FRONTEND noninteractive

# Package installation
RUN apt-get update
## Common packages for linux build environment
RUN apt install -y clang python pkg-config git curl bzip2 unzip make
## Packages for linux desktop version
RUN apt install -y libibus-1.0-dev libglib2.0-dev qtbase5-dev libgtk2.0-dev libxcb-xfixes0-dev
## Packages for Android
RUN apt install -y --no-install-recommends openjdk-7-jdk openjdk-7-jre-headless libjsr305-java ant zip libc6-i386 lib32stdc++6 lib32ncurses5 lib32z1
## Packages for NaCl
RUN apt install -y libc6-i386 lib32stdc++6
## For emacsian
RUN apt install -y emacs

ENV HOME /home/mozc_builder
RUN useradd --create-home --shell /bin/bash --base-dir /home mozc_builder
USER mozc_builder

# SDK setup
RUN mkdir -p /home/mozc_builder/work
WORKDIR /home/mozc_builder/work

## Android SDK/NDK
RUN curl -LO http://dl.google.com/android/repository/android-ndk-r16b-linux-x86_64.zip && unzip android-ndk-r16b-linux-x86_64.zip && rm android-ndk-r16b-linux-x86_64.zip
RUN curl -L http://dl.google.com/android/android-sdk_r24.1.2-linux.tgz | tar -zx
ENV ANDROID_NDK_HOME /home/mozc_builder/work/android-ndk-r16b
ENV ANDROID_HOME /home/mozc_builder/work/android-sdk-linux
ENV PATH $PATH:${ANDROID_HOME}/tools:${ANDROID_HOME}/platform-tools:${ANDROID_NDK_HOME}
RUN echo y | android update sdk --all --force --no-ui --filter android-22
RUN echo y | android update sdk --all --force --no-ui --filter build-tools-22.0.0
RUN echo y | android update sdk --all --force --no-ui --filter platform-tool
RUN mkdir -p ${ANDROID_HOME}/extras/android
RUN curl -LO https://dl.google.com/dl/android/repository/support_r23.1.1.zip && unzip -d ${ANDROID_HOME}/extras/android support_r23.1.1.zip support/v13/android-support-v13.jar && rm support_r23.1.1.zip

## depot_tools for Ninja prebuilt
RUN git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
ENV PATH $PATH:/home/mozc_builder/work/depot_tools

# check out Mozc source with submodules
RUN git clone https://github.com/google/mozc.git -b master --single-branch --recursive

WORKDIR /home/mozc_builder/work/mozc/src
ENTRYPOINT bash
