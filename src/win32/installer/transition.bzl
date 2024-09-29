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

"""Define a transition rules for Windows build."""

load("//:build_defs.bzl", "MOZC_TAGS")
load("//:config.bzl", "BAZEL_TOOLS_PREFIX")

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
        "static_crt": attr.bool(),
        "cpu": attr.string(),
    },
)

# Define a transition target with the given build target with the given build configurations.
#
# For instance, the following code creates a target "my_target" with setting "cpu" as "x64_windows"
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
def mozc_win_build_target(
        name,
        target,
        cpu = CPU.X64,
        static_crt = False,
        target_compatible_with = [],
        tags = [],
        **kwargs):
    """Define a transition target with the given build target with the given build configurations.

    The following code creates a target "my_target" with setting "cpu" as "x64_windows" and setting
    "static_link_msvcrt" feature.

      mozc_win_build_target(
          name = "my_target",
          cpu = CPU.X64,
          static_crt = True,
          target = "//bath/to/target:my_target",
      )

    Args:
      name: name of the target.
      target: the actual Bazel target to be built with the specified configurations.
      cpu: CPU type of the target.
      static_crt: True if the target should be built with static CRT.
      target_compatible_with: optional. Visibility for the unit test target.
      tags: optional. Tags for both the library and unit test targets.
      **kwargs: other arguments passed to mozc_objc_library.
    """
    mandatory_target_compatible_with = [
        "@platforms//os:windows",
    ]
    for item in mandatory_target_compatible_with:
        if item not in target_compatible_with:
            target_compatible_with.append(item)

    mandatory_tags = MOZC_TAGS.WIN_ONLY
    for item in mandatory_tags:
        if item not in tags:
            tags.append(item)

    _mozc_win_build_rule(
        name = name,
        target = target,
        cpu = cpu,
        static_crt = static_crt,
        target_compatible_with = mandatory_target_compatible_with,
        tags = tags,
        **kwargs
    )
