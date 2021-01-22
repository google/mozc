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

load(
    "//:build_defs.bzl",
    "cc_library_mozc",
    "cc_test_mozc",
    "select_mozc",
)

package(default_visibility = [
    "//:__subpackages__",
])

cc_library_mozc(
    name = "engine_builder_interface",
    hdrs = ["engine_builder_interface.h"],
    deps = [
        ":engine_interface",
        "//base:port",
        "//protocol:engine_builder_proto",
        "@com_google_absl//absl/strings",
    ],
)

cc_library_mozc(
    name = "engine_builder",
    srcs = ["engine_builder.cc"],
    hdrs = ["engine_builder.h"],
    deps = [
        ":engine",
        ":engine_builder_interface",
        ":engine_interface",
        "//base:file_util",
        "//base:logging",
        "//base:port",
        "//base:thread",
        "//data_manager",
        "//protocol:engine_builder_proto",
        "@com_google_absl//absl/memory",
    ],
)

cc_test_mozc(
    name = "engine_builder_test",
    srcs = ["engine_builder_test.cc"],
    data = [
        "engine_builder_test.cc",
        "//data_manager/testing:mock_mozc.data",
    ],
    requires_full_emulation = False,
    deps = [
        ":engine_builder",
        "//base:file_util",
        "//prediction:predictor_interface",
        "//protocol:engine_builder_proto",
        "//testing:gunit_main",
        "//testing:mozctest",
    ],
)

cc_library_mozc(
    name = "engine_interface",
    hdrs = ["engine_interface.h"],
    deps = [
        "//base",
        "//base:port",
        "//data_manager:data_manager_interface",
        "//dictionary:suppression_dictionary",
        "@com_google_absl//absl/strings",
    ],
)

cc_library_mozc(
    name = "engine",
    srcs = [
        "engine.cc",
    ],
    hdrs = ["engine.h"],
    visibility = [
        "//android/jni:__pkg__",
        "//converter:__pkg__",
        "//ios:__pkg__",
        "//session:__pkg__",
        "//wasm:__pkg__",
    ],
    deps = [
        ":engine_interface",
        ":user_data_manager_interface",
        "//base",
        "//base:logging",
        "//base:port",
        "//base:status",
        "//converter",
        "//converter:connector",
        "//converter:converter_interface",
        "//converter:immutable_converter_interface",
        "//converter:immutable_converter_no_factory",
        "//converter:segmenter",
        "//data_manager:data_manager_interface",
        "//dictionary:dictionary_impl",
        "//dictionary:dictionary_interface",
        "//dictionary:pos_group",
        "//dictionary:pos_matcher_lib",
        "//dictionary:suffix_dictionary",
        "//dictionary:suppression_dictionary",
        "//dictionary:user_dictionary",
        "//dictionary:user_pos",
        "//dictionary/system:system_dictionary",
        "//dictionary/system:value_dictionary",
        "//prediction:dictionary_predictor",
        "//prediction:predictor",
        "//prediction:predictor_interface",
        "//prediction:suggestion_filter",
        "//prediction:user_history_predictor",
        "//rewriter",
        "//rewriter:rewriter_interface",
        "@com_google_absl//absl/memory",
    ],
)

cc_library_mozc(
    name = "mock_converter_engine",
    testonly = 1,
    srcs = ["mock_converter_engine.cc"],
    hdrs = ["mock_converter_engine.h"],
    visibility = ["//session:__subpackages__"],
    deps = [
        ":engine_interface",
        ":user_data_manager_mock",
        "//base",
        "//base:logging",
        "//base:port",
        "//converter:converter_interface",
        "//converter:converter_mock",
    ],
)

cc_library_mozc(
    name = "engine_stub",
    testonly = 1,
    hdrs = ["engine_stub.h"],
    visibility = ["//session:__subpackages__"],
    deps = [":engine_interface"],
)

cc_library_mozc(
    name = "engine_factory",
    hdrs = ["engine_factory.h"],
    deps = select_mozc(
        android = [":android_engine_factory"],
        default = [":google_engine_factory"],
        oss = [":oss_engine_factory"],
    ),
)

cc_test_mozc(
    name = "engine_factory_test",
    srcs = ["engine_factory_test.cc"],
    requires_full_emulation = False,
    deps = [
        ":engine_factory",
        ":engine_interface",
        "//base",
        "//base:logging",
        "//prediction:predictor_interface",
        "//testing:gunit_main",
    ],
)

cc_library_mozc(
    name = "google_engine_factory",
    hdrs = ["google_engine_factory.h"],
    visibility = [
        "//converter:__pkg__",
        "//evaluation:__subpackages__",
    ],
    deps = [
        ":engine",
        "//base",
        "//base:logging",
        "//data_manager/google:google_data_manager",
        "//prediction:predictor",
    ],
)

cc_library_mozc(
    name = "android_engine_factory",
    hdrs = ["android_engine_factory.h"],
    visibility = [
        "//converter:__pkg__",
        "//evaluation:__subpackages__",
    ],
    deps = [
        ":engine",
        "//base:logging",
        "//data_manager/android:android_data_manager",
    ],
)

cc_library_mozc(
    name = "oss_engine_factory",
    hdrs = ["oss_engine_factory.h"],
    visibility = ["//evaluation:__subpackages__"],
    deps = [
        ":engine",
        "//base:logging",
        "//data_manager/oss:oss_data_manager",
    ],
)

cc_library_mozc(
    name = "mock_data_engine_factory",
    testonly = 1,
    srcs = ["mock_data_engine_factory.cc"],
    hdrs = ["mock_data_engine_factory.h"],
    visibility = [
        "//converter:__pkg__",
        "//evaluation:__pkg__",
        "//rewriter:__pkg__",
        "//session:__pkg__",
    ],
    deps = [
        ":engine",
        "//base:logging",
        "//data_manager/testing:mock_data_manager",
    ],
)

cc_library_mozc(
    name = "small_immutable_converter",
    srcs = ["small_immutable_converter.cc"],
    hdrs = ["small_immutable_converter.h"],
    visibility = ["//visibility:private"],
    deps = [
        "//base",
        "//base:logging",
        "//base:port",
        "//converter:connector",
        "//converter:immutable_converter_interface",
        "//converter:immutable_converter_no_factory",
        "//converter:segmenter",
        "//converter:segments",
        "//data_manager/android:android_data_manager",
        "//dictionary:dictionary_impl",
        "//dictionary:dictionary_interface",
        "//dictionary:pos_group",
        "//dictionary:pos_matcher_lib",
        "//dictionary:suffix_dictionary",
        "//dictionary:suppression_dictionary",
        "//dictionary:user_dictionary_stub",
        "//dictionary/system:system_dictionary",
        "//dictionary/system:value_dictionary",
        "//prediction:suggestion_filter",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
    ],
)

cc_library_mozc(
    name = "large_immutable_converter",
    srcs = ["large_immutable_converter.cc"],
    hdrs = ["large_immutable_converter.h"],
    visibility = [
        "//converter:__pkg__",
        "//rewriter:__pkg__",
    ],
    deps = [
        "//base",
        "//base:logging",
        "//base:port",
        "//converter:connector",
        "//converter:immutable_converter_interface",
        "//converter:immutable_converter_no_factory",
        "//converter:segmenter",
        "//converter:segments",
        "//data_manager:data_manager_interface",
        "//data_manager/google:google_data_manager",
        "//dictionary:dictionary_impl",
        "//dictionary:dictionary_interface",
        "//dictionary:pos_group",
        "//dictionary:pos_matcher_lib",
        "//dictionary:suffix_dictionary",
        "//dictionary:suppression_dictionary",
        "//dictionary:user_dictionary_stub",
        "//dictionary/system:system_dictionary",
        "//dictionary/system:value_dictionary",
        "//prediction:suggestion_filter",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
    ],
)

cc_library_mozc(
    name = "user_data_manager_mock",
    srcs = ["user_data_manager_mock.cc"],
    hdrs = ["user_data_manager_mock.h"],
    visibility = [
        "//:__subpackages__",
    ],
    deps = [
        ":user_data_manager_interface",
        "//base",
        "//base:port",
    ],
)

cc_library_mozc(
    name = "user_data_manager_interface",
    hdrs = ["user_data_manager_interface.h"],
    visibility = [
        # For //session:session_handler_scenario_test.
        "//session:__pkg__",
    ],
)

cc_library_mozc(
    name = "minimal_engine",
    srcs = ["minimal_engine.cc"],
    hdrs = ["minimal_engine.h"],
    visibility = [
        "//:__subpackages__",
    ],
    deps = [
        ":engine_interface",
        ":user_data_manager_interface",
        "//base:port",
        "//composer",
        "//converter:converter_interface",
        "//converter:segments",
        "//data_manager",
        "//dictionary:suppression_dictionary",
        "//prediction:predictor_interface",
        "//request:conversion_request",
    ],
)
