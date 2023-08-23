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

"""mozc_run_build_tool() build rule implementation.

See the mozc_run_build_tool macro document for details.

This is a workaround to Skylib's run_binary limitations with $(locations ...) expansion.
https://github.com/bazelbuild/bazel-skylib/issues/319
"""

load("@bazel_skylib//lib:dicts.bzl", "dicts")

def _build_args(options, labels):
    """Builds lists of arguments and files from options and labels lists.

    Labels with the same option name are grouped together.
    """
    d = {}
    for option, label in zip(options, labels):
        ls = d.get(option, [])
        ls.append(label)
        d[option] = ls
    args = []
    files = []
    for key in d.keys():
        args.append(key)
        for label in d[key]:
            for file in label.files.to_list():
                args.append(file.path)
                files.append(file)
    return args, files

def _mozc_run_build_tool_impl(ctx):
    in_args, in_files = _build_args(ctx.attr.src_options, ctx.attr.src_labels)
    args = [] + ctx.attr.args + in_args

    # Declare output files.
    outputs = []
    for out_option, out_name in ctx.attr.outs.items():
        args.append(out_option)
        file = ctx.actions.declare_file(out_name)
        args.append(file.path)
        outputs.append(file)

    # Positional arguments come at the end after "--".
    if ctx.attr.positional_srcs:
        args.append("--")
        for src in ctx.files.positional_srcs:
            args.append(src.path)
            in_files.append(src)

    tools, tools_manifests = ctx.resolve_tools(tools = [ctx.attr.tool])
    ctx.actions.run(
        outputs = outputs,
        inputs = in_files,
        tools = tools,
        executable = ctx.executable.tool,
        arguments = args,
        mnemonic = "MozcRunBuildTool",
        env = dicts.add(ctx.configuration.default_shell_env, ctx.attr.env),
        input_manifests = tools_manifests,
        toolchain = None,
    )
    return DefaultInfo(
        files = depset(outputs),
        runfiles = ctx.runfiles(files = outputs),
    )

_mozc_run_build_tool = rule(
    implementation = _mozc_run_build_tool_impl,
    doc = "Runs a build tool as a build action. To be used by mozc_run_build_tool",
    attrs = {
        "tool": attr.label(
            doc = "The build tool to run. Must be an executable file or a _binary rule.",
            executable = True,
            allow_files = True,
            mandatory = True,
            cfg = "exec",
        ),
        "args": attr.string_list(
            doc = "Other command line options to pass to the tool.",
        ),
        "env": attr.string_dict(
            doc = "Environment variables of the action.",
        ),
        "positional_srcs": attr.label_list(
            allow_files = True,
            doc = "Input files to pass to the tool as positional arguments.",
        ),
        "src_options": attr.string_list(
            doc = "List of options for each input file. Must have the same size as src_labels.",
        ),
        "src_labels": attr.label_list(
            allow_files = True,
            doc = "Input files for the tool. Each label is passed with the corresponding item in" +
                  "src_options.",
        ),
        "outs": attr.string_dict(
            doc = "Dictionary of output options and files.",
        ),
    },
)

def _build_opt_lists(d):
    """Converts the srcs dictionary to lists of options and labels."""
    options = []
    labels = []
    for k, v in d.items():
        for label in v:
            options.append(k)
            labels.append(native.package_relative_label(label))
    return options, labels

def mozc_run_build_tool(name, tool, outs, args = [], env = {}, srcs = {}, positional_srcs = [], **kwargs):
    """Runs the specified tool as a build action. Unlike genrule, this macro works without Bash.

    The arguments are processed in a way that Python's argparse can handle.

    Args:
        name: Unique name for this rule.
        tool: The build tool to run. Must be an executable file or a _binary rule.
        outs: String dictionary of output files where keys are options and values are file names.
        args: List of optional arguments to pass to the tool.
        env: Dictionary of additional environmental variables for the command.
        srcs: Dictionary of input files where keys are options and values are a list of input
            files and labels.
        positional_srcs: List of additional input files. Passed as positional arguments after "--".
        **kwargs: Passed as it is to the internal rule. Specify general rules like visibility,
            testonly, etc.

    Example:
        mozc_run_build_tool(
            name = "name",
            srcs = {
                "--input1": ["//label/of:input"],
                "--input2": [":input2_label1", ":input2_label2"],
            },
            positional_srcs = ["label1", "label2"],
            outs = {
                "--output": "output_file.name",
            },
            env = {
                "LANG": "en_US.UTF-8",
            },
            tool = ":tool_binary_rule",
        )

    """
    src_options, src_labels = _build_opt_lists(srcs)
    _mozc_run_build_tool(
        name = name,
        tool = tool,
        args = args,
        env = env,
        src_options = src_options,
        src_labels = src_labels,
        positional_srcs = positional_srcs,
        outs = outs,
        **kwargs
    )
