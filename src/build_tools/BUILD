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

# Description:
#   Contains tools which are required to build mozc.

load(
    "//:build_defs.bzl",
    "py_binary_mozc",
    "py_library_mozc",
)

py_library_mozc(
    name = "replace_macros_lib",
    srcs = ["replace_macros.py"],
)

py_binary_mozc(
    name = "replace_macros",
    srcs = ["replace_macros.py"],
    deps = [":replace_macros_lib"],
)

# TODO(noriyukit): Stop using //testing/pybase to support tests on iOS.
py_test(
    name = "replace_macros_test",
    size = "small",
    srcs = ["replace_macros_test.py"],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":replace_macros_lib",
        "//testing/pybase",
    ],
)

py_library_mozc(
    name = "code_generator_util",
    srcs = ["code_generator_util.py"],
    visibility = ["//:__subpackages__"],
    deps = [],
)

# TODO(noriyukit): Stop using //testing/pybase to support tests on iOS.
py_test(
    name = "code_generator_util_test",
    size = "small",
    srcs = ["code_generator_util_test.py"],
    python_version = "PY3",
    deps = [
        ":code_generator_util",
        "//testing/pybase",
    ],
)

py_library_mozc(
    name = "mozc_version_lib",
    srcs = ["mozc_version.py"],
)

py_binary_mozc(
    name = "mozc_version",
    srcs = ["mozc_version.py"],
    visibility = ["//:__subpackages__"],
    deps = [":mozc_version_lib"],
)

# TODO(noriyukit): Stop using //testing/pybase to support tests on iOS.
py_test(
    name = "mozc_version_test",
    size = "small",
    srcs = ["mozc_version_test.py"],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":mozc_version_lib",
        "//testing/pybase",
    ],
)

py_binary_mozc(
    name = "replace_version",
    srcs = ["replace_version.py"],
    visibility = ["//:__subpackages__"],
    deps = [":mozc_version_lib"],
)

py_library_mozc(
    name = "util",
    srcs = ["util.py"],
    visibility = ["//:__subpackages__"],
    deps = [],
)

py_library_mozc(
    name = "zlib_util_lib",
    srcs = ["zlib_util.py"],
)

py_binary_mozc(
    name = "zlib_util",
    srcs = ["zlib_util.py"],
    visibility = ["//:__subpackages__"],
    deps = [":zlib_util_lib"],
)

py_library_mozc(
    name = "embed_pathname_lib",
    srcs = ["embed_pathname.py"],
)

py_binary_mozc(
    name = "embed_pathname",
    srcs = ["embed_pathname.py"],
    visibility = ["//:__subpackages__"],
    deps = [":embed_pathname_lib"],
)

py_library_mozc(
    name = "embed_file_lib",
    srcs = ["embed_file.py"],
    deps = [],
)

py_binary_mozc(
    name = "embed_file",
    srcs = ["embed_file.py"],
    visibility = ["//:__subpackages__"],
    deps = [":embed_file_lib"],
)

py_library_mozc(
    name = "serialized_string_array_builder",
    srcs = ["serialized_string_array_builder.py"],
    visibility = ["//:__subpackages__"],
    deps = [],
)

py_library_mozc(
    name = "tweak_data",
    srcs = ["tweak_data.py"],
)

py_binary_mozc(
    name = "tweak_info_plist",
    srcs = ["tweak_info_plist.py"],
    visibility = ["//:__subpackages__"],
    deps = [
        ":mozc_version_lib",
        ":tweak_data",
    ],
)

py_binary_mozc(
    name = "tweak_info_plist_strings",
    srcs = ["tweak_info_plist_strings.py"],
    visibility = ["//:__subpackages__"],
    deps = [
        ":tweak_data",
    ],
)
