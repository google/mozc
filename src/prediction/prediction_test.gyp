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
    'relative_dir': 'prediction',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  'targets': [
    {
      'target_name': 'prediction_test',
      'type': 'executable',
      'sources': [
        'dictionary_predictor_test.cc',
        'suggestion_filter_test.cc',
        'user_history_predictor_test.cc',
        'predictor_test.cc',
      ],
      'dependencies': [
        '../converter/converter_base.gyp:connector',
        '../converter/converter_base.gyp:immutable_converter',
        '../converter/converter_base.gyp:segmenter',
        '../converter/converter_base.gyp:segments',
        '../data_manager/data_manager.gyp:user_pos_manager',
        '../dictionary/dictionary.gyp:dictionary',
        '../dictionary/dictionary.gyp:dictionary_mock',
        '../dictionary/dictionary.gyp:suffix_dictionary',
        '../dictionary/dictionary_base.gyp:install_dictionary_test_data',
        '../dictionary/dictionary_base.gyp:pos_matcher',
        '../config/config.gyp:config_handler',
        '../session/session_base.gyp:request_handler',
        '../testing/testing.gyp:gtest_main',
        'prediction.gyp:prediction',
      ],
      'variables': {
        'test_size': 'small',
      },
      'conditions': [
        ['use_separate_connection_data==1',{
          'dependencies': [
            '../converter/converter.gyp:connection_data_injected_environment',
          ],
        }],
        ['use_separate_dictionary==1',{
          'dependencies': [
            '../dictionary/dictionary.gyp:dictionary_data_injected_environment',
          ],
        }],
        ['target_platform=="Android"', {
          'sources!': [
            # This test depends on encryptor depending on Java libaray on
            # Java, which is not available on native test.
            # I.e., we cannot run this on Android.
            'user_history_predictor_test.cc'
          ],
        }],
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
