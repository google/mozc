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
BUILD wrapper for dictionary evaluation.
"""

def evaluation(
        name,
        outs,
        data_file,
        data_type,
        engine_type,
        test_files,
        base_file,
        visibility = None,
        **kwargs):
    """Generates a dictionary evaluation tsv file."""
    evaluation_name = name + "_result"
    evaluation_out = evaluation_name + ".tsv"
    test_file_locations = ["$(location %s)" % file for file in test_files]

    native.genrule(
        name = evaluation_name,
        srcs = [data_file] + test_files,
        outs = [evaluation_out],
        testonly = 1,
        cmd = """
            LANG=en_US.UTF8

            $(location //converter:quality_regression_main) \
                --test_files="{test_file_locations}" \
                --data_file="$(location {data_file})" \
                --data_type="{data_type}" \
                --engine_type="{engine_type}" > "$@"
        """.format(
            test_file_locations = ",".join(test_file_locations),
            data_file = data_file,
            data_type = data_type,
            engine_type = engine_type,
        ),
        tags = ["manual"],
        tools = ["//converter:quality_regression_main"],
        visibility = ["//visibility:private"],
    )

    native.genrule(
        name = name,
        srcs = [
            base_file,
            evaluation_name,
            "//base:mozc_version_txt",
        ],
        outs = outs,
        testonly = 1,
        cmd = """
            $(location //converter:quality_regression) \
                --version_file="$(location //base:mozc_version_txt)" \
                --data_type="{data_type}" \
                --base="$(location {base_file})" \
                --input="$(location {evaluation_name})" \
                --output="$@"
        """.format(
            data_type = data_type,
            base_file = base_file,
            evaluation_name = evaluation_name,
        ),
        tools = ["//converter:quality_regression"],
        visibility = visibility,
        **kwargs
    )
