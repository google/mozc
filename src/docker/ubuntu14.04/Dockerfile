# Copyright 2010-2014, Google Inc.
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

FROM ubuntu:14.04

ENV DEBIAN_FRONTEND noninteractive

# Package installation
RUN dpkg --add-architecture i386
RUN apt-get update
## Common packages for linux build environment
RUN apt-get install -y g++ python pkg-config subversion git curl bzip2 unzip make
## Packages for linux desktop version
RUN apt-get install -y libibus-1.0-dev libdbus-1-dev libglib2.0-dev subversion libqt4-dev libzinnia-dev tegaki-zinnia-japanese libgtk2.0-dev libxcb-xfixes0-dev
## Packages for Android
RUN apt-get install -y ant openjdk-6-jdk libc6:i386 libstdc++6:i386 libncurses5:i386 zlib1g:i386
## Packages for NaCl
RUN apt-get install -y libc6:i386 libstdc++6:i386
## For emacsian
RUN apt-get install -y emacs

ENV HOME /home/mozc_builder
RUN useradd --create-home --shell /bin/bash --base-dir /home mozc_builder
USER mozc_builder

# SDK setup
RUN mkdir -p /home/mozc_builder/work
WORKDIR /home/mozc_builder/work
## Android SDK/NDK
RUN curl -L http://dl.google.com/android/ndk/android-ndk32-r10b-linux-x86_64.tar.bz2 | tar -xj
RUN curl -LO http://dl.google.com/android/adt/22.6.2/adt-bundle-linux-x86_64-20140321.zip && unzip adt-bundle-linux-x86_64-20140321.zip && rm adt-bundle-linux-x86_64-20140321.zip
## NaCl SDK
RUN curl -LO http://storage.googleapis.com/nativeclient-mirror/nacl/nacl_sdk/nacl_sdk.zip && unzip nacl_sdk.zip && rm nacl_sdk.zip
RUN cd nacl_sdk && ./naclsdk install pepper_27
## depot_tools
RUN git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git

# env setup
ENV JAVA_HOME /usr/lib/jvm/java-6-openjdk-amd64
ENV ANDROID_NDK_HOME /home/mozc_builder/work/android-ndk-r10b
ENV ANDROID_HOME /home/mozc_builder/work/adt-bundle-linux-x86_64-20140321/sdk
ENV NACL_SDK_ROOT /home/mozc_builder/work/nacl_sdk/pepper_27
ENV PATH $PATH:${ANDROID_HOME}/tools:${ANDROID_HOME}/platform-tools:${ANDROID_NDK_HOME}:/home/mozc_builder/work/depot_tools

# check out Mozc source
RUN mkdir -p /home/mozc_builder/work/mozc
WORKDIR /home/mozc_builder/work/mozc
RUN gclient config http://mozc.googlecode.com/svn/trunk/src
RUN gclient sync

WORKDIR /home/mozc_builder/work/mozc/src
ENTRYPOINT bash
