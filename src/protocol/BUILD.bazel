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

load("//tools/build_defs:stubs.bzl", "jspb_proto_library")
load("//tools/build_defs:stubs.bzl", "portable_proto_library")

package(default_visibility = [
    "//:__subpackages__",
])

exports_files(["portable_proto.pbtxt"])

# Generates proto library for Android.
# proto_library() depends on //base, which doesn't build on Android config.
# portable_proto_library() generates proto library which is independent
# from //base by using protoc in third_party.
portable_proto_library(
    name = "user_dictionary_storage_proto",
    config_string = "optimize_mode: SPEED allow_all: true",
    header_outs = [
        "user_dictionary_storage.pb.h",
    ],
    proto_deps = [
        ":user_dictionary_storage_proto_full",
    ],
)

portable_proto_library(
    name = "candidates_proto",
    config_string = "optimize_mode: SPEED allow_all: true",
    header_outs = [
        "candidates.pb.h",
    ],
    proto_deps = [
        ":candidates_proto_full",
    ],
)

portable_proto_library(
    name = "config_proto",
    config_string = "optimize_mode: SPEED allow_all: true",
    header_outs = [
        "config.pb.h",
    ],
    proto_deps = [
        ":config_proto_full",
    ],
)

portable_proto_library(
    name = "engine_builder_proto",
    config_string = "optimize_mode: SPEED allow_all: true",
    header_outs = [
        "engine_builder.pb.h",
    ],
    proto_deps = [
        ":engine_builder_proto_full",
    ],
)

portable_proto_library(
    name = "commands_proto",
    config_string = "optimize_mode: SPEED allow_all: true",
    header_outs = [
        "commands.pb.h",
    ],
    portable_deps = [
        ":candidates_proto",
        ":config_proto",
        ":engine_builder_proto",
        ":user_dictionary_storage_proto",
    ],
    proto_deps = ["commands_proto_full"],
)

portable_proto_library(
    name = "state_proto",
    config_string = "optimize_mode: SPEED allow_all: true",
    header_outs = [
        "state.pb.h",
    ],
    portable_deps = [
        ":candidates_proto",
        ":commands_proto",
    ],
    proto_deps = ["state_proto_full"],
)

proto_library(
    name = "commands_proto_full",
    srcs = [
        "commands.proto",
    ],
    deps = [
        ":candidates_proto_full",
        ":config_proto_full",
        ":engine_builder_proto_full",
        ":user_dictionary_storage_proto_full",
    ],
)

jspb_proto_library(
    name = "commands_jspb_proto",
    visibility = [],
    deps = [":commands_proto_full"],
)

# Proto library for Android Java.
java_lite_proto_library(
    name = "commands_proto_android_java",
    visibility = [
    ],
    deps = [
        ":candidates_proto_full",
        ":commands_proto_full",
        ":config_proto_full",
        ":engine_builder_proto_full",
        ":user_dictionary_storage_proto_full",
    ],
)

proto_library(
    name = "config_proto_full",
    srcs = ["config.proto"],
    visibility = [],
)

jspb_proto_library(
    name = "config_jspb_proto",
    visibility = [],
    deps = [":config_proto_full"],
)

java_lite_proto_library(
    name = "config_proto_lite",
    visibility = [
    ],
    deps = [":config_proto_full"],
)

proto_library(
    name = "renderer_proto_full",
    srcs = [
        "renderer_command.proto",
        "renderer_style.proto",
    ],
    visibility = ["//visibility:private"],
    deps = [":commands_proto_full"],
)

portable_proto_library(
    name = "renderer_proto",
    config_string = "optimize_mode: SPEED allow_all: true",
    header_outs = [
        "renderer_command.pb.h",
        "renderer_style.pb.h",
    ],
    portable_deps = [":commands_proto"],
    proto_deps = [":renderer_proto_full"],
)

proto_library(
    name = "candidates_proto_full",
    srcs = [
        "candidates.proto",
    ],
)

jspb_proto_library(
    name = "candidates_jspb_proto",
    visibility = [],
    deps = [":candidates_proto_full"],
)

proto_library(
    name = "state_proto_full",
    srcs = ["state.proto"],
    visibility = ["//visibility:private"],
    deps = [
        ":candidates_proto_full",
        ":commands_proto_full",
    ],
)

proto_library(
    name = "user_dictionary_storage_proto_full",
    srcs = ["user_dictionary_storage.proto"],
    visibility = [],
)

jspb_proto_library(
    name = "user_dictionary_storage_jspb_proto",
    visibility = [],
    deps = [":user_dictionary_storage_proto_full"],
)

java_lite_proto_library(
    name = "user_dictionary_storage_proto_lite",
    visibility = [],
    deps = [":user_dictionary_storage_proto_full"],
)

proto_library(
    name = "segmenter_data_proto_full",
    srcs = ["segmenter_data.proto"],
    visibility = ["//visibility:private"],
)

portable_proto_library(
    name = "segmenter_data_proto",
    config_string = "optimize_mode: SPEED allow_all: true",
    header_outs = ["segmenter_data.pb.h"],
    proto_deps = [":segmenter_data_proto_full"],
)

proto_library(
    name = "engine_builder_proto_full",
    srcs = ["engine_builder.proto"],
    visibility = ["//visibility:private"],
)
