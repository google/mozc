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
    "cc_binary_mozc",
    "cc_library_mozc",
    "cc_test_mozc",
    "py_binary_mozc",
    "py_library_mozc",
)
load("//tools/build_defs:stubs.bzl", "portable_proto_library")

package(default_visibility = ["//:__subpackages__"])

# c.f., //protocol:portable_proto
portable_proto_library(
    name = "user_history_predictor_proto",
    config = "//protocol:portable_proto.pbtxt",
    header_outs = [
        "user_history_predictor.pb.h",
    ],
    proto_deps = [
        ":user_history_predictor_proto_full",
    ],
    visibility = ["//visibility:private"],
)

proto_library(
    name = "user_history_predictor_proto_full",
    srcs = ["user_history_predictor.proto"],
    visibility = ["//visibility:private"],
)

cc_library_mozc(
    name = "predictor_interface",
    hdrs = ["predictor_interface.h"],
)

cc_library_mozc(
    name = "user_history_predictor",
    srcs = ["user_history_predictor.cc"],
    hdrs = ["user_history_predictor.h"],
    deps = [
        ":predictor_interface",
        ":user_history_predictor_proto",
        "//base",
        "//base:clock",
        "//base:config_file_stream",
        "//base:flags",
        "//base:freelist",
        "//base:hash",
        "//base:logging",
        "//base:mozc_hash_set",
        "//base:thread",
        "//base:trie",
        "//base:util",
        "//composer",
        "//config:config_handler",
        "//converter:segments",
        "//dictionary:dictionary_interface",
        "//dictionary:pos_matcher_lib",
        "//dictionary:suppression_dictionary",
        "//protocol:commands_proto",
        "//protocol:config_proto",
        "//request:conversion_request",
        "//rewriter:variants_rewriter",
        "//storage:encrypted_string_storage",
        "//storage:lru_cache",
        "//testing:gunit_prod",
        "//usage_stats",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
    ],
    alwayslink = 1,
)

cc_test_mozc(
    name = "user_history_predictor_test",
    size = "small",
    timeout = "moderate",
    srcs = ["user_history_predictor_test.cc"],
    requires_full_emulation = False,
    deps = [
        ":user_history_predictor",
        "//base",
        "//base:clock_mock",
        "//base:encryptor",
        "//base:file_util",
        "//base:logging",
        "//base:port",
        "//base:system_util",
        "//base:util",
        "//composer",
        "//composer:table",
        "//config:config_handler",
        "//converter:segments",
        "//data_manager/testing:mock_data_manager",
        "//dictionary:dictionary_mock",
        "//dictionary:suppression_dictionary",
        "//protocol:commands_proto",
        "//protocol:config_proto",
        "//request:conversion_request",
        "//session:request_test_util",
        "//storage:encrypted_string_storage",
        "//testing:gunit_main",
        "//usage_stats",
        "//usage_stats:usage_stats_testing_util",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
    ],
)

cc_library_mozc(
    name = "dictionary_predictor",
    srcs = [
        "dictionary_predictor.cc",
    ],
    hdrs = ["dictionary_predictor.h"],
    deps = [
        ":predictor_interface",
        ":suggestion_filter",
        ":zero_query_dict",
        "//base",
        "//base:flags",
        "//base:logging",
        "//base:mozc_hash_map",
        "//base:number_util",
        "//base:util",
        "//composer",
        "//composer/internal:typing_corrector",
        "//config:config_handler",
        "//converter:connector",
        "//converter:converter_interface",
        "//converter:immutable_converter_interface",
        "//converter:node_list_builder",
        "//converter:segmenter",
        "//converter:segments",
        "//data_manager:data_manager_interface",
        "//dictionary:dictionary_interface",
        "//dictionary:dictionary_token",
        "//dictionary:pos_matcher_lib",
        "//protocol:commands_proto",
        "//protocol:config_proto",
        "//request:conversion_request",
        "//testing:gunit_prod",
        "//usage_stats",
        "@com_google_absl//absl/strings",
    ],
    alwayslink = 1,
)

cc_test_mozc(
    name = "dictionary_predictor_test",
    size = "small",
    timeout = "moderate",
    srcs = [
        "dictionary_predictor_test.cc",
    ],
    requires_full_emulation = False,
    deps = [
        ":dictionary_predictor",
        ":suggestion_filter",
        ":zero_query_dict",
        "//base",
        "//base:flags",
        "//base:logging",
        "//base:port",
        "//base:serialized_string_array",
        "//base:system_util",
        "//base:util",
        "//composer",
        "//composer:table",
        "//composer/internal:typing_model",
        "//config:config_handler",
        "//converter:connector",
        "//converter:converter_interface",
        "//converter:converter_mock",
        "//converter:immutable_converter_interface",
        "//converter:immutable_converter_no_factory",
        "//converter:node_allocator",
        "//converter:segmenter",
        "//converter:segments",
        "//data_manager:data_manager_interface",
        "//data_manager/testing:mock_data_manager",
        "//dictionary:dictionary_interface",
        "//dictionary:dictionary_mock",
        "//dictionary:pos_group",
        "//dictionary:pos_matcher_lib",
        "//dictionary:suffix_dictionary",
        "//dictionary:suppression_dictionary",
        "//dictionary/system:system_dictionary",
        "//dictionary/system:value_dictionary",
        "//protocol:commands_proto",
        "//protocol:config_proto",
        "//request:conversion_request",
        "//session:request_test_util",
        "//testing:gunit_main",
        "//transliteration",
        "//usage_stats",
        "//usage_stats:usage_stats_testing_util",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
    ],
)

cc_library_mozc(
    name = "predictor",
    srcs = ["predictor.cc"],
    hdrs = ["predictor.h"],
    deps = [
        ":predictor_interface",
        "//base",
        "//base:flags",
        "//base:logging",
        "//config:config_handler",
        "//converter:segments",
        "//protocol:commands_proto",
        "//protocol:config_proto",
        "//request:conversion_request",
        "@com_google_absl//absl/memory",
    ],
    alwayslink = 1,
)

cc_test_mozc(
    name = "predictor_test",
    srcs = ["predictor_test.cc"],
    requires_full_emulation = False,
    deps = [
        ":dictionary_predictor",
        ":predictor",
        ":predictor_interface",
        ":user_history_predictor",
        "//base",
        "//base:logging",
        "//base:singleton",
        "//base:system_util",
        "//composer",
        "//config:config_handler",
        "//converter:segments",
        "//data_manager/testing:mock_data_manager",
        "//dictionary:dictionary_mock",
        "//dictionary:pos_matcher_lib",
        "//dictionary:suppression_dictionary",
        "//protocol:commands_proto",
        "//protocol:config_proto",
        "//request:conversion_request",
        "//session:request_test_util",
        "//testing:gunit_main",
        "@com_google_absl//absl/memory",
    ],
)

cc_library_mozc(
    name = "suggestion_filter",
    srcs = ["suggestion_filter.cc"],
    hdrs = ["suggestion_filter.h"],
    deps = [
        "//base",
        "//base:hash",
        "//base:logging",
        "//base:port",
        "//base:util",
        "//storage:existence_filter",
    ],
)

cc_library_mozc(
    name = "zero_query_dict",
    hdrs = ["zero_query_dict.h"],
    visibility = ["//visibility:private"],
    deps = [
        "//base:port",
        "//base:serialized_string_array",
        "@com_google_absl//absl/strings",
    ],
)

cc_test_mozc(
    name = "zero_query_dict_test",
    srcs = ["zero_query_dict_test.cc"],
    requires_full_emulation = False,
    visibility = ["//visibility:private"],
    deps = [
        ":zero_query_dict",
        "//base",
        "//base:port",
        "//base:serialized_string_array",
        "//testing:gunit_main",
        "@com_google_absl//absl/strings",
    ],
)

cc_binary_mozc(
    name = "gen_suggestion_filter_main",
    srcs = ["gen_suggestion_filter_main.cc"],
    deps = [
        "//base",
        "//base:codegen_bytearray_stream",
        "//base:file_stream",
        "//base:flags",
        "//base:hash",
        "//base:init_mozc_buildtool",
        "//base:logging",
        "//base:util",
        "//storage:existence_filter",
    ],
)

py_library_mozc(
    name = "gen_zero_query_util",
    srcs = ["gen_zero_query_util.py"],
    deps = [
        "//build_tools:code_generator_util",
        "//build_tools:serialized_string_array_builder",
    ],
)

py_library_mozc(
    name = "gen_zero_query_number_data_lib",
    srcs = ["gen_zero_query_number_data.py"],
    deps = [":gen_zero_query_util"],
)

py_binary_mozc(
    name = "gen_zero_query_number_data",
    srcs = ["gen_zero_query_number_data.py"],
    python_version = "PY3",
    deps = [
        ":gen_zero_query_number_data_lib",
        ":gen_zero_query_util",
    ],
)

py_library_mozc(
    name = "gen_zero_query_data_lib",
    srcs = ["gen_zero_query_data.py"],
    deps = [
        ":gen_zero_query_util",
        "//build_tools:code_generator_util",
    ],
)

py_binary_mozc(
    name = "gen_zero_query_data",
    srcs = ["gen_zero_query_data.py"],
    python_version = "PY3",
    deps = [
        ":gen_zero_query_data_lib",
        ":gen_zero_query_util",
        "//build_tools:code_generator_util",
    ],
)
