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
    'relative_dir': 'prediction',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  'targets': [
    {
      'target_name': 'prediction_test',
      'type': 'executable',
      'sources': [
        'dictionary_predictor_test.cc',
        'number_decoder_test.cc',
        'user_history_predictor_test.cc',
        'predictor_test.cc',
        'zero_query_dict_test.cc',
      ],
      'dependencies': [
        '../base/absl.gyp:absl_random',
        '../base/absl.gyp:absl_strings',
        '../base/absl.gyp:absl_time',
        '../base/base_test.gyp:clock_mock',
        '../composer/composer.gyp:composer',
        '../config/config.gyp:config_handler',
        '../converter/converter_base.gyp:connector',
        '../converter/converter_base.gyp:immutable_converter',
        '../converter/converter_base.gyp:segmenter',
        '../converter/converter_base.gyp:segments',
        '../data_manager/testing/mock_data_manager.gyp:mock_data_manager',
        '../dictionary/dictionary.gyp:dictionary',
        '../dictionary/dictionary.gyp:suffix_dictionary',
        '../dictionary/dictionary_base.gyp:pos_matcher',
        '../dictionary/system/system_dictionary.gyp:system_dictionary',
        '../dictionary/system/system_dictionary.gyp:value_dictionary',
        '../protocol/protocol.gyp:commands_proto',
        '../protocol/protocol.gyp:config_proto',
        '../session/session_base.gyp:request_test_util',
        '../storage/storage.gyp:storage',
        '../testing/testing.gyp:gtest_main',
        '../usage_stats/usage_stats_test.gyp:usage_stats_testing_util',
        'prediction.gyp:prediction',
      ],
      'variables': {
        'test_size': 'small',
      },
      'cflags': [
        '-Wno-unknown-warning-option',
        '-Wno-inconsistent-missing-override',
      ],
    },
    # Test cases meta target: this target is referred from gyp/tests.gyp
    {
      'target_name': 'prediction_all_test',
      'type': 'none',
      'dependencies': [
        'prediction_test',
      ],
    },
  ],
}
