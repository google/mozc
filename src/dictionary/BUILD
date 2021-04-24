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

# Description:
#   The implementation of dictionaries used in Mozc.

load(
    "//:build_defs.bzl",
    "cc_binary_mozc",
    "cc_library_mozc",
    "cc_test_mozc",
    "py_binary_mozc",
    "py_library_mozc",
    "select_mozc",
)

cc_library_mozc(
    name = "dictionary_interface",
    hdrs = ["dictionary_interface.h"],
    visibility = [
        # For //converter:converter_impl.
        "//converter:__pkg__",
        # For //dictionary/system:system_dictionary.
        "//dictionary/system:__pkg__",
        # For //engine:small_immutable_converter.
        "//engine:__pkg__",
        # For //prediction:dictionary_predictor.
        "//prediction:__pkg__",
        # For //rewriter:language_aware_rewriter.
        "//rewriter:__pkg__",
    ],
    deps = [
        ":dictionary_token",
        "//base:port",
        "//request:conversion_request",
        "@com_google_absl//absl/strings",
    ],
)

cc_test_mozc(
    name = "dictionary_impl_test",
    size = "small",
    srcs = ["dictionary_impl_test.cc"],
    requires_full_emulation = False,
    deps = [
        ":dictionary_impl",
        ":dictionary_interface",
        ":dictionary_token",
        ":pos_matcher_lib",
        ":suffix_dictionary",
        ":suppression_dictionary",
        ":user_dictionary_stub",
        "//base",
        "//base:port",
        "//base:system_util",
        "//base:util",
        "//config:config_handler",
        "//converter:node",
        "//converter:node_allocator",
        "//data_manager/testing:mock_data_manager",
        "//dictionary/system:system_dictionary",
        "//dictionary/system:value_dictionary",
        "//protocol:config_proto",
        "//request:conversion_request",
        "//testing:gunit_main",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
    ],
)

cc_library_mozc(
    name = "dictionary_impl",
    srcs = [
        "dictionary_impl.cc",
    ],
    hdrs = [
        "dictionary_impl.h",
    ],
    visibility = ["//:__subpackages__"],
    deps = [
        ":dictionary_interface",
        ":dictionary_token",
        ":pos_matcher_lib",
        ":suppression_dictionary",
        "//base",
        "//base:logging",
        "//base:port",
        "//base:util",
        "//protocol:commands_proto",
        "//protocol:config_proto",
        "//usage_stats",
        "@com_google_absl//absl/strings",
    ],
)

cc_library_mozc(
    name = "suffix_dictionary",
    srcs = ["suffix_dictionary.cc"],
    hdrs = ["suffix_dictionary.h"],
    visibility = ["//:__subpackages__"],
    deps = [
        ":dictionary_interface",
        ":dictionary_token",
        "//base",
        "//base:logging",
        "//base:port",
        "//base:serialized_string_array",
        "//base:util",
        "@com_google_absl//absl/strings",
    ],
)

cc_library_mozc(
    name = "user_dictionary",
    srcs = [
        "user_dictionary.cc",
    ],
    hdrs = [
        "user_dictionary.h",
    ],
    visibility = ["//:__subpackages__"],
    deps = [
        ":dictionary_interface",
        ":dictionary_token",
        ":pos_matcher_lib",
        ":suppression_dictionary",
        ":user_dictionary_storage",
        ":user_dictionary_util",
        ":user_pos",
        ":user_pos_interface",
        "//base",
        "//base:file_util",
        "//base:hash",
        "//base:logging",
        "//base:mutex",
        "//base:port",
        "//base:singleton",
        "//base:stl_util",
        "//base:thread",
        "//base:util",
        "//config:config_handler",
        "//protocol:config_proto",
        "//protocol:user_dictionary_storage_proto",
        "//usage_stats",
        "@com_google_absl//absl/strings",
    ],
)

cc_test_mozc(
    name = "user_dictionary_test",
    size = "small",
    srcs = [
        "user_dictionary_test.cc",
    ],
    requires_full_emulation = False,
    deps = [
        ":dictionary_test_util",
        ":dictionary_token",
        ":pos_matcher_lib",
        ":suppression_dictionary",
        ":user_dictionary",
        ":user_dictionary_storage",
        ":user_pos",
        ":user_pos_interface",
        "//base:file_util",
        "//base:logging",
        "//base:port",
        "//base:singleton",
        "//base:util",
        "//config:config_handler",
        "//data_manager/testing:mock_data_manager",
        "//protocol:config_proto",
        "//request:conversion_request",
        "//testing:gunit_main",
        "//testing:mozctest",
        "//usage_stats",
        "//usage_stats:usage_stats_testing_util",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
    ],
)

cc_library_mozc(
    name = "user_dictionary_stub",
    hdrs = ["user_dictionary_stub.h"],
    visibility = ["//:__subpackages__"],
    deps = [
        ":dictionary_interface",
        "@com_google_absl//absl/strings",
    ],
)

# TODO(team): move this rule into dictionary/system.
cc_library_mozc(
    name = "dictionary_token",
    hdrs = ["dictionary_token.h"],
    visibility = [
        "//converter:__subpackages__",
        "//dictionary/system:__subpackages__",
        # For //prediction:dictionary_predictor.
        "//prediction:__pkg__",
    ],
    deps = ["//base:port"],
)

cc_library_mozc(
    name = "pos_group",
    hdrs = ["pos_group.h"],
    visibility = ["//:__subpackages__"],
    deps = ["//base:port"],
)

py_library_mozc(
    name = "gen_pos_map_lib",
    srcs = ["gen_pos_map.py"],
    deps = [
        "//build_tools:code_generator_util",
    ],
)

py_binary_mozc(
    name = "gen_pos_map",
    srcs = ["gen_pos_map.py"],
    deps = [
        ":gen_pos_map_lib",
        "//build_tools:code_generator_util",
    ],
)

genrule(
    name = "pos_map",
    srcs = [
        "//data/rules:user_pos.def",
        "//data/rules:third_party_pos_map.def",
    ],
    outs = ["pos_map.inc"],
    cmd = (
        "$(location :gen_pos_map) " +
        "--user_pos_file=" +
        "$(location //data/rules:user_pos.def) " +
        "--third_party_pos_map_file=" +
        "$(location //data/rules:third_party_pos_map.def) " +
        "--output=$@"
    ),
    exec_tools = [":gen_pos_map"],
)

cc_library_mozc(
    name = "user_dictionary_session",
    srcs = ["user_dictionary_session.cc"],
    hdrs = ["user_dictionary_session.h"],
    visibility = ["//gui:__subpackages__"],
    deps = [
        ":user_dictionary_importer",
        ":user_dictionary_storage",
        ":user_dictionary_util",
        "//base",
        "//base:logging",
        "//base:port",
        "//base/protobuf",
        "//base/protobuf:repeated_field",
        "//protocol:user_dictionary_storage_proto",
    ],
)

cc_test_mozc(
    name = "user_dictionary_session_test",
    size = "medium",
    srcs = ["user_dictionary_session_test.cc"],
    requires_full_emulation = False,
    deps = [
        ":user_dictionary_session",
        ":user_dictionary_storage",
        "//base",
        "//base:file_util",
        "//base:system_util",
        "//protocol:user_dictionary_storage_proto",
        "//testing:gunit_main",
        "//testing:testing_util",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/strings",
    ],
)

cc_library_mozc(
    name = "user_dictionary_session_handler",
    srcs = ["user_dictionary_session_handler.cc"],
    hdrs = ["user_dictionary_session_handler.h"],
    visibility = ["//:__subpackages__"],
    deps = [
        ":user_dictionary_session",
        ":user_dictionary_util",
        "//base",
        "//base:file_util",
        "//base:logging",
        "//base:port",
        "//base:util",
        "//base/protobuf",
        "//base/protobuf:repeated_field",
        "//protocol:user_dictionary_storage_proto",
        "@com_google_absl//absl/memory",
    ],
)

cc_test_mozc(
    name = "user_dictionary_session_handler_test",
    size = "small",
    srcs = ["user_dictionary_session_handler_test.cc"],
    requires_full_emulation = True,
    deps = [
        ":user_dictionary_session_handler",
        "//base:file_stream",
        "//base:file_util",
        "//base:system_util",
        "//base/protobuf",
        "//base/protobuf:repeated_field",
        "//protocol:user_dictionary_storage_proto",
        "//testing:gunit_main",
        "//testing:testing_util",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/memory",
    ],
)

cc_library_mozc(
    name = "user_dictionary_storage",
    srcs = ["user_dictionary_storage.cc"],
    hdrs = ["user_dictionary_storage.h"],
    visibility = ["//:__subpackages__"],
    deps = [
        ":user_dictionary_util",
        "//base",
        "//base:file_stream",
        "//base:file_util",
        "//base:logging",
        "//base:mutex",
        "//base:port",
        "//base:process_mutex",
        "//base:util",
        "//base/protobuf:zero_copy_stream_impl",
        "//protocol:user_dictionary_storage_proto",
    ],
)

cc_test_mozc(
    name = "user_dictionary_storage_test",
    size = "small",
    srcs = ["user_dictionary_storage_test.cc"],
    requires_full_emulation = False,
    deps = [
        ":user_dictionary_importer",
        ":user_dictionary_storage",
        ":user_dictionary_util",
        "//base",
        "//base:file_stream",
        "//base:file_util",
        "//base:logging",
        "//base:mmap",
        "//base:system_util",
        "//base:util",
        "//testing:gunit_main",
        "@com_google_absl//absl/flags:flag",
    ],
)

cc_library_mozc(
    name = "user_dictionary_util",
    srcs = ["user_dictionary_util.cc"],
    hdrs = ["user_dictionary_util.h"],
    visibility = [
        "//:__subpackages__",
    ],
    deps = [
        ":user_pos",
        ":user_pos_interface",
        "//base",
        "//base:config_file_stream",
        "//base:file_stream",
        "//base:logging",
        "//base:port",
        "//base:util",
        "//base/protobuf",
        "//base/protobuf:message",
        "//protocol:user_dictionary_storage_proto",
    ],
)

cc_test_mozc(
    name = "user_dictionary_util_test",
    size = "small",
    srcs = ["user_dictionary_util_test.cc"],
    requires_full_emulation = False,
    deps = [
        ":user_dictionary_util",
        "//base",
        "//base:util",
        "//testing:gunit_main",
        "//testing:testing_util",
    ],
)

cc_library_mozc(
    name = "user_dictionary_importer",
    srcs = [
        "user_dictionary_importer.cc",
        ":pos_map",
    ],
    hdrs = ["user_dictionary_importer.h"],
    visibility = ["//gui:__subpackages__"],
    deps = [
        ":user_dictionary",
        ":user_dictionary_util",
        "//base",
        "//base:hash",
        "//base:logging",
        "//base:mmap",
        "//base:number_util",
        "//base:port",
        "//base:system_util",
        "//base:util",
        "//base:win_util",
        "//protocol:user_dictionary_storage_proto",
        "@com_google_absl//absl/strings",
    ],
)

cc_test_mozc(
    name = "user_dictionary_importer_test",
    size = "small",
    srcs = ["user_dictionary_importer_test.cc"],
    requires_full_emulation = False,
    deps = [
        ":user_dictionary_importer",
        ":user_dictionary_storage",
        ":user_dictionary_util",
        "//base:util",
        "//testing:gunit_main",
    ],
)

# TODO(team): move this rule into dictionary/system.
cc_library_mozc(
    name = "text_dictionary_loader",
    srcs = [
        "text_dictionary_loader.cc",
    ],
    hdrs = [
        "text_dictionary_loader.h",
    ],
    visibility = [
        "//:__subpackages__",
    ],
    deps = [
        ":dictionary_token",
        ":pos_matcher_lib",
        "//base",
        "//base:file_stream",
        "//base:iterator_adapter",
        "//base:logging",
        "//base:multifile",
        "//base:number_util",
        "//base:port",
        "//base:stl_util",
        "//base:util",
        "//testing:gunit_prod",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/strings",
    ],
)

cc_test_mozc(
    name = "text_dictionary_loader_test",
    size = "small",
    srcs = [
        "text_dictionary_loader_test.cc",
    ],
    requires_full_emulation = False,
    deps = [
        ":dictionary_token",
        ":pos_matcher_lib",
        ":text_dictionary_loader",
        "//base:file_stream",
        "//base:file_util",
        "//base:util",
        "//data_manager/testing:mock_data_manager",
        "//testing:gunit_main",
        "@com_google_absl//absl/flags:flag",
    ],
)

cc_library_mozc(
    name = "user_pos",
    srcs = ["user_pos.cc"],
    hdrs = ["user_pos.h"],
    visibility = [
        "//:__subpackages__",
    ],
    deps = [
        ":user_pos_interface",
        "//base",
        "//base:logging",
        "//base:port",
        "//base:serialized_string_array",
        "//base:util",
        "//data_manager:data_manager_interface",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
    ],
)

cc_test_mozc(
    name = "user_pos_test",
    size = "small",
    srcs = ["user_pos_test.cc"],
    requires_full_emulation = False,
    deps = [
        ":user_pos",
        ":user_pos_interface",
        "//base:logging",
        "//base:util",
        "//data_manager/testing:mock_data_manager",
        "//testing:gunit_main",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
    ],
)

py_library_mozc(
    name = "pos_util",
    srcs = ["pos_util.py"],
    visibility = ["//visibility:private"],
    deps = [
        "//build_tools:code_generator_util",
    ],
)

py_library_mozc(
    name = "gen_user_pos_data_lib",
    srcs = ["gen_user_pos_data.py"],
    deps = [
        ":pos_util",
        "//build_tools:serialized_string_array_builder",
    ],
)

py_binary_mozc(
    name = "gen_user_pos_data",
    srcs = ["gen_user_pos_data.py"],
    python_version = "PY3",
    visibility = ["//:__subpackages__"],
    deps = [
        ":gen_user_pos_data_lib",
        ":pos_util",
        "//build_tools:serialized_string_array_builder",
    ],
)

py_library_mozc(
    name = "gen_pos_matcher_code_lib",
    srcs = ["gen_pos_matcher_code.py"],
    deps = [
        ":pos_util",
    ],
)

py_binary_mozc(
    name = "gen_pos_matcher_code",
    srcs = ["gen_pos_matcher_code.py"],
    visibility = ["//:__subpackages__"],
    deps = [
        ":gen_pos_matcher_code_lib",
        ":pos_util",
    ],
)

genrule(
    name = "pos_matcher",
    srcs = ["//data/rules:pos_matcher_rule.def"],
    outs = ["pos_matcher.h"],
    cmd = (
        "$(location :gen_pos_matcher_code) " +
        "--pos_matcher_rule_file=$< " +
        "--output_pos_matcher_h=$@"
    ),
    exec_tools = [":gen_pos_matcher_code"],
    visibility = [
        "//visibility:private",
    ],
)

cc_library_mozc(
    name = "pos_matcher_lib",
    hdrs = ["pos_matcher.h"],
    visibility = [
        "//:__subpackages__",
    ],
    deps = ["//base:port"],
)

py_library_mozc(
    name = "gen_pos_rewrite_rule_lib",
    srcs = ["gen_pos_rewrite_rule.py"],
)

py_binary_mozc(
    name = "gen_pos_rewrite_rule",
    srcs = ["gen_pos_rewrite_rule.py"],
    python_version = "PY3",
    visibility = ["//:__subpackages__"],
    deps = [":gen_pos_rewrite_rule_lib"],
)

cc_binary_mozc(
    name = "gen_system_dictionary_data_main",
    srcs = ["gen_system_dictionary_data_main.cc"],
    visibility = ["//:__subpackages__"],
    deps = [
        ":dictionary_token",
        ":pos_matcher_lib",
        ":text_dictionary_loader",
        "//base",
        "//base:file_stream",
        "//base:init_mozc_buildtool",
        "//base:logging",
        "//base:util",
        "//data_manager",
        "//dictionary/system:system_dictionary_builder",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/strings",
    ],
)

cc_library_mozc(
    name = "suppression_dictionary",
    srcs = ["suppression_dictionary.cc"],
    hdrs = ["suppression_dictionary.h"],
    visibility = ["//:__subpackages__"],
    deps = [
        "//base",
        "//base:logging",
        "//base:mutex",
        "//base:port",
    ],
)

cc_test_mozc(
    name = "suppression_dictionary_test",
    size = "small",
    srcs = ["suppression_dictionary_test.cc"],
    requires_full_emulation = False,
    deps = [
        ":suppression_dictionary",
        "//base",
        "//base:logging",
        "//base:thread",
        "//base:util",
        "//testing:gunit_main",
    ],
)

cc_library_mozc(
    name = "dictionary_mock",
    testonly = 1,
    srcs = ["dictionary_mock.cc"],
    hdrs = ["dictionary_mock.h"],
    visibility = ["//:__subpackages__"],
    deps = [
        ":dictionary_interface",
        ":dictionary_token",
        "//base",
        "//base:logging",
        "//base:port",
        "//base:stl_util",
        "//base:util",
        "//request:conversion_request",
        "@com_google_absl//absl/strings",
    ],
)

cc_test_mozc(
    name = "dictionary_mock_test",
    size = "small",
    srcs = ["dictionary_mock_test.cc"],
    requires_full_emulation = False,
    deps = [
        ":dictionary_mock",
        ":dictionary_test_util",
        ":dictionary_token",
        "//base:logging",
        "//base:util",
        "//request:conversion_request",
        "//testing:gunit_main",
        "@com_google_absl//absl/memory",
    ],
)

py_library_mozc(
    name = "gen_suffix_data_lib",
    srcs = ["gen_suffix_data.py"],
    deps = ["//build_tools:serialized_string_array_builder"],
)

py_binary_mozc(
    name = "gen_suffix_data",
    srcs = ["gen_suffix_data.py"],
    python_version = "PY3",
    visibility = ["//:__subpackages__"],
    deps = [
        ":gen_suffix_data_lib",
        "//build_tools:serialized_string_array_builder",
    ],
)

cc_test_mozc(
    name = "suffix_dictionary_test",
    size = "small",
    srcs = ["suffix_dictionary_test.cc"],
    requires_full_emulation = False,
    deps = [
        ":dictionary_interface",
        ":dictionary_test_util",
        ":suffix_dictionary",
        "//base",
        "//base:util",
        "//data_manager/testing:mock_data_manager",
        "//request:conversion_request",
        "//testing:gunit_main",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
    ],
)

cc_library_mozc(
    name = "dictionary_test_util",
    testonly = 1,
    srcs = ["dictionary_test_util.cc"],
    hdrs = ["dictionary_test_util.h"],
    visibility = [
        "//:__subpackages__",
    ],
    deps = [
        ":dictionary_interface",
        ":dictionary_token",
        "//base",
        "//base:util",
        "//testing",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
    ],
)

cc_library_mozc(
    name = "user_pos_interface",
    hdrs = ["user_pos_interface.h"],
    visibility = ["//data_manager:__subpackages__"],
    deps = ["//base:port"],
)

py_binary_mozc(
    name = "gen_zip_code_seed",
    srcs = [
        "gen_zip_code_seed.py",
        "zip_code_util.py",
    ],
    visibility = ["//train/zip_code:__subpackages__"],
    deps = [],
)

genrule(
    name = "zip_code_data",
    srcs = select_mozc(
        default = [
        ],
        oss = [
            "@zip_code_ken_all//:KEN_ALL.CSV",
            "@zip_code_jigyosyo//:JIGYOSYO.CSV",
        ],
    ),
    outs = ["zip_code.tsv"],
    cmd = (
        "$(location :gen_zip_code_seed)" +
        " --output $@ " + select_mozc(
            default = "",
            oss = (" --zip_code $(location @zip_code_ken_all//:KEN_ALL.CSV)" +
                   " --jigyosyo $(location @zip_code_jigyosyo//:JIGYOSYO.CSV)"),
        )
    ),
    exec_tools = [":gen_zip_code_seed"],
    visibility = ["//:__subpackages__"],
)
