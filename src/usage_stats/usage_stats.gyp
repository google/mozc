# Copyright 2010-2012, Google Inc.
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
        'usage_stats.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../storage/storage.gyp:storage',
        'gen_usage_stats_list#host',
        'usage_stats_protocol',
      ],
    },
    {
      'target_name': 'usage_stats_uploader',
      'type': 'static_library',
      'sources': [
        'upload_util.cc',
        'usage_stats_uploader.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:encryptor',
        '../config/config.gyp:stats_config_util',
        '../net/net.gyp:http_client',
        '../storage/storage.gyp:storage',
        'gen_usage_stats_list#host',
        'usage_stats',
        'usage_stats_protocol',
      ],
    },
    {
      'target_name': 'gen_usage_stats_list',
      'type': 'none',
      'toolsets': ['host'],
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
      'target_name': 'usage_stats_protocol',
      'type': 'static_library',
      'hard_dependency': 1,
      'sources': [
        '<(proto_out_dir)/<(relative_dir)/usage_stats.pb.cc',
      ],
      'dependencies': [
        '../protobuf/protobuf.gyp:protobuf',
        'genproto_usage_stats#host',
      ],
      'export_dependent_settings': [
        'genproto_usage_stats#host',
      ],
    },
    {
      'target_name': 'genproto_usage_stats',
      'type': 'none',
      'toolsets': ['host'],
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
        'usage_stats_test.cc',
      ],
      'dependencies': [
        '../testing/testing.gyp:gtest_main',
        'usage_stats',
        'usage_stats_protocol',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'usage_stats_uploader_test',
      'type': 'executable',
      'sources': [
        'upload_util_test.cc',
        'usage_stats_uploader_test.cc',
      ],
      'dependencies': [
        '../net/net.gyp:http_client_mock',
        '../testing/testing.gyp:gtest_main',
        'usage_stats_uploader',
        'usage_stats_protocol',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    # Test cases meta target: this target is referred from gyp/tests.gyp
    {
      'target_name': 'usage_stats_all_test',
      'type': 'none',
      'dependencies': [
        'usage_stats_test',
        'usage_stats_uploader_test',
      ],
    },
  ],
}
