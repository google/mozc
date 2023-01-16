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
"""

load("//bazel:stubs.bzl", "register_extension_info")
load("//bazel:stubs.bzl", "pytype_strict_binary", "pytype_strict_library")
load("//bazel:stubs.bzl", "android_cc_test")
load("//:config.bzl", "BRANDING", "MACOS_BUNDLE_ID_PREFIX", "MACOS_MIN_OS_VER")
load("@build_bazel_rules_apple//apple:apple_binary.bzl", "apple_binary")
load("@build_bazel_rules_apple//apple:macos.bzl", "macos_application", "macos_bundle")

def cc_library_mozc(deps = [], copts = [], **kwargs):
    """
    cc_library wrapper adding //:macro dependecny.
    """
    native.cc_library(
        deps = deps + ["//:macro"],
        copts = copts + ["-funsigned-char"],
        **kwargs
    )

register_extension_info(
    extension = "cc_library_mozc",
    label_regex_for_dep = "{extension_name}",
)

def cc_binary_mozc(deps = [], copts = [], **kwargs):
    """
    cc_binary wrapper adding //:macro dependecny.
    """
    native.cc_binary(
        deps = deps + ["//:macro"],
        copts = copts + ["-funsigned-char"],
        **kwargs
    )

register_extension_info(
    extension = "cc_binary_mozc",
    label_regex_for_dep = "{extension_name}",
)

def cc_test_mozc(name, tags = [], deps = [], copts = [], **kwargs):
    """cc_test wrapper adding //:macro dependecny.

    Args:
      name: name for cc_test.
      tags: targs for cc_test.
      deps: deps for cc_test.  //:macro is added.
      copts: copts for cc_test.  -funsigned-char is added.
      **kwargs: other args for cc_test.
    """

    requires_full_emulation = kwargs.pop("requires_full_emulation", False)
    native.cc_test(
        name = name,
        tags = tags,
        deps = deps + ["//:macro"],
        copts = copts + ["-funsigned-char"],
        **kwargs
    )

    if "no_android" not in tags:
        android_cc_test(
            name = name + "_android",
            cc_test_name = name,
            copts = copts + ["-funsigned-char"],
            requires_full_emulation = requires_full_emulation,
            # "manual" prevents this target triggered by a wild card.
            # So that "blaze test ..." does not contain this target.
            # Otherwise it is too slow.
            tags = ["manual", "notap"],
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

def py_binary_mozc(name, srcs, python_version = "PY3", srcs_version = "PY3", test_lib = True, **kwargs):
    """py_binary wrapper generating import-modified python script for iOS.

    To use this rule, corresponding py_library_mozc needs to be defined to
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
    extension = "py_binary_mozc",
    label_regex_for_dep = "{extension_name}",
)

def infoplist_mozc(name, srcs = [], outs = []):
    native.genrule(
        name = name,
        srcs = srcs + ["//base:mozc_version_txt"],
        outs = outs,
        cmd = ("$(location //build_tools:tweak_info_plist)" +
               " --output $@" +
               " --input $(location " + srcs[0] + ")" +
               " --version_file $(location //base:mozc_version_txt)" +
               " --branding " + BRANDING),
        exec_tools = ["//build_tools:tweak_info_plist"],
    )

def infoplist_strings_mozc(name, srcs = [], outs = []):
    native.genrule(
        name = name,
        srcs = srcs,
        outs = outs,
        cmd = ("$(location //build_tools:tweak_info_plist_strings)" +
               " --output $@" +
               " --input $(location " + srcs[0] + ")" +
               " --branding " + BRANDING),
        exec_tools = ["//build_tools:tweak_info_plist_strings"],
    )

def objc_library_mozc(
        name,
        srcs = [],
        hdrs = [],
        textual_hdrs = [],
        deps = [],
        proto_deps = [],
        copts = [],
        sdk_frameworks = [],
        tags = [],
        **kwargs):
    # Because proto_library cannot be in deps of objc_library,
    # cc_library as a wrapper is necessary as a workaround.
    proto_deps_name = name + "_proto_deps"
    native.cc_library(
        name = proto_deps_name,
        deps = proto_deps,
        copts = copts + ["-funsigned-char"],
    )
    sdk_frameworks_deps = ["//third_party/apple_frameworks:" + name for name in sdk_frameworks]
    native.objc_library(
        name = name,
        srcs = srcs,
        hdrs = hdrs,
        textual_hdrs = textual_hdrs + proto_deps,
        deps = deps + [
            "//:macro",
            proto_deps_name,
        ] + select_mozc(
            macos = sdk_frameworks_deps,
            oss_macos = [],
        ),
        copts = copts + ["-funsigned-char", "-std=c++17"],
        sdk_frameworks = select_mozc(oss_macos = sdk_frameworks),
        # The 'manual' tag excludes this from the targets of 'all' and '...'.
        # This is a workaround to exclude objc_library rules from Linux build
        # because target_compatible_with doesn't work as expected.
        # https://github.com/bazelbuild/bazel/issues/12897
        tags = tags + ["manual"],
        **kwargs
    )

def _run_test_impl(ctx):
    """Rule to run executables (e.g. apple_binary) as test."""

    # Relative path from the executable to the build root.
    base_dir = "/".join([".."] * ctx.outputs.executable.path.count("/"))

    # ":" is noop for avoiding empty commands.
    sandboxed_commands = [":"]
    commands = [":"]
    for test in ctx.files.tests:
        sandboxed_commands.append("{short_path} || err=1".format(short_path = test.short_path))
        commands.append("{base_dir}/{path} || err=1".format(base_dir = base_dir, path = test.path))

    # The directory path to the executable is different between 'blaze test' and 'blaze build'.
    # If this is called by 'blaze test', TEST_WORKSPACE is filled.
    script = """
err=0
if [ \"${{TEST_WORKSPACE}}\" != \"\" ]; then
  {sandboxed_commands}
else
  cd `dirname ${{0}}`
  {commands}
fi
exit $err
""".format(sandboxed_commands = "\n".join(sandboxed_commands), commands = "\n".join(commands))

    # Write the file to be executed by run_mozc_test.
    ctx.actions.write(
        output = ctx.outputs.executable,
        content = script,
    )

    return [DefaultInfo(runfiles = ctx.runfiles(files = ctx.files.tests))]

_run_test = rule(
    implementation = _run_test_impl,
    attrs = {
        "tests": attr.label_list(
            mandatory = True,
            allow_files = True,
        ),
    },
    test = True,
)

def objc_test_mozc(name, srcs = [], deps = [], sdk_frameworks = [], **kwargs):
    objc_library_mozc(
        name = name + "_lib",
        testonly = 1,
        srcs = srcs,
        deps = deps,
        alwayslink = 1,
        **kwargs
    )

    sdk_frameworks_deps = ["//third_party/apple_frameworks:" + name for name in sdk_frameworks]
    apple_binary(
        name = name + "_bin",
        testonly = 1,
        minimum_os_version = "10.13",
        platform_type = "macos",
        sdk_frameworks = select_mozc(oss_macos = sdk_frameworks),
        deps = [name + "_lib"] + select_mozc(
            macos = sdk_frameworks_deps,
            oss_macos = [],
        ),
        tags = ["manual"],
    )

    _run_test(
        name = name,
        tests = select_mozc(macos = [name + "_bin"]),
    )

def _tweak_infoplists(name, infoplists):
    tweaked_infoplists = []
    for i, plist in enumerate(infoplists):
        plist_name = "%s_plist%d" % (name, i)
        infoplist_mozc(
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
        infoplist_strings_mozc(
            name = string_name,
            srcs = [string],
            outs = ["tweaked/" + string],
        )
        tweaked_strings.append(string_name)
    return tweaked_strings

def macos_application_mozc(name, bundle_name, infoplists, strings = [], bundle_id = None, tags = [], **kwargs):
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
        # The 'manual' tag excludes this from the targets of 'all' and '...'.
        # This is a workaround to exclude objc_library rules from Linux build
        # because target_compatible_with doesn't work as expected.
        # https://github.com/bazelbuild/bazel/issues/12897
        tags = tags + ["manual"],
        **kwargs
    )

def macos_bundle_mozc(name, bundle_name, infoplists, strings = [], bundle_id = None, tags = [], **kwargs):
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
        # The 'manual' tag excludes this from the targets of 'all' and '...'.
        # This is a workaround to exclude objc_library rules from Linux build
        # because target_compatible_with doesn't work as expected.
        # https://github.com/bazelbuild/bazel/issues/12897
        tags = tags + ["manual"],
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
        "//conditions:default": default,
    })
