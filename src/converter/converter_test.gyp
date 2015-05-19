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
        'segments_test.cc',
      ],
      'dependencies': [
        '../composer/composer.gyp:composer',
        '../config/config.gyp:config_handler',
        '../data_manager/data_manager.gyp:user_pos_manager',
        '../data_manager/testing/mock_data_manager.gyp:mock_data_manager',
        '../dictionary/dictionary.gyp:dictionary_mock',
        '../engine/engine.gyp:engine_factory',
        '../engine/engine.gyp:mock_data_engine_factory',
        '../rewriter/rewriter.gyp:rewriter',
        '../testing/testing.gyp:gtest_main',
        '../transliteration/transliteration.gyp:transliteration',
        'converter.gyp:converter',
        'converter_base.gyp:converter_mock',
        'converter_base.gyp:segments',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'sparse_connector_test',
      'type': 'executable',
      'sources': [
        'sparse_connector_test.cc',
      ],
      'dependencies': [
        '../testing/testing.gyp:gtest_main',
        'converter_base.gyp:sparse_connector',
        'install_test_connection_data',
        'generate_test_connection_data_image',
      ],
      'variables': {
        'test_size': 'large',
      },
    },
    {
      'target_name': 'generate_test_connection_data_image',
      'type': 'none',
      'sources': [
        '../build_tools/code_generator_util.py',
        '../data_manager/gen_connection_data.py',
      ],
      'actions': [
        {
          'action_name': 'gen_test_connection_data',
          'variables': {
            'text_connection_file': '../data/test/dictionary/connection.txt',
            'id_file': '../data/test/dictionary/id.def',
            'special_pos_file': '../data/rules/special_pos.def',
            'use_1byte_cost_flag': 'false',
          },
          'inputs': [
            '<(text_connection_file)',
            '<(id_file)',
            '<(special_pos_file)',
          ],
          'outputs': [
            '<(gen_out_dir)/test_connection_data.data',
          ],
          'action': [
            'python', '../data_manager/gen_connection_data.py',
            '--text_connection_file=<(text_connection_file)',
            '--id_file=<(id_file)',
            '--special_pos_file=<(special_pos_file)',
            '--binary_output_file=<(gen_out_dir)/test_connection_data.data',
            '--target_compiler=<(target_compiler)',
            '--use_1byte_cost=<(use_1byte_cost_flag)',
          ],
          'message': ('Generating ' +
                      '<(gen_out_dir)/test_connection_data.data'),
        },
      ],
    },
    {
      'target_name': 'install_test_connection_data',
      'type': 'none',
      'variables': {
        'test_data': [
          '../<(test_data_subdir)/connection.txt',
        ],
        'test_data_subdir': 'data/test/dictionary',
      },
      'includes': ['../gyp/install_testdata.gypi'],
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
        'sparse_connector_test',
      ],
    },
  ],
}
