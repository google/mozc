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
  'targets': [
    {
      'target_name': 'system_dictionary_codec_test',
      'type': 'executable',
      'sources': [
        'codec_test.cc',
      ],
      'dependencies': [
        '<(mozc_oss_src_dir)/base/absl.gyp:absl_random',
        '<(mozc_oss_src_dir)/base/absl.gyp:absl_strings',
        '<(mozc_oss_src_dir)/testing/testing.gyp:gtest_main',
        'system_dictionary.gyp:system_dictionary_codec',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'key_expansion_table_test',
      'type': 'executable',
      'sources': [
        'key_expansion_table_test.cc',
      ],
      'dependencies': [
        '<(mozc_oss_src_dir)/testing/testing.gyp:gtest_main',
        'system_dictionary.gyp:key_expansion_table',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'system_dictionary_test',
      'type': 'executable',
      'sources': [
        'system_dictionary_test.cc',
      ],
      'dependencies': [
        '<(mozc_oss_src_dir)/base/absl.gyp:absl_random',
        '<(mozc_oss_src_dir)/base/absl.gyp:absl_strings',
        '<(mozc_oss_src_dir)/base/base.gyp:base_core',
        '<(mozc_oss_src_dir)/config/config.gyp:config_handler',
        '<(mozc_oss_src_dir)/data_manager/oss/oss_data_manager_test.gyp:install_oss_data_manager_test_data',
        '<(mozc_oss_src_dir)/data_manager/testing/mock_data_manager.gyp:mock_data_manager',
        '<(mozc_oss_src_dir)/request/request.gyp:conversion_request',
        '<(mozc_oss_src_dir)/request/request.gyp:request_test_util',	
        '<(mozc_oss_src_dir)/testing/testing.gyp:gtest_main',
        '<(mozc_oss_src_dir)/testing/testing.gyp:mozctest',
        '<(mozc_oss_src_dir)/dictionary/dictionary.gyp:dictionary_test_util',
        'system_dictionary.gyp:system_dictionary',
        'system_dictionary.gyp:system_dictionary_builder',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'value_dictionary_test',
      'type': 'executable',
      'sources': [
        'value_dictionary_test.cc',
      ],
      'dependencies': [
        '<(mozc_oss_src_dir)/base/base.gyp:base_core',
        '<(mozc_oss_src_dir)/data_manager/testing/mock_data_manager.gyp:mock_data_manager',
        '<(mozc_oss_src_dir)/request/request.gyp:conversion_request',
        '<(mozc_oss_src_dir)/storage/louds/louds.gyp:louds_trie_builder',
        '<(mozc_oss_src_dir)/testing/testing.gyp:gtest_main',
        '<(mozc_oss_src_dir)/dictionary/dictionary.gyp:dictionary_test_util',
        'system_dictionary.gyp:value_dictionary',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    # Test cases meta target: this target is referred from gyp/tests.gyp
    {
      'target_name': 'system_dictionary_all_test',
      'type': 'none',
      'dependencies': [
        'key_expansion_table_test',
        'system_dictionary_codec_test',
        'system_dictionary_test',
        'value_dictionary_test',
      ],
    },
  ],
}
