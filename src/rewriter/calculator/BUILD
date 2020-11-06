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

# Visibility:  please choose a more appropriate default for the package,
# and update any rules that should be different.

load(
    "//:build_defs.bzl",
    "cc_library_mozc",
    "cc_test_mozc",
)

package(default_visibility = ["//:__subpackages__"])

cc_library_mozc(
    name = "calculator",
    srcs = ["calculator.cc"],
    hdrs = [
        "calculator_interface.h",
        "parser.c",
    ],
    deps = [
        "//base",
        "//base:logging",
        "//base:number_util",
        "//base:singleton",
        "//base:util",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ],
    alwayslink = 1,
)

cc_library_mozc(
    name = "calculator_mock",
    testonly = 1,
    srcs = ["calculator_mock.cc"],
    hdrs = ["calculator_mock.h"],
    deps = [
        ":calculator",
        "//base",
        "//base:logging",
    ],
)

cc_test_mozc(
    name = "calculator_test",
    size = "small",
    srcs = ["calculator_test.cc"],
    data = ["//data/test/calculator:testset.txt"],
    requires_full_emulation = False,
    deps = [
        ":calculator",
        "//base",
        "//base:file_util",
        "//base:logging",
        "//testing:gunit_main",
        "//testing:mozctest",
    ],
)
