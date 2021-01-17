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

load("//:build_defs.bzl", "cc_library_mozc")
load(
    "//tools/build_defs:qt.bzl",
    "cc_qt_binary_mozc",
    "cc_qt_library_mozc",
)

cc_library_mozc(
    name = "prelauncher_lib",
    srcs = ["prelauncher_libmain.cc"],
    deps = [
        "//base:flags",
        "//base:mac_util",
        "//client",
        "//client:client_interface",
        "//renderer:renderer_client",
    ],
)

cc_qt_library_mozc(
    name = "mozc_tool_lib",
    srcs = [
        "mozc_tool_libmain.cc",
    ],
    hdrs = [
        "mozc_tool_libmain.h",
    ],
    deps = [
        "//base:crash_report_handler",
        "//base:file_util",
        "//base:flags",
        "//base:init_mozc",
        "//base:logging",
        "//base:port",
        "//base:process",
        "//base:run_level",
        "//base:system_util",
        "//base:util",
        "//base:version",
        "//config:stats_config_util",
        "//gui/about_dialog",
        "//gui/administration_dialog",
        "//gui/base:gui_base",
        "//gui/base:singleton_window_helper",
        "//gui/config_dialog",
        "//gui/confirmation_dialog",
        "//gui/dictionary_tool",
        "//gui/error_message_dialog",
        "//gui/post_install_dialog",
        "//gui/set_default_dialog",
        "//gui/word_register_dialog",
    ],
)

cc_qt_binary_mozc(
    name = "mozc_tool",
    srcs = ["mozc_tool_main.cc"],
    visibility = ["//:__pkg__"],
    deps = [
        ":mozc_tool_lib",
        "//base:init_mozc",
        "//gui/base:gui_base",
    ],
)
