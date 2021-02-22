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
    "info_plist_mozc",
    "macos_application_mozc",
    "objc_library_mozc",
    "py_binary_mozc",
)
load(
    "//:config.bzl",
    "BRANDING",
    "MACOS_BUNDLE_ID_PREFIX",
)

package(default_visibility = ["//:__subpackages__"])

macos_application_mozc(
    name = "mozc_macos",
    app_icons = ["//data/images/mac:product_icon.icns"],
    bundle_id = MACOS_BUNDLE_ID_PREFIX,
    bundle_name = BRANDING,
    infoplists = [":info_plist"],
    resources = [
        "//data/images/mac:direct.tiff",
        "//data/images/mac:full_ascii.tiff",
        "//data/images/mac:full_katakana.tiff",
        "//data/images/mac:half_ascii.tiff",
        "//data/images/mac:half_katakana.tiff",
        "//data/images/mac:hiragana.tiff",
        "//data/installer:credits_en.html",
    ],
    deps = ["mozc_macos_main_lib"],
)

objc_library_mozc(
    name = "common",
    hdrs = ["common.h"],
)

objc_library_mozc(
    name = "mozc_macos_main_lib",
    srcs = ["main.mm"],
    sdk_frameworks = ["InputMethodKit"],
    deps = [
        ":imk_controller",
        ":imk_server",
        "//base:crash_report_handler",
        "//base:init_mozc",
        "//base:logging",
        "//base:run_level",
        "//client",
        "//config:stats_config_util",
    ],
)

objc_library_mozc(
    name = "imk_controller",
    srcs = [
        "GoogleJapaneseInputController.mm",
    ],
    hdrs = [
        "GoogleJapaneseInputController.h",
    ],
    proto_deps = [
        "//protocol:commands_proto",
        "//protocol:config_proto",
        "//protocol:renderer_proto",
    ],
    textual_hdrs = [
        "GoogleJapaneseInputControllerInterface.h",
    ],
    deps = [
        ":common",
        ":imk_server",
        ":keycode_map",
        "//base:logging",
        "//base:mac_process",
        "//base:mac_util",
        "//base:mutex",
        "//base:util",
        "//client",
        "//ipc",
        "//renderer:renderer_client",
        "//session:ime_switch_util",
    ],
)

objc_library_mozc(
    name = "imk_server",
    srcs = ["GoogleJapaneseInputServer.mm"],
    hdrs = ["GoogleJapaneseInputServer.h"],
    proto_deps = ["//protocol:commands_proto"],
    deps = [
        ":common",
        "//base:logging",
        "//base:mutex",
    ],
)

objc_library_mozc(
    name = "keycode_map",
    srcs = ["KeyCodeMap.mm"],
    hdrs = ["KeyCodeMap.h"],
    proto_deps = ["//protocol:commands_proto"],
    textual_hdrs = [
        ":init_kanamap",
        ":init_specialkeymap",
        ":init_specialcharmap",
    ],
    deps = [
        "//base:logging",
        "//base:mutex",
    ],
)

py_binary_mozc(
    name = "generate_mapping",
    srcs = ["generate_mapping.py"],
    deps = [],
)

genrule(
    name = "init_kanamap",
    srcs = ["//data/preedit:mac-kana.tsv"],
    outs = ["init_kanamap.h"],
    cmd = ("$(location :generate_mapping) --mapname=KanaMap" +
           " --result_type='const char *' --filename $< > $@"),
    exec_tools = [":generate_mapping"],
)

genrule(
    name = "init_specialkeymap",
    srcs = ["//data/preedit:mac-specialkeys.tsv"],
    outs = ["init_specialkeymap.h"],
    cmd = ("$(location :generate_mapping) --mapname=SpecialKeyMap" +
           " --result_type=KeyEvent::SpecialKey --filename $< > $@"),
    exec_tools = [":generate_mapping"],
)

genrule(
    name = "init_specialcharmap",
    srcs = ["//data/preedit:mac-specialchars.tsv"],
    outs = ["init_specialcharmap.h"],
    cmd = ("$(location :generate_mapping) --mapname=SpecialCharMap" +
           " --key_type=unichar --result_type=KeyEvent::SpecialKey" +
           " --filename $< > $@"),
    exec_tools = [":generate_mapping"],
)

info_plist_mozc(
    name = "info_plist",
    srcs = ["Info.plist"],
    outs = [BRANDING + "Info.plist"],
)
