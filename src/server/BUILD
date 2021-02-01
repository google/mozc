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
    "BRANDING",
    "MACOS_BUNDLE_ID_PREFIX",
    "MACOS_MIN_OS_VER",
    "cc_binary_mozc",
    "cc_library_mozc",
)
load("@build_bazel_rules_apple//apple:macos.bzl", "macos_application")

package(default_visibility = ["//:__subpackages__"])

cc_binary_mozc(
    name = "mozc_server",
    deps = [":mozc_server_lib"],
)

cc_library_mozc(
    name = "mozc_server_lib",
    srcs = [
        "mozc_server.cc",
        "mozc_server_main.cc",
    ],
    hdrs = [
        "mozc_server.h",
    ],
    copts = ["$(STACK_FRAME_UNLIMITED)"],  # mozc_server.cc
    deps = [
        "//base",
        "//base:crash_report_handler",
        "//base:flags",
        "//base:init_mozc",
        "//base:logging",
        "//base:port",
        "//base:process_mutex",
        "//base:run_level",
        "//base:singleton",
        "//base:system_util",
        "//base:util",
        "//config:stats_config_util",
        "//session",
        "//session:session_server",
        "@com_google_absl//absl/flags:flag",
    ],
)

cc_binary_mozc(
    name = "mozc_rpc_server_main",
    srcs = ["mozc_rpc_server_main.cc"],
    deps = [
        "//base",
        "//base:flags",
        "//base:init_mozc",
        "//base:singleton",
        "//base:system_util",
        "//engine:engine_factory",
        "//protocol:commands_proto",
        "//session",
        "//session:random_keyevents_generator",
        "//session:session_handler",
        "//session:session_usage_observer",
        "@com_google_absl//absl/flags:flag",
    ],
)

genrule(
    name = "gen_mozc_server_info_plist",
    srcs = [
        "//base:mozc_version_txt",
        "//data/mac:mozc_server_info",
    ],
    outs = ["mozc_server_info.plist"],
    cmd = ("$(location //build_tools:tweak_info_plist)" +
           " --output $@" +
           " --input $(location //data/mac:mozc_server_info)" +
           " --version_file $(location //base:mozc_version_txt)" +
           " --branding " + BRANDING),
    exec_tools = ["//build_tools:tweak_info_plist"],
)

macos_application(
    name = "mozc_server_macos",
    app_icons = ["//data/images/mac:product_icon.icns"],
    bundle_id = MACOS_BUNDLE_ID_PREFIX + ".Converter",
    bundle_name = BRANDING + "Converter",
    infoplists = [":gen_mozc_server_info_plist"],
    minimum_os_version = MACOS_MIN_OS_VER,
    deps = [":mozc_server_lib"],
    # When we support Breakpad, uncomment the following block.
    # additional_contents = {
    #     "[Breakpad]" : "Frameworks",
    # },
)
