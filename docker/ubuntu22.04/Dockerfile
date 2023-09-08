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

# Build instructions with this Dockerfile.
# https://github.com/google/mozc/blob/master/docs/build_mozc_in_docker.md

FROM ubuntu:22.04

ENV DEBIAN_FRONTEND noninteractive

# Package installation
RUN apt update --fix-missing
RUN apt upgrade -y
## Common packages for linux build environment
RUN apt-get install -y python3 pkg-config git curl bzip2 unzip make ninja-build
## Packages for linux desktop version
RUN apt-get install -y libibus-1.0-dev libglib2.0-dev qt6-base-dev libgl-dev
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
## Work around https://bugreports.qt.io/browse/QTBUG-86080 for Ubuntu 22.04
ENV PKG_CONFIG_PATH="/home/mozc_builder/work/mozc/docker/ubuntu22.04/qt6-core-pkgconfig:${PKG_CONFIG_PATH}"

## For Android NDK
RUN curl -LO https://dl.google.com/android/repository/android-ndk-r25c-linux.zip && unzip android-ndk-r25c-linux.zip && rm android-ndk-r25c-linux.zip
ENV ANDROID_NDK_HOME /home/mozc_builder/work/android-ndk-r25c

# check out Mozc source with submodules
RUN mkdir /home/mozc_builder/work/mozc
# COPY --chown=mozc_builder:mozc_builder src/ /home/mozc_builder/work/mozc/src/
RUN git clone https://github.com/google/mozc.git -b master --single-branch --recursive

WORKDIR /home/mozc_builder/work/mozc/src
ENTRYPOINT bash
