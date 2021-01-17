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
    "//tools/build_defs:qt.bzl",
    "cc_qt_binary_mozc",
    "cc_qt_library_mozc",
    "qt_moc_mozc",
    "qt_rcc_mozc",
    "qt_uic_mozc",
)

package(
    default_visibility = ["//gui:__subpackages__"],
)

qt_moc_mozc(
    name = "moc_post_install_dialog",
    srcs = ["post_install_dialog.h"],
    outs = ["moc_post_install_dialog.cc"],
)

qt_uic_mozc(
    name = "uic_post_install_dialog",
    srcs = ["post_install_dialog.ui"],
    outs = ["ui_post_install_dialog.h"],
)

qt_rcc_mozc(
    name = "qrc_post_install_dialog",
    srcs = [
        "post_install_dialog_en.qm",
        "post_install_dialog_ja.qm",
        "//gui/base:tr_en.qm",
        "//gui/base:tr_ja.qm",
    ],
    outs = ["qrc_post_install_dialog.cc"],
    qrc_file = "post_install_dialog.qrc",
    qrc_name = "qrc_post_install_dialog",
)

cc_qt_library_mozc(
    name = "post_install_dialog",
    srcs = [
        "post_install_dialog.cc",
        "post_install_dialog_libmain.cc",
        ":moc_post_install_dialog",
        ":qrc_post_install_dialog",
    ],
    hdrs = [
        "post_install_dialog.h",
        ":uic_post_install_dialog",
    ],
    deps = [
        "//base:file_util",
        "//base:logging",
        "//base:port",
        "//base:process",
        "//base:process_mutex",
        "//base:run_level",
        "//base:system_util",
        "//base:util",
        "//base:version",
        "//gui/base:gui_base",
        "//gui/base:setup_util",
        "//gui/base:singleton_window_helper",
        "//usage_stats",
    ],
)

cc_qt_binary_mozc(
    name = "post_install_dialog_main",
    srcs = ["post_install_dialog_main.cc"],
    deps = [
        ":post_install_dialog",
        "//base:init_mozc",
        "//gui/base:gui_base",
    ],
)
