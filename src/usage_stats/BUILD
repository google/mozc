# -*- coding: utf-8 -*-
# Copyright 2010-2021, Google Inc.
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
    "py_binary_mozc",
)
load("//tools/build_defs:stubs.bzl", "portable_proto_library")

package(default_visibility = ["//:__subpackages__"])

portable_proto_library(
    name = "usage_stats_proto",
    config = "//protocol:portable_proto.pbtxt",
    header_outs = ["usage_stats.pb.h"],
    proto_deps = [":usage_stats_proto_full"],
    visibility = [
        # For //session:session_usage_observer.
        "//session:__pkg__",
    ],
)

proto_library(
    name = "usage_stats_proto_full",
    srcs = ["usage_stats.proto"],
    visibility = ["//visibility:private"],
)

cc_library_mozc(
    name = "usage_stats",
    srcs = [
        "usage_stats.cc",
        ":usage_stats_list",
    ],
    hdrs = ["usage_stats.h"],
    deps = [
        ":usage_stats_proto",
        ":usage_stats_uploader",
        "//base",
        "//base:logging",
        "//base:port",
        "//config:stats_config_util",
        "//storage:registry",
    ],
)

cc_test_mozc(
    name = "usage_stats_test",
    size = "small",
    srcs = ["usage_stats_test.cc"],
    tags = [
        "no_android",
    ],
    deps = [
        ":usage_stats",
        ":usage_stats_proto",
        "//base:port",
        "//base:system_util",
        "//config:stats_config_util",
        "//config:stats_config_util_mock",
        "//storage:registry",
        "//storage:storage_interaface",
        "//testing:gunit_main",
    ],
)

cc_library_mozc(
    name = "usage_stats_uploader",
    srcs = [
        "usage_stats_uploader.cc",
        ":usage_stats_list",
    ],
    hdrs = ["usage_stats_uploader.h"],
    deps = [
        "//base",
        "//base:port",
        "//storage:registry",
    ],
)

cc_test_mozc(
    name = "usage_stats_uploader_test",
    size = "small",
    srcs = ["usage_stats_uploader_test.cc"],
    tags = [
        "no_android",
    ],
    deps = [
        ":usage_stats",
        ":usage_stats_testing_util",
        ":usage_stats_uploader",
        "//base",
        "//base:port",
        "//base:system_util",
        "//base:version",
        "//storage:registry",
        "//testing:gunit_main",
    ],
)

cc_library_mozc(
    name = "usage_stats_testing_util",
    testonly = 1,
    srcs = ["usage_stats_testing_util.cc"],
    hdrs = ["usage_stats_testing_util.h"],
    deps = [
        ":usage_stats",
        ":usage_stats_proto",
        "//base",
        "//base:port",
        "//base:util",
        "//config:stats_config_util",
        "//config:stats_config_util_mock",
        "//testing",
        "@com_google_absl//absl/strings",
    ],
)

py_binary_mozc(
    name = "gen_stats_list",
    srcs = ["gen_stats_list.py"],
)

genrule(
    name = "usage_stats_list",
    srcs = ["//data/usage_stats:stats.def"],
    outs = ["usage_stats_list.h"],
    cmd = "$(location :gen_stats_list) $(SRCS) > $@",
    exec_tools = [":gen_stats_list"],
)
