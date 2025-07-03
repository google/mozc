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

"""Qt build rules."""

load("@build_bazel_rules_apple//apple:macos.bzl", "macos_application")
load(
    "//:build_defs.bzl",
    "mozc_cc_binary",
    "mozc_cc_library",
    "mozc_select",
)
load(
    "//:config.bzl",
    "MACOS_BUNDLE_ID_PREFIX",
    "MACOS_MIN_OS_VER",
)
load("//bazel:stubs.bzl", "register_extension_info")

def mozc_cc_qt_library(name, deps = [], **kwargs):
    mozc_cc_library(
        name = name,
        deps = deps + mozc_select(
            default = ["//third_party/qt/qt6:qt_native"],
            oss_linux = ["@qt_linux//:qt_linux"],
            oss_macos = ["@qt_mac//:qt_mac"],
            oss_windows = ["@qt_win//:qt_win"],
        ),
        **kwargs
    )

register_extension_info(
    extension = mozc_cc_qt_library,
    label_regex_for_dep = "{extension_name}",
)

def mozc_cc_qt_binary(name, deps = [], **kwargs):
    mozc_cc_binary(
        name = name,
        deps = deps + mozc_select(
            default = ["//third_party/qt/qt6:qt_native"],
            oss_linux = ["@qt_linux//:qt_linux"],
            oss_macos = ["@qt_mac//:qt_mac"],
            oss_windows = ["@qt_win//:qt_win"],
        ),
        **kwargs
    )

register_extension_info(
    extension = mozc_cc_qt_binary,
    label_regex_for_dep = "{extension_name}",
)

def mozc_qt_moc(name, srcs, outs):
    native.genrule(
        name = name,
        srcs = srcs,
        outs = outs,
        cmd = mozc_select(
            default = "$(location //third_party/qt/qt6:moc) -p $$(dirname $<) -o $@ $(SRCS)",
            oss_linux = "$(location @qt_linux//:libexec/moc) -p $$(dirname $<) -o $@ $(SRCS)",
            oss_macos = "$(location @qt_mac//:libexec/moc) -p $$(dirname $<) -o $@ $(SRCS)",
            oss_windows = "$(location @qt_win//:bin/moc.exe) -p $$(dirname $<) -o $@ $(SRCS)",
        ),
        tools = mozc_select(
            default = ["//third_party/qt/qt6:moc"],
            oss_linux = ["@qt_linux//:libexec/moc"],
            oss_macos = ["@qt_mac//:libexec/moc"],
            oss_windows = ["@qt_win//:bin/moc.exe"],
        ),
    )

def mozc_qt_uic(name, srcs, outs):
    native.genrule(
        name = name,
        srcs = srcs,
        outs = outs,
        cmd = mozc_select(
            default = "$(location //third_party/qt/qt6:uic) -o $@ $(SRCS)",
            oss_linux = "$(location @qt_linux//:libexec/uic) -o $@ $(SRCS)",
            oss_macos = "$(location @qt_mac//:libexec/uic) -o $@ $(SRCS)",
            oss_windows = "$(location @qt_win//:bin/uic.exe) -o $@ $(SRCS)",
        ),
        tools = mozc_select(
            default = ["//third_party/qt/qt6:uic"],
            oss_linux = ["@qt_linux//:libexec/uic"],
            oss_macos = ["@qt_mac//:libexec/uic"],
            oss_windows = ["@qt_win//:bin/uic.exe"],
        ),
    )

def mozc_qt_rcc(name, qrc_name, qrc_file, srcs, outs):
    native.genrule(
        name = name,
        srcs = [qrc_file] + srcs,
        outs = outs,
        cmd = mozc_select(
            default = "$(location //third_party/qt/qt6:rcc) -o $@ -name " + qrc_name + " " + qrc_file,
            oss_linux = "$(location @qt_linux//:libexec/rcc) -o $@ -name " + qrc_name + " $(location " + qrc_file + ")",
            oss_macos = "$(location @qt_mac//:libexec/rcc) -o $@ -name " + qrc_name + " $(location " + qrc_file + ")",
            oss_windows = "$(location @qt_win//:bin/rcc.exe) -o $@ -name " + qrc_name + " $(location " + qrc_file + ")",
        ),
        tools = mozc_select(
            default = ["//third_party/qt/qt6:rcc"],
            oss_linux = ["@qt_linux//:libexec/rcc"],
            oss_macos = ["@qt_mac//:libexec/rcc"],
            oss_windows = ["@qt_win//:bin/rcc.exe"],
        ),
    )

def mozc_macos_qt_application(name, bundle_name, deps):
    macos_application(
        name = name,
        tags = ["manual", "notap"],
        additional_contents = mozc_select(
            default = {},
            oss = {"@qt_mac//:libqcocoa": "Resources"},
        ),
        app_icons = ["//data/images/mac:product_icon.icns"],
        bundle_id = MACOS_BUNDLE_ID_PREFIX + ".Tool." + bundle_name,
        bundle_name = bundle_name,
        infoplists = ["//gui:mozc_tool_info_plist"],
        minimum_os_version = MACOS_MIN_OS_VER,
        resources = [
            "//data/images/mac:candidate_window_logo.tiff",
            "//gui:qt_conf",
        ],
        visibility = [
            "//:__subpackages__",
            "//:__subpackages__",
        ],
        deps = deps + mozc_select(
            default = [],
            oss = [
                "@qt_mac//:QtCore_mac",
                "@qt_mac//:QtGui_mac",
                "@qt_mac//:QtPrintSupport_mac",
                "@qt_mac//:QtWidgets_mac",
            ],
        ),
    )

register_extension_info(
    extension = mozc_macos_qt_application,
    label_regex_for_dep = "{extension_name}",
)
