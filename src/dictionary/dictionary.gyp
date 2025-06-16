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
    'relative_dir': 'dictionary',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  'targets': [
    {
      'target_name': 'dictionary',
      'type': 'none',
      'dependencies': [
        '<(mozc_oss_src_dir)/base/base.gyp:base',
        'dictionary_base.gyp:user_dictionary',
        'dictionary_impl',
        'single_kanji_dictionary',
        'suffix_dictionary',
        'system/system_dictionary.gyp:system_dictionary',
        'system/system_dictionary.gyp:value_dictionary',
      ],
    },
    {
      'target_name': 'suffix_dictionary',
      'type': 'static_library',
      'sources': [
        'suffix_dictionary.cc',
      ],
      'dependencies': [
        '<(mozc_oss_src_dir)/base/absl.gyp:absl_strings',
        '<(mozc_oss_src_dir)/base/base.gyp:base',
        '<(mozc_oss_src_dir)/base/base.gyp:serialized_string_array',
        '<(mozc_oss_src_dir)/request/request.gyp:conversion_request',
      ],
    },
    {
      'target_name': 'single_kanji_dictionary',
      'type': 'static_library',
      'sources': [
        'single_kanji_dictionary.cc',
      ],
      'dependencies': [
        '<(mozc_oss_src_dir)/base/absl.gyp:absl_strings',
        '<(mozc_oss_src_dir)/base/base.gyp:base',
        '<(mozc_oss_src_dir)/base/base.gyp:serialized_string_array',
        '<(mozc_oss_src_dir)/data_manager/data_manager_base.gyp:serialized_dictionary',
      ],
    },
    {
      'target_name': 'dictionary_impl',
      'type': 'static_library',
      'sources': [
        'dictionary_impl.cc',
      ],
      'dependencies': [
        '<(mozc_oss_src_dir)/base/absl.gyp:absl_strings',
        '<(mozc_oss_src_dir)/base/base.gyp:base',
        '<(mozc_oss_src_dir)/config/config.gyp:config_handler',
        '<(mozc_oss_src_dir)/protocol/protocol.gyp:commands_proto',
        '<(mozc_oss_src_dir)/protocol/protocol.gyp:config_proto',
        '<(mozc_oss_src_dir)/protocol/protocol.gyp:user_dictionary_storage_proto',
        'pos_matcher.gyp:pos_matcher',
      ],
    },
    {
      'target_name': 'gen_system_dictionary_data_main',
      'type': 'executable',
      'toolsets': ['host'],
      'sources': [
        'gen_system_dictionary_data_main.cc',
      ],
      'dependencies': [
        '<(mozc_oss_src_dir)/base/absl.gyp:absl_base',
        '<(mozc_oss_src_dir)/base/absl.gyp:absl_strings',
        '<(mozc_oss_src_dir)/base/base.gyp:base',
        '<(mozc_oss_src_dir)/data_manager/data_manager_base.gyp:data_manager',
        'pos_matcher.gyp:pos_matcher',
        'system/system_dictionary.gyp:system_dictionary_builder',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'LargeAddressAware': '2',
        },
      },
    },
    {
      'target_name': 'dictionary_test_util',
      'type': 'static_library',
      'sources': [
        'dictionary_test_util.cc',
      ],
      'dependencies': [
        '<(mozc_oss_src_dir)/base/absl.gyp:absl_strings',
        '<(mozc_oss_src_dir)/base/base.gyp:base',
        '<(mozc_oss_src_dir)/request/request.gyp:conversion_request',
        '<(mozc_oss_src_dir)/testing/testing.gyp:testing',
      ],
    },
  ],
}
