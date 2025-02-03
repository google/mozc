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
    'relative_dir': 'engine',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  'targets': [
    {
      'target_name': 'data_loader_test',
      'type': 'executable',
      'sources': ['data_loader_test.cc'],
      'dependencies': [
        'engine.gyp:engine_builder',
        'install_data_loader_test_src',
        '<(mozc_oss_src_dir)/data_manager/testing/mock_data_manager.gyp:mock_data_manager',
        '<(mozc_oss_src_dir)/testing/testing.gyp:gtest_main',
        '<(mozc_oss_src_dir)/testing/testing.gyp:mozctest',
      ],
    },
    {
      'target_name': 'install_data_loader_test_src',
      'type': 'none',
      'copies': [
        {
          'destination': '<(mozc_oss_data_dir)/engine',
          'files': [
            'data_loader_test.cc',
          ],
        }
      ],
    },
    {
      'target_name': 'engine_internal_test',
      'type': 'executable',
      'sources': [
        'candidate_list_test.cc',
        'engine_output_test.cc',
      ],
      'dependencies': [
        '<(mozc_oss_src_dir)/base/base.gyp:base',
        '<(mozc_oss_src_dir)/protocol/protocol.gyp:commands_proto',
        '<(mozc_oss_src_dir)/protocol/protocol.gyp:config_proto',
        '<(mozc_oss_src_dir)/testing/testing.gyp:gtest_main',
        '<(mozc_oss_src_dir)/testing/testing.gyp:mozctest',
        '<(mozc_oss_src_dir)/testing/testing.gyp:testing_util',
        'engine.gyp:engine_converter',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'engine_converter_test',
      'type': 'executable',
      'sources': [
        'engine_converter_test.cc',
      ],
      'dependencies': [
        '<(mozc_oss_src_dir)/data_manager/testing/mock_data_manager.gyp:mock_data_manager',
        '<(mozc_oss_src_dir)/request/request.gyp:request_test_util',
        '<(mozc_oss_src_dir)/testing/testing.gyp:gtest_main',
        '<(mozc_oss_src_dir)/testing/testing.gyp:testing',
        '<(mozc_oss_src_dir)/testing/testing.gyp:testing_util',
        '<(mozc_oss_src_dir)/testing/testing.gyp:mozctest',
        '<(mozc_oss_src_dir)/usage_stats/usage_stats_test.gyp:usage_stats_testing_util',
        'engine.gyp:engine_converter',
      ],
    },
    {
      'target_name': 'engine_converter_stress_test',
      'type': 'executable',
      'sources': [
        'engine_converter_stress_test.cc'
      ],
      'dependencies': [
        '<(mozc_oss_src_dir)/engine/engine.gyp:mock_data_engine_factory',
        '<(mozc_oss_src_dir)/testing/testing.gyp:gtest_main',
        'engine.gyp:engine_converter',
      ],
      'variables': {
        'test_size': 'large',
      },
    },
    # Test cases meta target: this target is referred from gyp/tests.gyp
    {
      'target_name': 'engine_all_test',
      'type': 'none',
      'dependencies': [
        ## Stress tests and scenario tests are disabled,
        ## because they take long time (~100sec in total).
        ## Those tests are checked by other tools (e.g. Bazel test).
        # 'engine_converter_stress_test',
        'engine_converter_test',
        'engine_internal_test',
        'data_loader_test',
      ],
    },
  ],
}
