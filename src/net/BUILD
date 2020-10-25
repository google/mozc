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

load(
    "//:build_defs.bzl",
    "cc_binary_mozc",
    "cc_library_mozc",
    "cc_test_mozc",
    "select_mozc",
)
load("//tools/build_defs:stubs.bzl", "portable_proto_library")

package_group(
    name = "self_and_friends",
    includes = [
    ],
    packages = [
        "//...",
    ],
)

test_suite(
    name = "nacl_test",
)

cc_library_mozc(
    name = "http_client_common_header",
    hdrs = [
        "http_client.h",
        "http_client_common.h",
    ],
    deps = ["//base:port"],
)

COMMON_HTTP_CLIENT_SRCS = [
    "http_client.cc",
    "http_client_null.cc",
    "proxy_manager.cc",
]

COMMON_HTTP_CLIENT_HDRS = [
    "http_client.h",
    "http_client_common.h",
    "http_client_null.h",
    "proxy_manager.h",
]

cc_library_mozc(
    name = "http_client",
    srcs = COMMON_HTTP_CLIENT_SRCS,
    hdrs = COMMON_HTTP_CLIENT_HDRS,
    visibility = [
        "//:__subpackages__",
    ],
    deps = select_mozc(
        client = [],
        default = ["//third_party/curl"],
    ) + [
        "//base",
        "//base:stopwatch",
    ] + select_mozc(
        ios = ["//base:mac_util"],
    ) + [
        "//base:logging",
        "//base:mutex",
        "//base:port",
        "//base:singleton",
        "//base:util",
    ],
)

cc_binary_mozc(
    name = "http_client_main",
    srcs = ["http_client_main.cc"],
    deps = [
        ":http_client",
        "//base",
        "//base:file_stream",
        "//base:flags",
        "//base:init_mozc",
        "//base:port",
    ],
)

cc_library_mozc(
    name = "http_client_mock",
    testonly = 1,
    srcs = ["http_client_mock.cc"],
    hdrs = ["http_client_mock.h"],
    deps = [
        ":http_client",
        "//base",
        "//base:logging",
        "//base:util",
    ],
)

cc_test_mozc(
    name = "http_client_mock_test",
    srcs = ["http_client_mock_test.cc"],
    requires_full_emulation = False,
    deps = [
        ":http_client",
        ":http_client_mock",
        "//testing:gunit_main",
    ],
)

cc_library_mozc(
    name = "jsoncpp",
    hdrs = ["jsoncpp.h"],
    visibility = [
        "//:__subpackages__",
    ],
    deps = [
        "//third_party/jsoncpp:json",
    ],
)

cc_library_mozc(
    name = "jsonpath",
    srcs = ["jsonpath.cc"],
    hdrs = ["jsonpath.h"],
    visibility = [":self_and_friends"],
    deps = [
        ":jsoncpp",
        "//base",
        "//base:logging",
        "//base:number_util",
        "//base:port",
        "//base:util",
    ],
)

cc_test_mozc(
    name = "jsonpath_test",
    srcs = ["jsonpath_test.cc"],
    requires_full_emulation = False,
    deps = [
        ":jsonpath",
        "//testing:gunit_main",
    ],
)

cc_library_mozc(
    name = "json_util",
    srcs = ["json_util.cc"],
    hdrs = ["json_util.h"],
    visibility = [
        "//:__subpackages__",
    ],
    deps = [
        ":jsoncpp",
        "//base",
        "//base:logging",
        "//base:number_util",
        "//base:port",
        "//base/protobuf",
        "//base/protobuf:descriptor",
        "//base/protobuf:message",
    ],
)

portable_proto_library(
    name = "json_util_test_proto",
    testonly = 1,
    config = "//protocol:portable_proto.pbtxt",
    header_outs = ["json_util_test.pb.h"],
    proto_deps = [":json_util_test_proto_full"],
    visibility = ["//visibility:private"],
)

proto_library(
    name = "json_util_test_proto_full",
    testonly = 1,
    srcs = ["json_util_test.proto"],
    visibility = ["//visibility:private"],
)

cc_proto_library(
    name = "json_util_test_proto_full_cc_proto",
    testonly = 1,
    visibility = ["//visibility:private"],
    deps = [":json_util_test_proto_full"],
)

cc_test_mozc(
    name = "json_util_test",
    srcs = ["json_util_test.cc"],
    requires_full_emulation = False,
    deps = [
        ":json_util",
        ":json_util_test_proto",
        ":jsoncpp",
        "//base:logging",
        "//base:port",
        "//base:util",
        "//testing:gunit_main",
        "//testing:testing_util",
    ],
)
