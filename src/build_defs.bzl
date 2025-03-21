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

load("@bazel_skylib//rules:select_file.bzl", "select_file")
load("@build_bazel_rules_apple//apple:macos.bzl", "macos_application", "macos_bundle", "macos_unit_test")
load("@windows_sdk//:windows_sdk_rules.bzl", "windows_resource")
load(
    "//:config.bzl",
    "BAZEL_TOOLS_PREFIX",
    "BRANDING",
    "MACOS_BUNDLE_ID_PREFIX",
    "MACOS_MIN_OS_VER",
)
load("//bazel:run_build_tool.bzl", "mozc_run_build_tool")
load("//bazel:stubs.bzl", "pytype_strict_binary", "pytype_strict_library", "register_extension_info")

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

def _mozc_gen_win32_resource_file(
        name,
        src,
        utf8 = False):
    """
    Generates a resource file from the specified resource file and template.
    """
    args = []
    if utf8:
        args.append("--utf8")
    mozc_run_build_tool(
        name = name,
        srcs = {
            "--main": [src],
            "--template": ["//build_tools:mozc_win32_resource_template.rc"],
            "--version_file": ["//base:mozc_version_txt"],
        },
        outs = {
            "--output": name,
        },
        args = args,
        tool = "//build_tools:gen_win32_resource_header",
    )

def mozc_win32_resource_from_template(
        name,
        src,
        manifests = [],
        resources = [],
        tags = MOZC_TAGS.WIN_ONLY,
        target_compatible_with = ["@platforms//os:windows"],
        **kwargs):
    """A rule to generate Win32 resource file from a template.

    Args:
      name: name for the generated resource target.
      src: a *.rc file to be overlayed to the template.
      manifests: Win32 manifest files to be embedded.
      resources: files to be referenced from the resource file.
      tags: optional tags.
      target_compatible_with: optional target_compatible_with.
      **kwargs: other args for resource compiler.
    """
    generated_rc_file = name + "_gen.rc"
    _mozc_gen_win32_resource_file(
        generated_rc_file,
        src = src,
    )
    _rc_defines = {
        "Mozc": ["MOZC_BUILD"],
        "GoogleJapaneseInput": ["GOOGLE_JAPANESE_INPUT_BUILD"],
    }.get(BRANDING, [])

    # Create main resource
    win32_resource_files_main = windows_resource

    win32_resource_files_main(
        name = name,
        rc_files = [":" + generated_rc_file],
        manifests = manifests,
        resources = resources,
        defines = _rc_defines,
        tags = tags,
        target_compatible_with = target_compatible_with,
        **kwargs
    )

register_extension_info(
    extension = mozc_win32_resource_from_template,
    label_regex_for_dep = "{extension_name}",
)

def _append_if_absent(target_list, item):
    """Append the given item only when it is absent in the target.

    Args:
      target_list: a String list to be checked.
      item: a String item that is to be made sure to exist in the list.
    Returns:
      list: target_list itself or a new list, where the given item exists.
    """
    if item in target_list:
        return target_list
    return target_list + [item]

def _remove_if_present(target_list, item):
    """Remove the given item if exists.

    Args:
      target_list: a String list to be checked.
      item: a String item that is to be made sure to not exist in the list.
    Returns:
      list: target_list itself or a new list, where the given item does not
            exist.
    """
    return [x for x in target_list if x != item]

def _win_executable_transition_impl(
        settings,
        attr):
    features = settings["//command_line_option:features"]

    features = _append_if_absent(features, "generate_pdb_file")

    if attr.static_crt:
        features = _remove_if_present(features, "dynamic_link_msvcrt")
        features = _append_if_absent(features, "static_link_msvcrt")
    else:
        features = _remove_if_present(features, "static_link_msvcrt")
        features = _append_if_absent(features, "dynamic_link_msvcrt")

    return {
        "//command_line_option:features": features,
        "//command_line_option:platforms": [attr.platform],
    }

_win_executable_transition = transition(
    implementation = _win_executable_transition_impl,
    inputs = [
        "//command_line_option:features",
    ],
    outputs = [
        "//command_line_option:features",
        "//command_line_option:platforms",
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
    return [
        DefaultInfo(
            files = depset([output]),
            executable = output,
        ),
        OutputGroupInfo(
            pdb_file = depset(ctx.files.pdb_file),
        ),
    ]

CPU = struct(
    ARM64 = "@platforms//cpu:arm64",  # aarch64 (64-bit) environment
    X64 = "@platforms//cpu:x86_64",  # x86-64 (64-bit) environment
    X86 = "@platforms//cpu:x86_32",  # x86 (32-bit) environment
)

_mozc_win_build_rule = rule(
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
        "pdb_file": attr.label(
            allow_files = True,
            mandatory = True,
        ),
        "static_crt": attr.bool(),
        "platform": attr.label(),
    },
)

def mozc_win32_cc_prod_binary(
        name,
        executable_name_map = {},  # @unused
        srcs = [],
        deps = [],
        features = None,
        linkopts = [],
        linkshared = False,
        cpu = CPU.X64,
        static_crt = False,
        tags = MOZC_TAGS.WIN_ONLY,
        win_def_file = None,
        target_compatible_with = ["@platforms//os:windows"],
        visibility = None,
        **kwargs):
    """A rule to build production binaries for Windows.

    This wraps mozc_cc_binary so that you can specify the target CPU
    architecture and CRT linkage type in a declarative manner with also building
    a debug symbol file (*.pdb).

    Implicit output targets:
      name.pdb: A debug symbol file.

    Args:
      name: name of the target.
      executable_name_map: a map from the branding name to the executable name.
      srcs: .cc files to build the executable.
      deps: deps to build the executable.
      features: features to be passed to mozc_cc_binary.
      linkopts: linker options to build the executable.
      linkshared: True if the target is a shared library (DLL).
      cpu: optional. The target CPU architecture.
      static_crt: optional. True if the target should be built with static CRT.
      tags: optional. Tags for both the library and unit test targets.
      win_def_file: optional. win32 def file to define exported functions.
      target_compatible_with: optional. Defines target platforms.
      visibility: optional. The visibility of the target.
      **kwargs: other arguments passed to mozc_cc_binary.
    """
    target_name = executable_name_map.get(BRANDING, None)
    if target_name == None:
        return

    intermediate_name = None
    if target_name.endswith(".exe"):
        # When the targete name is "foobar.exe", then "foobar.exe.dll" will be
        # generated.
        intermediate_name = target_name
    elif target_name.endswith(".dll"):
        # When the targete name is "foobar.dll", then "foobar.pdb" will be
        # generated. To produce "foobar.dll.pdb", the target name needs to be
        # something like "foobar.dll.dll".
        intermediate_name = target_name + ".dll"
        linkshared = True
    else:
        return

    modified_linkopts = []
    modified_linkopts.extend(linkopts)
    modified_linkopts.extend([
        "/DEBUG:FULL",
        "/PDBALTPATH:%_PDB%",
    ])

    # '/CETCOMPAT' is available only on x86/x64 architectures.
    if cpu in ["@platforms//cpu:x86_32", "@platforms//cpu:x86_64"]:
        modified_linkopts.append("/CETCOMPAT")

    LOAD_LIBRARY_SEARCH_APPLICATION_DIR = 0x200
    LOAD_LIBRARY_SEARCH_SYSTEM32 = 0x800
    load_flags = LOAD_LIBRARY_SEARCH_SYSTEM32
    if not linkshared:
        # We build *.exe with dynamic CRT and deploy CRT DLLs into the
        # application dir. Thus LOAD_LIBRARY_SEARCH_APPLICATION_DIR is also
        # necessary.
        load_flags += LOAD_LIBRARY_SEARCH_APPLICATION_DIR
    modified_linkopts.append("/DEPENDENTLOADFLAG:0x%X" % load_flags)

    mozc_cc_binary(
        name = intermediate_name,
        srcs = srcs,
        deps = deps,
        features = features,
        linkopts = modified_linkopts,
        linkshared = linkshared,
        tags = tags,
        target_compatible_with = target_compatible_with,
        visibility = ["//visibility:private"],
        win_def_file = win_def_file,
        **kwargs
    )

    mandatory_target_compatible_with = [
        cpu,
        "@platforms//os:windows",
    ]
    for item in mandatory_target_compatible_with:
        if item not in target_compatible_with:
            target_compatible_with.append(item)

    mandatory_tags = MOZC_TAGS.WIN_ONLY
    for item in mandatory_tags:
        if item not in tags:
            tags.append(item)

    native.filegroup(
        name = intermediate_name + "_pdb_file",
        srcs = [intermediate_name],
        output_group = "pdb_file",
        visibility = ["//visibility:private"],
    )

    platform_name = "_" + name + "_platform"
    native.platform(
        name = platform_name,
        constraint_values = [
            cpu,
            "@platforms//os:windows",
        ],
        visibility = ["//visibility:private"],
    )

    _mozc_win_build_rule(
        name = name,
        pdb_file = intermediate_name + "_pdb_file",
        platform = platform_name,
        static_crt = static_crt,
        tags = tags,
        target = intermediate_name,
        target_compatible_with = target_compatible_with,
        visibility = visibility,
        **kwargs
    )

    native.filegroup(
        name = name + "_pdb_file",
        srcs = [name],
        output_group = "pdb_file",
        target_compatible_with = target_compatible_with,
        visibility = ["//visibility:private"],
    )

    select_file(
        name = name + ".pdb",
        srcs = name + "_pdb_file",
        subpath = target_name + ".pdb",
        visibility = visibility,
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
        # Workaround for https://github.com/google/mozc/issues/1224
        linkopts = ["/DEBUG"],
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
