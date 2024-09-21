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
  - mozc_select

See also: https://bazel.build/rules/bzl-style#rules

"""

load(
    "@bazel_tools//tools/build_defs/repo:http.bzl",
    "http_archive",
    "http_file",
)
load(
    "//:config.bzl",
    "BAZEL_TOOLS_PREFIX",
    "BRANDING",
    "MACOS_BUNDLE_ID_PREFIX",
    "MACOS_MIN_OS_VER",
    "MACOS_QT_PATH",
)
load("//bazel:run_build_tool.bzl", "mozc_run_build_tool")
load("//bazel:stubs.bzl", "pytype_strict_binary", "pytype_strict_library", "register_extension_info")
# Qt for macos
load("@//bazel:qt_mac_repository.bzl", "qt_mac_repository")

# Tags aliases for build filtering.
MOZC_TAGS = struct(
    ANDROID_ONLY = ["nolinux", "nomac", "nowin"],
    LINUX_ONLY = ["noandroid", "nomac", "nowin"],
    MAC_ONLY = ["noandroid", "nolinux", "nowin"],
    WIN_ONLY = ["noandroid", "nolinux", "nomac"],
)

def _copts_unsigned_char():
    return select({
        "//:compiler_msvc_like": ["/J"],
        "//conditions:default": ["-funsigned-char"],
    })

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
        copts = copts + _copts_unsigned_char(),
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
        copts = copts + _copts_unsigned_char(),
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
      copts: copts for cc_test.
      **kwargs: other args for cc_test.
    """
    native.cc_test(
        name = name,
        tags = tags,
        deps = deps + ["//:macro"],
        copts = copts + _copts_unsigned_char(),
        **kwargs
    )

register_extension_info(
    extension = mozc_cc_test,
    label_regex_for_dep = "{extension_name}",
)

def mozc_cc_win32_library(
        name,
        srcs = [],
        deps = [],
        hdrs = [],
        win_def_file = None,
        tags = MOZC_TAGS.WIN_ONLY,
        target_compatible_with = ["@platforms//os:windows"],
        visibility = None,
        **kwargs):
    """A rule to build an DLL import library for Win32 system DLLs.

    Args:
      name: name for cc_library.
      srcs: stub .cc files to define exported APIs.
      deps: deps to build stub .cc files.
      hdrs: header files to define exported APIs.
      win_def_file: win32 def file to define exported APIs.
      tags: optional tags.
      target_compatible_with: optional target_compatible_with.
      visibility: optional visibility.
      **kwargs: other args for cc_library.
    """

    # A DLL name, which actually will not be used in production.
    # e.g. "input_dll_fake.dll" vs "C:\Windows\System32\input.dll"
    # The actual DLL name should be specified in the LIBRARY section of
    # win_def_file.
    # https://learn.microsoft.com/en-us/cpp/build/reference/library
    cc_binary_target_name = name + "_fake.dll"
    filegroup_target_name = name + "_lib"
    cc_import_taget_name = name + "_import"

    mozc_cc_binary(
        name = cc_binary_target_name,
        srcs = srcs,
        deps = deps,
        win_def_file = win_def_file,
        linkshared = 1,
        tags = tags,
        target_compatible_with = target_compatible_with,
        visibility = ["//visibility:private"],
        **kwargs
    )

    native.filegroup(
        name = filegroup_target_name,
        srcs = [":" + cc_binary_target_name],
        output_group = "interface_library",
        tags = tags,
        target_compatible_with = target_compatible_with,
        visibility = ["//visibility:private"],
    )

    native.cc_import(
        name = cc_import_taget_name,
        interface_library = ":" + filegroup_target_name,
        shared_library = ":" + cc_binary_target_name,
        tags = tags,
        target_compatible_with = target_compatible_with,
        visibility = ["//visibility:private"],
    )

    mozc_cc_library(
        name = name,
        hdrs = hdrs,
        deps = [":" + cc_import_taget_name],
        tags = tags,
        target_compatible_with = target_compatible_with,
        visibility = visibility,
    )

register_extension_info(
    extension = mozc_cc_win32_library,
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

def _snake_case_camel_case(id):
    # Don't capitalize if it's just one word.
    if id.find("_") < 0:
        return id
    components = id.split("_")
    return "".join([s.capitalize() for s in components])

def _win_executable_transition_impl(
        settings,  # @unused
        attr):
    features = []
    if attr.static_crt:
        features = ["static_link_msvcrt"]
    return {
        "//command_line_option:features": features,
        "//command_line_option:cpu": attr.cpu,
    }

_win_executable_transition = transition(
    implementation = _win_executable_transition_impl,
    inputs = [],
    outputs = [
        "//command_line_option:features",
        "//command_line_option:cpu",
    ],
)

def _mozc_win_build_rule_impl(ctx):
    input_file = ctx.file.target
    output = ctx.actions.declare_file(
        ctx.label.name + "." + input_file.extension,
    )
    if input_file.path == output.path:
        fail("input=%d and output=%d are the same." % (input_file.path, output.path))

    # Create a symlink as we do not need to create an actual copy.
    ctx.actions.symlink(
        output = output,
        target_file = input_file,
        is_executable = True,
    )
    return [DefaultInfo(
        files = depset([output]),
        executable = output,
    )]

# The follwoing CPU values are mentioned in https://bazel.build/configure/windows#build_cpp
CPU = struct(
    ARM64 = "arm64_windows",  # aarch64 (64-bit) environment
    X64 = "x64_windows",  # x86-64 (64-bit) environment
    X86 = "x64_x86_windows",  # x86 (32-bit) environment
)

# A custom rule to reference the given build target with the given build configurations.
#
# For instance, the following rule creates a target "my_target" with setting "cpu" as "x64_windows"
# and setting "static_link_msvcrt" feature.
#
#   mozc_win_build_rule(
#       name = "my_target",
#       cpu = CPU.X64,
#       static_crt = True,
#       target = "//bath/to/target:my_target",
#   )
#
# See the following page for the details on transition.
# https://bazel.build/rules/lib/builtins/transition
mozc_win_build_rule = rule(
    implementation = _mozc_win_build_rule_impl,
    cfg = _win_executable_transition,
    attrs = {
        "_allowlist_function_transition": attr.label(
            default = BAZEL_TOOLS_PREFIX + "//tools/allowlists/function_transition_allowlist",
        ),
        "target": attr.label(
            allow_single_file = [".dll", ".exe"],
            doc = "the actual Bazel target to be built.",
            mandatory = True,
        ),
        "static_crt": attr.bool(
            default = False,
        ),
        "cpu": attr.string(
            default = "x64_windows",
        ),
    },
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

def mozc_select_enable_supplemental_model(on = [], off = []):
    return select({
        "//:enable_spellchecker": on,
        "//conditions:default": off,
    })

def mozc_select_enable_usage_rewriter(on = [], off = []):
    return select({
        "//:enable_usage_rewriter": on,
        "//conditions:default": off,
    })

def macos_deps():
    http_archive(
        name = "bazel_features",
        sha256 = "5d7e4eb0bb17aee392143cd667b67d9044c270a9345776a5e5a3cccbc44aa4b3",
        strip_prefix = "bazel_features-1.13.0",
        url = "https://github.com/bazel-contrib/bazel_features/releases/download/v1.13.0/bazel_features-v1.13.0.tar.gz",
    )
    http_archive(
        name = "rules_proto",
        sha256 = "303e86e722a520f6f326a50b41cfc16b98fe6d1955ce46642a5b7a67c11c0f5d",
        strip_prefix = "rules_proto-6.0.0",
        url = "https://github.com/bazelbuild/rules_proto/releases/download/6.0.0/rules_proto-6.0.0.tar.gz",
    )

    # Bazel macOS build (3.5.1 2024-04-09)
    # Note, versions after 3.5.1 result a build failure of universal binary.
    # https://github.com/bazelbuild/rules_apple
    http_archive(
        name = "build_bazel_rules_apple",
        sha256 = "b4df908ec14868369021182ab191dbd1f40830c9b300650d5dc389e0b9266c8d",
        url = "https://github.com/bazelbuild/rules_apple/releases/download/3.5.1/rules_apple.3.5.1.tar.gz",
    )
    
    http_archive(
        name = "build_bazel_rules_swift",
        url = "https://github.com/bazelbuild/rules_swift/releases/download/1.18.0/rules_swift.1.18.0.tar.gz",
        sha256 = "bb01097c7c7a1407f8ad49a1a0b1960655cf823c26ad2782d0b7d15b323838e2",
    	patches = ["@//bazel:swift_extras.patch"],
    )

    # Google Toolbox for Mac
    # https://github.com/google/google-toolbox-for-mac
    # We just need UnitTesting, so strip to the directory and skip dependencies.
    GTM_GIT_SHA="8fbaae947b87c1e66c0934493168fc6d583ed889"
    http_archive(
        name = "google_toolbox_for_mac",
        urls = [
            "https://github.com/google/google-toolbox-for-mac/archive/%s.zip" % GTM_GIT_SHA
        ],
        strip_prefix = "google-toolbox-for-mac-%s/UnitTesting" % GTM_GIT_SHA,
        build_file = "@//bazel:BUILD.google_toolbox_for_mac.bazel",
        patches = ["@//:google_toolbox_for_mac.patch"],
        patch_args = ["-p2"],
    )

    # Qt for macOS
    qt_mac_repository(
        name = "qt_mac",
        default_path = MACOS_QT_PATH,  # can be replaced with MOZC_QT_PATH envvar.
    )

def android_deps():
    # Java rules (7.9.0 2024-08-13)
    # https://github.com/bazelbuild/rules_java
    http_archive(
        name = "rules_java",
        sha256 = "41131de4417de70b9597e6ebd515168ed0ba843a325dc54a81b92d7af9a7b3ea",
        urls = ["https://github.com/bazelbuild/rules_java/releases/download/7.9.0/rules_java-7.9.0.tar.gz"],
    )

    # Android NDK setup (0.1.2 2024-07-23)
    http_archive(
        name = "rules_android_ndk",
        sha256 = "65aedff0cd728bee394f6fb8e65ba39c4c5efb11b29b766356922d4a74c623f5",
        strip_prefix = "rules_android_ndk-0.1.2",
        url = "https://github.com/bazelbuild/rules_android_ndk/releases/download/v0.1.2/rules_android_ndk-v0.1.2.tar.gz",
        patches = ["bazel/rules_android_ndk.patch"],
    )

    # Android SDK rules (0.5.1 2024-08-06)
    # https://github.com/bazelbuild/rules_android
    http_archive(
        name = "rules_android",
        url = "https://github.com/bazelbuild/rules_android/releases/download/v0.5.1/rules_android-v0.5.1.tar.gz",
        strip_prefix = "rules_android-0.5.1",
        sha256 = "b1599e4604c1594a1b0754184c5e50f895a68f444d1a5a82b688b2370d990ba0",
    )

def load_platform_specific_deps():
    _impl = select({
        "//bazel/cc_target_os:oss_macos": macos_deps(),
        "//bazel/cc_target_os:oss_android": android_deps(),
    })
