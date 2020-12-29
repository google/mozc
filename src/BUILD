# -*- coding: utf-8 -*-
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

# Description:
#   Declaration of package_group for visibility managing.

load("//:build_defs.bzl", "select_mozc")
load("//tools/build_defs:stubs.bzl", "bzl_library")

package_group(
    name = "mozc",
    packages = ["//..."],
)

# White listed tests which are enabled on NaCl.
# data_manager tests are excluded since they require files on real file system.
# TODO(hsumita): Add chrome/nacl:nacl_test.
NACL_COMMON_TESTS = [
    "//base:nacl_test",
    "//composer:nacl_test",
    "//composer/internal:nacl_test",
    "//config:nacl_test",
    "//dictionary:nacl_test",
    "//engine:nacl_test",
    "//net:nacl_test",
    "//prediction:nacl_test",
    "//rewriter:nacl_test",
    "//session:nacl_test",
    "//session/internal:nacl_test",
    "//storage:nacl_test",
    "//storage/louds:nacl_test",
    "//transliteration:nacl_test",
    "//usage_stats:nacl_test",
]

test_suite(
    name = "nacl_test",
    tests = NACL_COMMON_TESTS + [
        "//converter:nacl_test",
    ],
)

test_suite(
    name = "nacl_tap_test",
    tests = NACL_COMMON_TESTS + [
        "//converter:nacl_tap_test",
    ],
)

test_suite(
    name = "android_test",
    tests = [
        "//base:android_test",
        "//session:android_test",
    ],
)

config_setting(
    name = "opt",
    values = {
        "compilation_mode": "opt",
    },
    visibility = ["//:__subpackages__"],
)

# Special target so as to define special macros for Android and CrOS (NaCl).
# Don't depend on this directly. Use cc_(library|binary|test)_mozc rule instead.
cc_library(
    name = "macro",
    defines = select_mozc(
        android = [
            "MOZC_DISABLE_SESSION_WATCHDOG",
            "NO_USAGE_REWRITER",
        ],
        default = [
        ],
        ios = [
            "MOZC_DISABLE_SESSION_WATCHDOG",
            "NO_USAGE_REWRITER",
        ],
        linux = [
            "GOOGLE_JAPANESE_INPUT_BUILD",
            "OS_LINUX",
        ],
        nacl = [
            "MOZC_DISABLE_SESSION_WATCHDOG",
            "MOZC_USE_MOZC_TESTING",
            "NO_USAGE_REWRITER",
        ],
        oss_android = [
            "MOZC_DISABLE_SESSION_WATCHDOG",
            "NO_USAGE_REWRITER",
        ],
        oss_linux = [
            "MOZC_BUILD",
            "OS_LINUX",
        ],
        wasm = [
            "GOOGLE_JAPANESE_INPUT_BUILD",
            "MOZC_DISABLE_SESSION_WATCHDOG",
            "NO_USAGE_REWRITER",
            "OS_WASM",
        ],
    ),
    visibility = ["//:__subpackages__"],
)

bzl_library(
    name = "build_defs_bzl",
    srcs = ["build_defs.bzl"],
    parse_tests = False,
    visibility = ["//:__subpackages__"],
    deps = [
        "//devtools/build_cleaner/skylark:build_defs_lib",
        "//tools/build_defs:stubs.bzl",
        "//tools/build_rules/android_cc_test:def",
    ],
)

filegroup(
    name = "package",
    srcs = select_mozc(
        default = [],
        oss_android = ["//android/jni:mozc_lib"],
        oss_linux = [
            "//gui/tool:mozc_tool",
            "//unix/emacs:mozc_emacs_helper",
            "//unix/ibus:ibus_mozc",
            "//renderer:mozc_renderer",
            "//server:mozc_server",
        ],
    ),
)
