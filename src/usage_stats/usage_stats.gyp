# Copyright 2010, Google Inc.
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
    'relative_dir': 'usage_stats',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  'targets': [
    {
      'target_name': 'usage_stats',
      'type': 'static_library',
      'sources': [
        '<(proto_out_dir)/<(relative_dir)/usage_stats.pb.cc',
        'upload_util.cc',
        'usage_stats.cc',
      ],
      'dependencies': [
        '../protobuf/protobuf.gyp:protobuf',
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../storage/storage.gyp:storage',
        'genproto_usage_stats',
      ],
      'actions': [
        {
          'action_name': 'gen_usage_stats_list',
          'variables': {
            'input_files': [
              '../data/usage_stats/stats.def',
            ],
          },
          'inputs': [
            'gen_stats_list.py',
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/usage_stats_list.h',
          ],
          'action': [
            'python', '../build_tools/redirect.py',
            '<(gen_out_dir)/usage_stats_list.h',
            'gen_stats_list.py',
            '<@(input_files)',
          ],
        },
      ],
    },
    {
      'target_name': 'genproto_usage_stats',
      'type': 'none',
      'sources': [
        'usage_stats.proto',
      ],
      'includes': [
        '../protobuf/genproto.gypi',
      ],
    },
    {
      'target_name': 'usage_stats_test',
      'type': 'executable',
      'sources': [
        'upload_util_test.cc',
        'usage_stats_test.cc',
      ],
      'dependencies': [
        '../testing/testing.gyp:gtest_main',
        'usage_stats',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
  ],
}
