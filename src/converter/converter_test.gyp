# Copyright 2010-2014, Google Inc.
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
    'relative_dir': 'converter',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  'targets': [
    {
      'target_name': 'converter_test',
      'type': 'executable',
      'sources': [
        'candidate_filter_test.cc',
        'converter_mock_test.cc',
        'converter_test.cc',
        'immutable_converter_test.cc',
        'key_corrector_test.cc',
        'lattice_test.cc',
        'nbest_generator_test.cc',
        'segments_test.cc',
      ],
      'dependencies': [
        '../composer/composer.gyp:composer',
        '../config/config.gyp:config_handler',
        '../data_manager/data_manager.gyp:user_pos_manager',
        '../data_manager/testing/mock_data_manager.gyp:mock_data_manager',
        '../dictionary/dictionary.gyp:dictionary_mock',
        '../dictionary/dictionary.gyp:suffix_dictionary',
        '../dictionary/system/system_dictionary.gyp:system_dictionary',
        '../dictionary/system/system_dictionary.gyp:value_dictionary',
        '../engine/engine.gyp:engine_factory',
        '../engine/engine.gyp:mock_data_engine_factory',
        '../prediction/prediction_base.gyp:suggestion_filter',
        '../rewriter/rewriter.gyp:rewriter',
        '../session/session_base.gyp:request_test_util',
        '../session/session_base.gyp:session_protocol',
        '../testing/testing.gyp:gtest_main',
        '../transliteration/transliteration.gyp:transliteration',
        '../usage_stats/usage_stats_test.gyp:usage_stats_testing_util',
        'converter.gyp:converter',
        'converter_base.gyp:connector_base',
        'converter_base.gyp:converter_mock',
        'converter_base.gyp:segmenter_base',
        'converter_base.gyp:segments',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'converter_regression_test',
      'type': 'executable',
      'sources': [
        'converter_regression_test.cc',
      ],
      'dependencies': [
        'converter.gyp:converter',
        '../base/base.gyp:base',
        '../config/config.gyp:config_handler',
        '../engine/engine.gyp:engine',
        '../engine/engine.gyp:engine_factory',
        '../session/session_base.gyp:request_test_util',
        '../session/session_base.gyp:session_protocol',
        '../testing/testing.gyp:gtest_main',
      ],
    },
    {
      'target_name': 'sparse_connector_test',
      'type': 'executable',
      'sources': [
        'sparse_connector_test.cc',
      ],
      'dependencies': [
        '../data_manager/data_manager.gyp:connection_file_reader',
        '../data_manager/testing/mock_data_manager.gyp:gen_separate_connection_data_for_mock#host',
        '../data_manager/testing/mock_data_manager.gyp:mock_data_manager',
        '../data_manager/testing/mock_data_manager_test.gyp:install_test_connection_txt',
        '../testing/testing.gyp:gtest_main',
        'converter_base.gyp:sparse_connector',
      ],
      'variables': {
        'test_size': 'large',
      },
    },
    {
      'target_name': 'pos_id_printer_test',
      'type': 'executable',
      'sources': [
        'pos_id_printer_test.cc',
      ],
      'dependencies': [
        '../testing/testing.gyp:gtest_main',
        'converter_base.gyp:pos_id_printer',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'cached_connector_test',
      'type': 'executable',
      'sources': [
        'cached_connector_test.cc',
      ],
      'dependencies': [
        '../testing/testing.gyp:gtest_main',
        'converter_base.gyp:cached_connector',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    # Test cases meta target: this target is referred from gyp/tests.gyp
    {
      'target_name': 'converter_all_test',
      'type': 'none',
      'dependencies': [
        'cached_connector_test',
        'converter_test',
        'converter_regression_test',
        'sparse_connector_test',
      ],
    },
  ],
}
