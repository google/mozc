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

# cc_(library|binary|test) wrappers to add :macro dependency.
# :macro defines attributes for each platforms so required macros are defined by
# depending on it.

load("//tools/build_defs:build_cleaner.bzl", "register_extension_info")
load("//tools/build_defs:stubs.bzl", "pytype_strict_binary", "pytype_strict_library")
load("//tools/build_rules/android_cc_test:def.bzl", "android_cc_test")

def cc_library_mozc(deps = [], **kwargs):
    """
    cc_library wrapper adding //:macro dependecny.
    """
    native.cc_library(
        deps = deps + ["//:macro"],
        **kwargs
    )

register_extension_info(
    extension = "cc_library_mozc",
    label_regex_for_dep = "{extension_name}",
)

def cc_binary_mozc(deps = [], **kwargs):
    """
    cc_binary wrapper adding //:macro dependecny.
    """
    native.cc_binary(
        deps = deps + ["//:macro"],
        **kwargs
    )

register_extension_info(
    extension = "cc_binary_mozc",
    label_regex_for_dep = "{extension_name}",
)

def cc_test_mozc(name, tags = [], deps = [], **kwargs):
    """
    cc_test wrapper adding //:macro dependecny.
    """

    requires_full_emulation = kwargs.pop("requires_full_emulation", False)
    native.cc_test(
        name = name,
        tags = tags,
        deps = deps + ["//:macro"],
        **kwargs
    )

    if "no_android" not in tags:
        android_cc_test(
            name = name + "_android",
            # "manual" prevents this target triggered by a wild card.
            # So that "blaze test ..." does not contain this target.
            # Otherwise it is too slow.
            tags = ["manual", "notap"],
            cc_test_name = name,
            requires_full_emulation = requires_full_emulation,
        )

register_extension_info(
    extension = "cc_test_mozc",
    label_regex_for_dep = "{extension_name}",
)

def py_library_mozc(name, srcs, srcs_version = "PY3", **kwargs):
    """py_library wrapper generating import-modified python scripts for iOS."""
    pytype_strict_library(
        name = name,
        srcs = srcs,
        srcs_version = srcs_version,
        **kwargs
    )

register_extension_info(
    extension = "py_library_mozc",
    label_regex_for_dep = "{extension_name}",
)

def py_binary_mozc(name, srcs, python_version = "PY3", srcs_version = "PY3", **kwargs):
    """py_binary wrapper generating import-modified python script for iOS.

    To use this rule, corresponding py_library_mozc needs to be defined to
    generate iOS sources.
    """
    pytype_strict_binary(
        name = name,
        srcs = srcs,
        python_version = python_version,
        srcs_version = srcs_version,
        test_lib = True,
        # This main specifier is required because, without it, py_binary expects
        # that the file name of source containing main() is name.py.
        main = srcs[0],
        **kwargs
    )

register_extension_info(
    extension = "py_binary_mozc",
    label_regex_for_dep = "{extension_name}",
)

def objc_library_mozc(name, srcs = [], hdrs = [], deps = [], sdk_frameworks = [], **kwargs):
    native.objc_library(
        name = name,
        srcs = srcs,
        hdrs = hdrs,
        deps = deps + ["//:macro"],
        sdk_frameworks = sdk_frameworks,
        **kwargs
    )

def _get_value(args):
    for arg in args:
        if arg != None:
            return arg
    return None

def select_mozc(
        default = [],
        client = None,
        oss = None,
        android = None,
        ios = None,
        chromiumos = None,
        linux = None,
        macos = None,
        oss_android = None,
        oss_linux = None,
        oss_macos = None,
        wasm = None):
    """select wrapper for target os selection.

    The priority of value checking:
      android: android > client > default
      ios,chromiumos,wasm,linux: same with android.
      oss_linux: oss_linux > oss > linux > client > default

    Args:
      default: default fallback value.
      client: default value for android, ios, chromiumos, wasm and oss_linux.
        If client is not specified, default is used.
      oss: default value for OSS build.
        If oss or specific platform is not specified, client is used.
      android: value for Android build.
      ios: value for iOS build.
      chromiumos: value for ChromeOS build.
      linux: value for Linux build.
      macos: value for Linux build.
      oss_android: value for OSS Android build.
      oss_linux: value for OSS Linux build.
      oss_macos: value for OSS macOS build.
      wasm: value for wasm build.

    Returns:
      Generated select statement.
    """
    return select({
        "//tools/cc_target_os:android": _get_value([android, client, default]),
        "//tools/cc_target_os:apple": _get_value([ios, client, default]),
        "//tools/cc_target_os:chromiumos": _get_value([chromiumos, client, default]),
        "//tools/cc_target_os:wasm": _get_value([wasm, client, default]),
        "//tools/cc_target_os:darwin": _get_value([macos, ios, client, default]),
        "//tools/cc_target_os:linux": _get_value([linux, client, default]),
        "//tools/cc_target_os:oss_android": _get_value([oss_android, oss, android, client, default]),
        "//tools/cc_target_os:oss_linux": _get_value([oss_linux, oss, linux, client, default]),
        "//tools/cc_target_os:oss_macos": _get_value([oss_macos, oss, macos, ios, client, default]),
        "//conditions:default": default,
    })
