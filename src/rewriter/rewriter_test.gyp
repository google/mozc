# Copyright 2010-2013, Google Inc.
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
    'relative_dir': 'rewriter',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  'targets': [
    {
      # TODO(team): split this test into individual tests.
      'target_name': 'rewriter_test',
      'type': 'executable',
      'sources': [
        'calculator_rewriter_test.cc',
        'collocation_rewriter_test.cc',
        'collocation_util_test.cc',
        'command_rewriter_test.cc',
        'correction_rewriter_test.cc',
        'date_rewriter_test.cc',
        'dice_rewriter_test.cc',
        'dictionary_generator_test.cc',
        'emoticon_rewriter_test.cc',
        'english_variants_rewriter_test.cc',
        'focus_candidate_rewriter_test.cc',
        'fortune_rewriter_test.cc',
        'merger_rewriter_test.cc',
        'normalization_rewriter_test.cc',
        'number_rewriter_test.cc',
        'remove_redundant_candidate_rewriter_test.cc',
        'rewriter_test.cc',
        'symbol_rewriter_test.cc',
        'transliteration_rewriter_test.cc',
        'unicode_rewriter_test.cc',
        'user_boundary_history_rewriter_test.cc',
        'user_dictionary_rewriter_test.cc',
        'user_segment_history_rewriter_test.cc',
        'variants_rewriter_test.cc',
        'version_rewriter_test.cc',
        'zipcode_rewriter_test.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../converter/converter.gyp:converter',
        '../converter/converter_base.gyp:converter_mock',
        '../data_manager/data_manager.gyp:user_pos_manager',
        '../data_manager/testing/mock_data_manager.gyp:mock_data_manager',
        '../engine/engine.gyp:mock_data_engine_factory',
        '../session/session_base.gyp:request_test_util',
        '../session/session_base.gyp:session_protocol',
        '../testing/testing.gyp:gtest_main',
        'calculator/calculator.gyp:calculator_mock',
        'rewriter.gyp:rewriter',
      ],
      'variables': {
        'test_size': 'small',
      },
      # TODO(horo): usage is available only in Mac and Win now.
      'conditions': [
        ['target_platform!="Android"', {
          'sources': [
            'emoji_rewriter_test.cc',
            'usage_rewriter_test.cc',
          ],
          'dependencies': [
            '../usage_stats/usage_stats_test.gyp:usage_stats_testing_util',
          ],
        }],
        ['target_platform=="NaCl" and _toolset=="target"', {
          'sources!': [
            'emoji_rewriter_test.cc',
            'user_boundary_history_rewriter_test.cc',
            'user_dictionary_rewriter_test.cc',
            'user_segment_history_rewriter_test.cc',
            'variants_rewriter_test.cc',
          ],
        }],
        ['use_packed_dictionary==1', {
          'dependencies': [
            '../data_manager/packed/packed_data_manager.gyp:gen_packed_data_header_mock#host',
          ],
          'hard_dependency': 1,
          'export_dependent_settings': [
            '../data_manager/packed/packed_data_manager.gyp:gen_packed_data_header_mock#host',
          ],
        }],
      ],
    },
    {
      'target_name': 'single_kanji_rewriter_test',
      'type': 'executable',
      'sources': [
        'single_kanji_rewriter_test.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../data_manager/testing/mock_data_manager.gyp:mock_data_manager',
        '../session/session_base.gyp:request_test_util',
        '../testing/testing.gyp:gtest_main',
        'rewriter.gyp:rewriter',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    # Test cases meta target: this target is referred from gyp/tests.gyp
    {
      'target_name': 'rewriter_all_test',
      'type': 'none',
      'dependencies': [
        'calculator/calculator.gyp:calculator_all_test',
        'rewriter_test',
        'single_kanji_rewriter_test',
      ],
    },
  ],
}
