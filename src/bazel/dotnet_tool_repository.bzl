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

"""Repository rule for dotnet tool."""

BUILD_TEMPLATE = """
package(
    default_visibility = ["//visibility:public"],
)

exports_files([
    "{executable}",  # version {version}
])
"""

def _dotnet_tool_repo_impl(repo_ctx):
    is_windows = repo_ctx.os.name.lower().startswith("win")
    if not is_windows:
        repo_ctx.file("BUILD.bazel", "")
        return

    repo_root = repo_ctx.path(".")
    dotnet_tool = repo_ctx.which("dotnet.exe")
    tool_name = repo_ctx.attr.tool_name
    if not tool_name:
        # In bzlmod, repo_ctx.attr.name has a prefix like "_main~_repo_rules~wix".
        # Note also that Bazel 8.0+ uses "+" instead of "~".
        # https://github.com/bazelbuild/bazel/issues/23127
        tool_name = repo_ctx.attr.name.replace("~", "+").split("+")[-1]
    version = repo_ctx.attr.version

    repo_ctx.execute([
        dotnet_tool,
        "tool",
        "install",
        tool_name,
        "--version",
        version,
        "--tool-path",
        repo_root,
    ])

    build_file_data = BUILD_TEMPLATE.format(
        executable = tool_name + ".exe",
        version = version,
    )
    repo_ctx.file("BUILD.bazel", build_file_data, executable = False)

dotnet_tool_repository = repository_rule(
    implementation = _dotnet_tool_repo_impl,
    configure = True,
    local = True,
    attrs = {
        "tool_name": attr.string(),
        "version": attr.string(mandatory = True),
    },
)
