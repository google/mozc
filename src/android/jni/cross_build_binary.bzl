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

"""Define a macro to cross build a target."""

load("//:config.bzl", "BAZEL_TOOLS_PREFIX")

def _cross_build_binary_transition_impl(
        settings,  # @unused
        attr):
    return [{
        "//command_line_option:platforms": str(attr.platform),
        "//command_line_option:cpu": attr.cpu,
    }]

_cross_build_binary_transition = transition(
    implementation = _cross_build_binary_transition_impl,
    inputs = [],
    outputs = ["//command_line_option:platforms", "//command_line_option:cpu"],
)

def _cross_build_binary_impl(ctx):
    return DefaultInfo(files = depset(ctx.files.target))

_cross_build_binary = rule(
    implementation = _cross_build_binary_impl,
    attrs = {
        "cpu": attr.string(),
        "target": attr.label(cfg = _cross_build_binary_transition),
        "platform": attr.label(),
        "_allowlist_function_transition": attr.label(
            default = BAZEL_TOOLS_PREFIX + "//tools/allowlists/function_transition_allowlist",
        ),
    },
)

def cross_build_binary(
        name,
        target,
        cpu,
        platform,
        target_compatible_with = [],
        tags = [],
        **kwargs):
    """Define a transition target with the given platform and CPU.

      cross_build_binary(
          name = "my_target",
          target = "//path/to/target:my_target",
          platform = ":my_platform",
          cpu = "arm64-v8a",
      )

    Args:
      name: name of the target.
      target: the actual Bazel target to be built with the specified configurations.
      cpu: CPU type of the target.
      platform: the platform name to be used to build target.
      target_compatible_with: optional. Visibility for the unit test target.
      tags: optional. Tags for both the library and unit test targets.
      **kwargs: other arguments passed to mozc_objc_library.
    """

    _cross_build_binary(
        name = name,
        target = target,
        cpu = cpu,
        platform = platform,
        target_compatible_with = target_compatible_with,
        tags = tags,
        **kwargs
    )
