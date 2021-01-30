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
    "select_mozc",
)
load("//tools/build_defs:stubs.bzl", "portable_proto_library")

package(default_visibility = ["//:__subpackages__"])

cc_library_mozc(
    name = "ipc",
    srcs = [
        "ipc.cc",
        "mach_ipc.cc",
        "unix_ipc.cc",
        "win32_ipc.cc",
    ],
    hdrs = ["ipc.h"],
    deps = [
        ":ipc_path_manager",
        "//base",
        "//base:cpu_stats",
        "//base:file_util",
        "//base:logging",
        "//base:port",
        "//base:scoped_handle",
        "//base:singleton",
        "//base:system_util",
        "//base:thread",
        "//base:util",
        "//base:win_util",
        "@com_google_absl//absl/memory",
    ] + select_mozc(
        ios = ["//base:mac_util"],
    ),
)

proto_library(
    name = "ipc_proto_full",
    srcs = ["ipc.proto"],
    visibility = ["//visibility:private"],
)

portable_proto_library(
    name = "ipc_proto",
    config_string = "allow_all: true",
    header_outs = ["ipc.pb.h"],
    proto_deps = [":ipc_proto_full"],
    visibility = ["//visibility:private"],
)

cc_library_mozc(
    name = "ipc_path_manager",
    srcs = ["ipc_path_manager.cc"],
    hdrs = ["ipc_path_manager.h"],
    deps = [
        ":ipc_hdr",
        ":ipc_proto",
        "//base",
        "//base:file_stream",
        "//base:file_util",
        "//base:logging",
        "//base:mutex",
        "//base:obfuscator_support",
        "//base:port",
        "//base:process_mutex",
        "//base:scoped_handle",
        "//base:singleton",
        "//base:system_util",
        "//base:util",
        "//base:version",
        "//base:win_util",
        "//testing:gunit_prod",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings:str_format",
    ] + select_mozc(
        ios = ["//base:mac_util"],
    ),
)

##    Commented out on 2011-03-09, because no other rule depends on it.
## cc_binary_mozc(name = "ipc_path_manager_main",
##           srcs = ["ipc_path_manager_main.cc",],
##           deps = [":ipc_path_manager",
##                   "//base:base",])

cc_test_mozc(
    name = "ipc_path_manager_test",
    size = "small",
    srcs = ["ipc_path_manager_test.cc"],
    requires_full_emulation = False,
    deps = [
        ":ipc",
        ":ipc_path_manager",
        ":ipc_proto",
        "//base",
        "//base:file_stream",
        "//base:file_util",
        "//base:flags",
        "//base:port",
        "//base:process_mutex",
        "//base:system_util",
        "//base:thread",
        "//base:util",
        "//base:version",
        "//testing:gunit_main",
    ],
)

cc_library_mozc(
    name = "ipc_mock",
    testonly = 1,
    srcs = ["ipc_mock.cc"],
    hdrs = ["ipc_mock.h"],
    deps = [
        ":ipc",
        "//base",
        "//base:version",
    ],
)

cc_library_mozc(
    name = "ipc_test_util",
    srcs = ["ipc_test_util.cc"],
    hdrs = ["ipc_test_util.h"],
    deps = [
        ":ipc",
        "//base",
        "//base:logging",
    ],
)

cc_test_mozc(
    name = "ipc_test",
    size = "small",
    srcs = ["ipc_test.cc"],
    copts = ["$(STACK_FRAME_UNLIMITED)"],  # ipc_test.cc
    requires_full_emulation = False,
    deps = [
        ":ipc",
        ":ipc_test_util",
        "//base",
        "//base:flags",
        "//base:port",
        "//base:system_util",
        "//base:thread",
        "//base:util",
        "//testing:gunit_main",
    ],
)

##    Commented out on 2011-03-09, because no other rule depends on it.
## cc_binary_mozc(name = "ipc_main",
##           srcs = ["ipc_main.cc",],
##           deps = [":ipc",
##                   "//base:base",],
##           copts = ["$(STACK_FRAME_UNLIMITED)"]  # ipc_main.cc
##           )

cc_library_mozc(
    name = "named_event",
    srcs = ["named_event.cc"],
    hdrs = ["named_event.h"],
    deps = [
        "//base",
        "//base:hash",
        "//base:logging",
        "//base:port",
        "//base:system_util",
        "//base:util",
        "@com_google_absl//absl/strings:str_format",
    ],
)

##    Commented out on 2011-03-09, because no other rule depends on it.
## cc_binary_mozc(name = "named_event_main",
##           srcs = ["named_event_main.cc",],
##           deps = [":named_event",
##                   "//base:base",])

cc_test_mozc(
    name = "named_event_test",
    size = "small",
    srcs = ["named_event_test.cc"],
    requires_full_emulation = False,
    deps = [
        ":named_event",
        "//base",
        "//base:clock",
        "//base:flags",
        "//base:port",
        "//base:system_util",
        "//base:thread",
        "//base:util",
        "//testing:gunit_main",
        "@com_google_absl//absl/memory",
    ],
)

cc_library_mozc(
    name = "process_watch_dog",
    srcs = ["process_watch_dog.cc"],
    hdrs = ["process_watch_dog.h"],
    deps = [
        "//base",
        "//base:logging",
        "//base:mutex",
        "//base:port",
        "//base:scoped_handle",
        "//base:thread",
        "//base:util",
    ],
)

# This header-only library was inserted because the target
#   //ipc:ipc_path_manager, which includes ipc/ipc.h
# cannot depend on the library
#   //ipc:ipc: Found an inverse include path.
# See http://go/cxx-layering-faq
cc_library_mozc(
    name = "ipc_hdr",
    textual_hdrs = ["ipc.h"],
    visibility = ["//visibility:private"],
    deps = [
        "//base:port",
        "//base:scoped_handle",
    ],
)

##    Commented out on 2011-03-09, because no other rule depends on it.
# cc_binary_mozc(
#     name = "process_watch_dog_main",
#     srcs = ["process_watch_dog_main.cc"],
#     deps = [
#         ":process_watch_dog",
#         "//base",
#         "//base:flags",
#         "//base:init_mozc",
#         "//base:logging",
#         "//base:number_util",
#         "//base:port",
#         "//base:util",
#     ],
# )

cc_test_mozc(
    name = "process_watch_dog_test",
    size = "small",
    srcs = ["process_watch_dog_test.cc"],
    requires_full_emulation = False,
    deps = [
        ":process_watch_dog",
        "//base",
        "//base:clock",
        "//base:logging",
        "//base:port",
        "//base:process",
        "//base:util",
        "//testing:gunit_main",
    ],
)

proto_library(
    name = "window_info_proto_full",
    srcs = ["window_info.proto"],
    visibility = ["//visibility:private"],
)

portable_proto_library(
    name = "window_info_proto",
    config_string = "allow_all: true",
    header_outs = ["window_info.pb.h"],
    proto_deps = [":window_info_proto_full"],
    visibility = ["//:__subpackages__"],
)
