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

load(
    "//:build_defs.bzl",
    "cc_binary_mozc",
    "cc_library_mozc",
    "select_mozc",
)
load(
    "//:config.bzl",
    "MACOS_BUNDLE_ID_PREFIX",
    "MACOS_MIN_OS_VER",
    "QT_BIN_PATH",
)
load("@build_bazel_rules_apple//apple:macos.bzl", "macos_application")
load("@bazel_skylib//lib:paths.bzl", "paths")

def cc_qt_library_mozc(name, deps = [], **kwargs):
    cc_library_mozc(
        name = name,
        deps = deps + select_mozc(
            default = ["//third_party/qt:qt_native"],
            oss_linux = ["@io_qt//:qt"],
            oss_macos = ["@io_qt//:qt_mac"],
        ),
        **kwargs
    )

def cc_qt_binary_mozc(name, deps = [], **kwargs):
    cc_binary_mozc(
        name = name,
        deps = deps + select_mozc(
            default = ["//third_party/qt:qt_native"],
            oss_linux = ["@io_qt//:qt"],
            oss_macos = ["@io_qt//:qt_mac"],
        ),
        **kwargs
    )

def qt_moc_mozc(name, srcs, outs):
    native.genrule(
        name = name,
        srcs = srcs,
        outs = outs,
        cmd = select_mozc(
            default = "$(location //third_party/qt:moc) -p $$(dirname $<) -o $@ $(SRCS)",
            oss = paths.join(QT_BIN_PATH, "moc") + " -p $$(dirname $<) -o $@ $(SRCS)",
        ),
        tools = select_mozc(
            default = ["//third_party/qt:moc"],
            oss = [],
        ),
    )

def qt_uic_mozc(name, srcs, outs):
    native.genrule(
        name = name,
        srcs = srcs,
        outs = outs,
        cmd = select_mozc(
            default = "$(location //third_party/qt:uic) -o $@ $(SRCS)",
            oss = paths.join(QT_BIN_PATH, "uic") + " -o $@ $(SRCS)",
        ),
        tools = select_mozc(
            default = ["//third_party/qt:uic"],
            oss = [],
        ),
    )

def qt_rcc_mozc(name, qrc_name, qrc_file, srcs, outs):
    native.genrule(
        name = name,
        srcs = [qrc_file] + srcs,
        outs = outs,
        cmd = select_mozc(
            default = "$(location //third_party/qt:rcc) -o $@ -name " + qrc_name + " " + qrc_file,
            oss = paths.join(QT_BIN_PATH, "rcc") + " -o $@ -name " + qrc_name + " $(location " + qrc_file + ")",
        ),
        tools = select_mozc(
            default = ["//third_party/qt:rcc"],
            oss = [],
        ),
    )

def macos_qt_application_mozc(name, bundle_name, deps):
    macos_application(
        name = name,
        tags = ["manual"],
        additional_contents = select_mozc(
            default = {},
            oss = {"@io_qt//:libqcocoa": "Resources"},
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
        visibility = ["//:__subpackages__"],
        deps = deps + select_mozc(
            default = [],
            oss = [
                "@io_qt//:QtCore_mac",
                "@io_qt//:QtGui_mac",
                "@io_qt//:QtPrintSupport_mac",
                "@io_qt//:QtWidgets_mac",
            ],
        ),
    )
