# Copyright 2010-2020, Google Inc.
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

FROM ubuntu:20.04

ENV DEBIAN_FRONTEND noninteractive

# Package installation
RUN apt-get update
## Common packages for linux build environment
RUN apt-get install -y clang libc++-dev libc++abi-dev python pkg-config git curl bzip2 unzip make ninja-build
## Packages for linux desktop version
RUN apt-get install -y libibus-1.0-dev libglib2.0-dev qtbase5-dev libgtk2.0-dev libxcb-xfixes0-dev
## Packages for misc tools
RUN apt-get install -y nano

## For Bazel
## https://docs.bazel.build/versions/master/install-ubuntu.html
RUN apt-get install -y curl gnupg libncurses5
RUN curl -fsSL https://bazel.build/bazel-release.pub.gpg | gpg --dearmor > bazel.gpg
RUN mv bazel.gpg /etc/apt/trusted.gpg.d/
RUN echo "deb [arch=amd64] https://storage.googleapis.com/bazel-apt stable jdk1.8" | tee /etc/apt/sources.list.d/bazel.list
RUN apt-get update
RUN apt-get install -y bazel

# Working environemnt
ENV HOME /home/mozc_builder
RUN useradd --create-home --shell /bin/bash --base-dir /home mozc_builder
USER mozc_builder
RUN mkdir -p /home/mozc_builder/work
WORKDIR /home/mozc_builder/work

## For Android NDK
RUN curl -LO http://dl.google.com/android/repository/android-ndk-r21b-linux-x86_64.zip && unzip android-ndk-r21b-linux-x86_64.zip && rm android-ndk-r21b-linux-x86_64.zip
ENV ANDROID_NDK_HOME /home/mozc_builder/work/android-ndk-r21b

# check out Mozc source with submodules
RUN mkdir /home/mozc_builder/work/mozc
# COPY --chown=mozc_builder:mozc_builder src/ /home/mozc_builder/work/mozc/src/
RUN git clone https://github.com/google/mozc.git -b master --single-branch --recursive

WORKDIR /home/mozc_builder/work/mozc/src
ENTRYPOINT bash
