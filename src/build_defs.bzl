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

"""
cc_(library|binary|test) wrappers to add :macro dependency.
:macro defines attributes for each platforms so required macros are defined by
depending on it.

Macro naming guideline: Use mozc_ as a prefix, e.g.:
  - mozc_cc_library
  - mozc_objc_test
  - mozc_macos_application
  - mozc_select

See also: https://bazel.build/rules/bzl-style#rules

"""

load("//bazel:stubs.bzl", "register_extension_info")
load("//bazel:stubs.bzl", "pytype_strict_binary", "pytype_strict_library")
load("//:config.bzl", "BRANDING", "MACOS_BUNDLE_ID_PREFIX", "MACOS_MIN_OS_VER")
load("//bazel:run_build_tool.bzl", "mozc_run_build_tool")
load("@build_bazel_rules_apple//apple:macos.bzl", "macos_application", "macos_bundle", "macos_unit_test")

def _update_visibility(visibility = None):
    """
    Returns updated visibility. This is temporarily used for the code location migration.
    """
    if not visibility:
        return visibility
    if ("//:__subpackages__" in visibility):
        return visibility + ["//:__subpackages__"]
    return visibility

def mozc_cc_library(deps = [], copts = [], visibility = None, **kwargs):
    """
    cc_library wrapper adding //:macro dependecny.
    """
    native.cc_library(
        deps = deps + ["//:macro"],
        copts = copts + ["-funsigned-char"],
        visibility = _update_visibility(visibility),
        **kwargs
    )

register_extension_info(
    extension = mozc_cc_library,
    label_regex_for_dep = "{extension_name}",
)

def mozc_cc_binary(deps = [], copts = [], **kwargs):
    """
    cc_binary wrapper adding //:macro dependecny.
    """
    native.cc_binary(
        deps = deps + ["//:macro"],
        copts = copts + ["-funsigned-char"],
        **kwargs
    )

register_extension_info(
    extension = mozc_cc_binary,
    label_regex_for_dep = "{extension_name}",
)

def mozc_cc_test(name, tags = [], deps = [], copts = [], **kwargs):
    """cc_test wrapper adding //:macro dependecny.

    Args:
      name: name for cc_test.
      tags: targs for cc_test.
      deps: deps for cc_test.  //:macro is added.
      copts: copts for cc_test.  -funsigned-char is added.
      **kwargs: other args for cc_test.
    """
    native.cc_test(
        name = name,
        tags = tags,
        deps = deps + ["//:macro"],
        copts = copts + ["-funsigned-char"],
        **kwargs
    )

register_extension_info(
    extension = mozc_cc_test,
    label_regex_for_dep = "{extension_name}",
)

def mozc_py_library(name, srcs, srcs_version = "PY3", **kwargs):
    """py_library wrapper generating import-modified python scripts for iOS."""
    pytype_strict_library(
        name = name,
        srcs = srcs,
        srcs_version = srcs_version,
        **kwargs
    )

register_extension_info(
    extension = mozc_py_library,
    label_regex_for_dep = "{extension_name}",
)

def mozc_py_binary(name, srcs, python_version = "PY3", srcs_version = "PY3", test_lib = True, **kwargs):
    """py_binary wrapper generating import-modified python script for iOS.

    To use this rule, corresponding mozc_py_library needs to be defined to
    generate iOS sources.
    """
    pytype_strict_binary(
        name = name,
        srcs = srcs,
        python_version = python_version,
        srcs_version = srcs_version,
        test_lib = test_lib,
        # This main specifier is required because, without it, py_binary expects
        # that the file name of source containing main() is name.py.
        main = srcs[0],
        **kwargs
    )

register_extension_info(
    extension = mozc_py_binary,
    label_regex_for_dep = "{extension_name}",
)

def mozc_infoplist(name, srcs = [], outs = []):
    mozc_run_build_tool(
        name = name,
        srcs = {
            "--input": srcs,
            "--version_file": ["//base:mozc_version_txt"],
        },
        args = [
            "--branding",
            BRANDING,
        ],
        outs = {
            "--output": outs[0],
        },
        tool = "//build_tools:tweak_info_plist",
    )

def mozc_infoplist_strings(name, srcs = [], outs = []):
    mozc_run_build_tool(
        name = name,
        srcs = {
            "--input": srcs,
        },
        outs = {
            "--output": outs[0],
        },
        args = [
            "--branding",
            BRANDING,
        ],
        tool = "//build_tools:tweak_info_plist_strings",
    )

def mozc_objc_library(
        name,
        srcs = [],
        hdrs = [],
        deps = [],
        copts = [],
        tags = [],
        **kwargs):
    native.objc_library(
        name = name,
        srcs = srcs,
        hdrs = hdrs,
        deps = deps + [
            "//:macro",
        ],
        copts = copts + ["-funsigned-char", "-std=c++17"],
        target_compatible_with = select({
            "@platforms//os:macos": [],
            "@platforms//os:ios": [],
            "//conditions:default": ["@platforms//:incompatible"],
        }),
        tags = tags + ["noandroid", "nolinux", "nowin"],
        **kwargs
    )

register_extension_info(
    extension = mozc_objc_library,
    label_regex_for_dep = "{extension_name}",
)

def _snake_case_camel_case(id):
    # Don't capitalize if it's just one word.
    if id.find("_") < 0:
        return id
    components = id.split("_")
    return "".join([s.capitalize() for s in components])

def mozc_objc_test(
        name,
        bundle_id = None,
        size = None,
        visibility = None,
        tags = [],
        **kwargs):
    """A wrapper for objc_library and macos_unit_test.

    This macro internally creates two targets: mozc_objc_library and macos_unit_test because the
    macos_unit_test rule doesn't take source files directly.

    Args:
      name: name for the macos_unit_test target
      bundle_id: optional. Test bundle id for macos_unit_test. The default value is
          MACOS_BUNDLE_ID_PREFIX.package.name.CamelCasedName.
      size: optional. passed to macos_unit_test.
      visibility: optional. Visibility for the unit test target.
      tags: optional. Tags for both the library and unit test targets.
      **kwargs: other arguments passed to mozc_objc_library.
    """
    lib_name = name + "_lib"
    default_bundle_id = ".".join([
        MACOS_BUNDLE_ID_PREFIX,
        _snake_case_camel_case(native.package_name().replace("/", ".")),
        _snake_case_camel_case(name),
    ])
    mozc_objc_library(
        name = lib_name,
        testonly = True,
        alwayslink = True,
        visibility = ["//visibility:private"],
        tags = ["manual"] + tags,
        **kwargs
    )
    macos_unit_test(
        name = name,
        minimum_os_version = MACOS_MIN_OS_VER,
        bundle_id = bundle_id or default_bundle_id,
        deps = [lib_name],
        size = size,
        visibility = visibility,
        target_compatible_with = ["@platforms//os:macos"],
        tags = MOZC_TAGS.MAC_ONLY + tags,
    )

register_extension_info(
    extension = mozc_objc_test,
    label_regex_for_dep = "{extension_name}",
)

def _tweak_infoplists(name, infoplists):
    tweaked_infoplists = []
    for i, plist in enumerate(infoplists):
        plist_name = "%s_plist%d" % (name, i)
        mozc_infoplist(
            name = plist_name,
            srcs = [plist],
            outs = [plist.replace(".plist", "_tweaked.plist")],
        )
        tweaked_infoplists.append(plist_name)
    return tweaked_infoplists

def _tweak_strings(name, strings):
    tweaked_strings = []
    for i, string in enumerate(strings):
        string_name = "%s_string%d" % (name, i)
        mozc_infoplist_strings(
            name = string_name,
            srcs = [string],
            outs = ["tweaked/" + string],
        )
        tweaked_strings.append(string_name)
    return tweaked_strings

def mozc_macos_application(name, bundle_name, infoplists, strings = [], bundle_id = None, tags = [], **kwargs):
    """Rule to create .app for macOS.

    Args:
      name: name for macos_application.
      bundle_name: bundle_name for macos_application.
      infoplists: infoplists are tweaked and applied to macos_application.
      strings: strings are tweaked and applied to macos_application.
      bundle_id: bundle_id for macos_application.
      tags: tags for macos_application.
      **kwargs: other arguments for macos_application.
    """
    macos_application(
        name = name,
        bundle_id = bundle_id or (MACOS_BUNDLE_ID_PREFIX + "." + bundle_name),
        bundle_name = bundle_name,
        infoplists = _tweak_infoplists(name, infoplists),
        strings = _tweak_strings(name, strings),
        minimum_os_version = MACOS_MIN_OS_VER,
        version = "//data/version:version_macos",
        target_compatible_with = ["@platforms//os:macos"],
        tags = tags + MOZC_TAGS.MAC_ONLY,
        **kwargs
    )

def mozc_macos_bundle(name, bundle_name, infoplists, strings = [], bundle_id = None, tags = [], **kwargs):
    """Rule to create .bundle for macOS.

    Args:
      name: name for macos_bundle.
      bundle_name: bundle_name for macos_bundle.
      infoplists: infoplists are tweaked and applied to macos_bundle.
      strings: strings are tweaked and applied to macos_bundle.
      bundle_id: bundle_id for macos_bundle.
      tags: tags for macos_bundle.
      **kwargs: other arguments for macos_bundle.
    """
    macos_bundle(
        name = name,
        bundle_id = bundle_id or (MACOS_BUNDLE_ID_PREFIX + "." + bundle_name),
        bundle_name = bundle_name,
        infoplists = _tweak_infoplists(name, infoplists),
        strings = _tweak_strings(name, strings),
        minimum_os_version = MACOS_MIN_OS_VER,
        version = "//data/version:version_macos",
        target_compatible_with = ["@platforms//os:macos"],
        tags = tags + MOZC_TAGS.MAC_ONLY,
        **kwargs
    )

def _get_value(args):
    for arg in args:
        if arg != None:
            return arg
    return None

def mozc_select(
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
        oss_windows = None,
        prod = None,
        prod_linux = None,
        prod_macos = None,
        prod_windows = None,
        wasm = None,
        windows = None):
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
      macos: value for macOS build.
      oss_android: value for OSS Android build.
      oss_linux: value for OSS Linux build.
      oss_macos: value for OSS macOS build.
      oss_windows: value for OSS Windows build.
      prod: value for prod build.
      prod_linux: value for prod Linux build.
      prod_macos: value for prod macOS build.
      prod_windows: value for prod Windows build.
      wasm: value for wasm build.
      windows: value for Windows build. (placeholder)

    Returns:
      Generated select statement.
    """
    return select({
        "//bazel/cc_target_os:android": _get_value([android, client, default]),
        "//bazel/cc_target_os:apple": _get_value([ios, client, default]),
        "//bazel/cc_target_os:chromiumos": _get_value([chromiumos, client, default]),
        "//bazel/cc_target_os:darwin": _get_value([macos, ios, client, default]),
        "//bazel/cc_target_os:wasm": _get_value([wasm, client, default]),
        "//bazel/cc_target_os:windows": _get_value([windows, client, default]),
        "//bazel/cc_target_os:linux": _get_value([linux, client, default]),
        "//bazel/cc_target_os:oss_android": _get_value([oss_android, oss, android, client, default]),
        "//bazel/cc_target_os:oss_linux": _get_value([oss_linux, oss, linux, client, default]),
        "//bazel/cc_target_os:oss_macos": _get_value([oss_macos, oss, macos, ios, client, default]),
        "//bazel/cc_target_os:oss_windows": _get_value([oss_windows, oss, windows, client, default]),
        "//bazel/cc_target_os:prod_linux": _get_value([prod_linux, prod, oss_linux, oss, linux, client, default]),
        "//bazel/cc_target_os:prod_macos": _get_value([prod_macos, prod, oss_macos, oss, macos, ios, client, default]),
        "//bazel/cc_target_os:prod_windows": _get_value([prod_windows, prod, oss_windows, oss, windows, client, default]),
        "//conditions:default": default,
    })

# Macros for build config settings.
#
# These macros are syntax sugars for the Bazel select statement.

def mozc_select_enable_session_watchdog(on = [], off = []):
    return select({
        "//:enable_session_watchdog": on,
        "//conditions:default": off,
    })

def mozc_select_enable_spellchecker(on = [], off = []):
    return select({
        "//:enable_spellchecker": on,
        "//conditions:default": off,
    })

def mozc_select_enable_usage_rewriter(on = [], off = []):
    return select({
        "//:enable_usage_rewriter": on,
        "//conditions:default": off,
    })

# Tags aliases for build filtering.
MOZC_TAGS = struct(
    ANDROID_ONLY = ["nolinux", "nomac", "nowin"],
    LINUX_ONLY = ["noandroid", "nomac", "nowin"],
    MAC_ONLY = ["noandroid", "nolinux", "nowin"],
    WIN_ONLY = ["noandroid", "nolinux", "nomac"],
)
