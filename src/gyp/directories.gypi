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

{
  'variables': {
    # Top directory of third party libraries.
    'third_party_dir': '<(DEPTH)/third_party',
    'absl_dir': '<(third_party_dir)/abseil-cpp',
    'absl_srcdir': '<(absl_dir)/absl',
    'absl_include_dirs': ['<(absl_dir)'],

    'mozc_oss_src_dir': '<(DEPTH)',
    'mozc_src_dir': '<(DEPTH)',

    # TODO(komatsu): This can be replaced with 'android_ndk_dir'.
    'mozc_build_tools_dir': '<(abs_depth)/<(build_short_base)/mozc_build_tools',

    'proto_out_dir': '<(SHARED_INTERMEDIATE_DIR)/proto_out',

    # server_dir represents the directory where mozc_server is
    # installed. This option is only for Linux.
    'server_dir%': '/usr/lib/mozc',

    # Represents the directory where the source code of protobuf is
    # extracted. This value is ignored when 'use_libprotobuf' is 1.
    'protobuf_root': '<(third_party_dir)/protobuf',

    'gtest_base_dir': '<(third_party_dir)/gtest',
    'gtest_dir': '<(gtest_base_dir)/googletest',
    'gmock_dir': '<(gtest_base_dir)/googlemock',

    'mozc_data_dir': '<(SHARED_INTERMEDIATE_DIR)/',
    'mozc_oss_data_dir': '<(SHARED_INTERMEDIATE_DIR)/',

    # glob command to get files.
    'glob': '<(python) <(abs_depth)/gyp/glob_files.py',
  },
}
