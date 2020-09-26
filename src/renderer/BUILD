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
    "cc_library_mozc",
    "cc_test_mozc",
    "select_mozc",
)

package(default_visibility = ["//:__subpackages__"])

cc_library_mozc(
    name = "renderer_client",
    srcs = ["renderer_client.cc"],
    hdrs = ["renderer_client.h"],
    deps = [
        ":renderer_interface",
        "//base",
        "//base:clock",
        "//base:logging",
        "//base:mutex",
        "//base:port",
        "//base:process",
        "//base:run_level",
        "//base:system_util",
        "//base:thread",
        "//base:util",
        "//base:version",
        "//ipc",
        "//ipc:named_event",
        "//protocol:renderer_proto",
    ] + select_mozc(
        ios = ["//base:mac_util"],
    ),
)

cc_test_mozc(
    name = "renderer_client_test",
    size = "medium",
    srcs = ["renderer_client_test.cc"],
    requires_full_emulation = False,
    tags = [
        "disable_on_nacl_test",
        "no_android",
    ],
    deps = [
        ":renderer_client",
        ":renderer_interface",
        "//base",
        "//base:logging",
        "//base:number_util",
        "//base:port",
        "//base:util",
        "//base:version",
        "//ipc",
        "//protocol:renderer_proto",
        "//testing:gunit_main",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_library_mozc(
    name = "renderer_server",
    srcs = ["renderer_server.cc"],
    hdrs = ["renderer_server.h"],
    deps = [
        ":renderer_interface",
        "//base",
        "//base:flags",
        "//base:logging",
        "//base:port",
        "//base:system_util",
        "//client",
        "//client:client_interface",
        "//config:config_handler",
        "//ipc",
        "//ipc:named_event",
        "//ipc:process_watch_dog",
        "//protocol:config_proto",
        "//protocol:renderer_proto",
    ],
)

cc_test_mozc(
    name = "renderer_server_test",
    size = "medium",
    srcs = ["renderer_server_test.cc"],
    requires_full_emulation = False,
    tags = [
        "disable_on_nacl_test",
        "no_android",
    ],
    deps = [
        ":renderer_client",
        ":renderer_interface",
        ":renderer_server",
        "//base",
        "//base:logging",
        "//base:port",
        "//base:system_util",
        "//base:util",
        "//ipc:ipc_test_util",
        "//protocol:renderer_proto",
        "//testing:googletest",
        "//testing:gunit_main",
    ],
)

cc_library_mozc(
    name = "table_layout",
    srcs = [
        "table_layout.cc",
        "table_layout_interface.h",
    ],
    hdrs = ["table_layout.h"],
    deps = [
        "//base",
        "//base:coordinates",
        "//base:logging",
        "//base:port",
    ],
)

cc_test_mozc(
    name = "table_layout_test",
    size = "small",
    srcs = [
        "table_layout_interface.h",
        "table_layout_test.cc",
    ],
    requires_full_emulation = False,
    tags = [
        "disable_on_nacl_test",
        "no_android",
    ],
    deps = [
        ":table_layout",
        "//base",
        "//testing:gunit_main",
    ],
)

cc_library_mozc(
    name = "window_util",
    srcs = ["window_util.cc"],
    hdrs = ["window_util.h"],
    deps = [
        "//base",
        "//base:coordinates",
    ],
)

cc_library_mozc(
    name = "renderer_interface",
    hdrs = ["renderer_interface.h"],
    visibility = ["//visibility:private"],
)

cc_library_mozc(
    name = "unix_const",
    hdrs = ["unix/const.h"],
    visibility = ["//visibility:private"],
    deps = ["//base:port"],
)

cc_library_mozc(
    name = "renderer_mock",
    testonly = 1,
    hdrs = ["renderer_mock.h"],
    visibility = ["//visibility:private"],
    deps = [
        ":renderer_interface",
        "//testing:gmock",
    ],
)

cc_test_mozc(
    name = "window_util_test",
    size = "small",
    srcs = ["window_util_test.cc"],
    requires_full_emulation = False,
    tags = [
        "disable_on_nacl_test",
        "no_android",
    ],
    deps = [
        ":window_util",
        "//base",
        "//base:coordinates",
        "//testing:gunit_main",
    ],
)
