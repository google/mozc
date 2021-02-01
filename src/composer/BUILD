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
    "cc_binary_mozc",
    "cc_library_mozc",
    "cc_test_mozc",
)

package(default_visibility = [
    "//:__subpackages__",
])

cc_library_mozc(
    name = "composer",
    srcs = ["composer.cc"],
    hdrs = ["composer.h"],
    deps = [
        ":composition_interface",
        ":key_event_util",
        ":table",
        ":type_corrected_query",
        "//base",
        "//base:flags",
        "//base:logging",
        "//base:port",
        "//base:util",
        "//base/protobuf",
        "//base/protobuf:repeated_field",
        "//composer/internal:composition",
        "//composer/internal:composition_input",
        "//composer/internal:mode_switching_handler",
        "//composer/internal:transliterators",
        "//composer/internal:typing_corrector",
        "//config:character_form_manager",
        "//config:config_handler",
        "//protocol:commands_proto",
        "//protocol:config_proto",
        "//testing:gunit_prod",
        "//transliteration",
    ],
)

cc_test_mozc(
    name = "composer_test",
    size = "small",
    srcs = ["composer_test.cc"],
    requires_full_emulation = False,
    deps = [
        ":composer",
        ":key_parser",
        ":table",
        "//base",
        "//base:logging",
        "//base:system_util",
        "//base:util",
        "//composer/internal:typing_model",
        "//config:character_form_manager",
        "//config:config_handler",
        "//data_manager/testing:mock_data_manager",
        "//protocol:commands_proto",
        "//protocol:config_proto",
        "//session:request_test_util",
        "//testing:gunit_main",
        "@com_google_absl//absl/strings",
    ],
)

cc_library_mozc(
    name = "table",
    srcs = ["table.cc"],
    hdrs = ["table.h"],
    deps = [
        "//base",
        "//base:config_file_stream",
        "//base:file_stream",
        "//base:hash",
        "//base:logging",
        "//base:mozc_hash_set",
        "//base:port",
        "//base:trie",
        "//base:util",
        "//composer/internal:composition_input",
        "//composer/internal:typing_model",
        "//config:config_handler",
        "//data_manager:data_manager_interface",
        "//protocol:commands_proto",
        "//protocol:config_proto",
        "//testing:gunit_prod",
    ],
)

cc_library_mozc(
    name = "composition_interface",
    hdrs = ["composition_interface.h"],
    visibility = [
        # For //composer/internal:composition.
        "//composer/internal:__pkg__",
    ],
    deps = ["//composer/internal:transliterators"],
)

cc_library_mozc(
    name = "type_corrected_query",
    hdrs = ["type_corrected_query.h"],
    visibility = [
        # For //composer/internal:typing_corrector_test.
        "//composer/internal:__pkg__",
    ],
)

cc_binary_mozc(
    name = "composer_main",
    srcs = ["composer_main.cc"],
    deps = [
        ":composer",
        ":composition_interface",
        ":table",
        "//base",
        "//base:flags",
        "//base:init_mozc",
        "//base/protobuf",
        "//protocol:commands_proto",
        "//protocol:config_proto",
        "//transliteration",
    ],
)

cc_test_mozc(
    name = "table_test",
    size = "small",
    srcs = ["table_test.cc"],
    requires_full_emulation = False,
    deps = [
        ":table",
        "//base",
        "//base:file_util",
        "//base:mozc_hash_set",
        "//base:port",
        "//base:system_util",
        "//composer/internal:composition_input",
        "//config:config_handler",
        "//data_manager/testing:mock_data_manager",
        "//protocol:commands_proto",
        "//protocol:config_proto",
        "//testing:gunit_main",
    ],
)

cc_library_mozc(
    name = "key_event_util",
    srcs = ["key_event_util.cc"],
    hdrs = ["key_event_util.h"],
    deps = [
        "//base",
        "//base:logging",
        "//base:port",
        "//protocol:commands_proto",
    ],
)

cc_test_mozc(
    name = "key_event_util_test",
    size = "small",
    srcs = ["key_event_util_test.cc"],
    requires_full_emulation = False,
    deps = [
        ":key_event_util",
        ":key_parser",
        "//base",
        "//base:util",
        "//protocol:commands_proto",
        "//testing:gunit_main",
    ],
)

cc_library_mozc(
    name = "key_parser",
    srcs = ["key_parser.cc"],
    hdrs = ["key_parser.h"],
    deps = [
        "//base",
        "//base:logging",
        "//base:port",
        "//base:singleton",
        "//base:util",
        "//protocol:commands_proto",
    ],
)

cc_test_mozc(
    name = "key_parser_test",
    size = "small",
    srcs = ["key_parser_test.cc"],
    requires_full_emulation = False,
    deps = [
        ":key_event_util",
        ":key_parser",
        "//base",
        "//base:port",
        "//protocol:commands_proto",
        "//testing:gunit_main",
    ],
)

cc_test_mozc(
    name = "composer_benchmark_test",
    size = "small",
    srcs = ["composer_benchmark_test.cc"],
    requires_full_emulation = False,
    deps = [
        ":composer",
        ":key_parser",
        ":table",
        "//base",
        "//composer/internal:typing_model",
        "//config:character_form_manager",
        "//config:config_handler",
        "//data_manager/testing:mock_data_manager",
        "//engine:engine_interface",
        "//protocol:commands_proto",
        "//protocol:config_proto",
        "//request:conversion_request",
        "//session:request_test_util",
        "//testing:gunit_main",
        "//testing:mozctest",
        "//testing/base/public:gunit_main",
        "@com_google_absl//absl/memory",
    ],
)
