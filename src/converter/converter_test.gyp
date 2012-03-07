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
        'connector_test.cc',
        'converter_mock_test.cc',
        'converter_test.cc',
        'immutable_converter_test.cc',
        'key_corrector_test.cc',
        'lattice_test.cc',
        'segmenter_test.cc',
        'segments_test.cc',
        'sparse_connector_builder.cc',
        'test_segmenter_test.cc',
      ],
      'dependencies': [
        '../composer/composer.gyp:composer',
        '../config/config.gyp:config_handler',
        '../rewriter/rewriter.gyp:rewriter',
        '../testing/testing.gyp:gtest_main',
        '../transliteration/transliteration.gyp:transliteration',
        'converter.gyp:converter',
        'converter_base.gyp:segments',
        'converter_base.gyp:segmenter',
        'converter_base.gyp:test_segmenter',
        'converter_base.gyp:gen_segmenter_inl',
        'converter_base.gyp:gen_test_segmenter_inl',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'quality_regression_test_data',
      'type': 'none',
      'actions': [
        {
          'action_name': 'quality_regression_test_data',
          'variables': {
            'input_files': [
            ],
          },
          'inputs': [
            'gen_quality_regression_test_data.py',
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/quality_regression_test_data.h',
          ],
          'action': [
            'python', '../build_tools/redirect.py',
            '<(gen_out_dir)/quality_regression_test_data.h',
            'gen_quality_regression_test_data.py',
            '<@(input_files)',
          ],
          'message': 'Generating <(gen_out_dir)/quality_regression_test_data.h.',
        },
      ],
    },
    {
      'target_name': 'quality_regression_test',
      'type': 'executable',
      'sources': [
        '<(gen_out_dir)/quality_regression_test_data.h',
        'quality_regression_test.cc',
        'quality_regression_util.cc'
      ],
      'dependencies': [
        '../config/config.gyp:config_handler',
        '../testing/testing.gyp:gtest_main',
        'converter.gyp:converter',
        'converter_base.gyp:segments',
        'quality_regression_test_data',
      ],
      'variables': {
        'test_size': 'large',
      },
    },
    {
      'target_name': 'quality_regression_main',
      'type': 'executable',
      'sources': [
        'quality_regression_main.cc',
        'quality_regression_util.cc',
       ],
      'dependencies': [
        'converter.gyp:converter',
        'converter_base.gyp:segments',
      ],
    },
    {
      'target_name': 'character_form_manager_test',
      'type': 'executable',
      'sources': [
        'character_form_manager_test.cc',
      ],
      'dependencies': [
        '../testing/testing.gyp:gtest_main',
        'converter_base.gyp:character_form_manager',
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
        'converter_base.gyp:sparse_connector_builder',
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
    {
      'target_name': 'connector_test',
      'type': 'executable',
      'sources': [
        'connector_test.cc',
      ],
      'dependencies': [
        '../testing/testing.gyp:gtest_main',
        'converter_base.gyp:connector',
      ],
      'variables': {
        'test_size': 'small',
        'test_data': [
          '../<(test_data_subdir)/connection.txt',
        ],
        'test_data_subdir': 'data/dictionary',
      },
      'includes': ['../gyp/install_testdata.gypi'],
    },
    {
      'target_name': 'test_connector_test',
      'type': 'executable',
      'sources': [
        'test_connector_test.cc',
      ],
      'dependencies': [
        '../testing/testing.gyp:gtest_main',
        'converter_base.gyp:connector',
        'converter_base.gyp:test_connector',
      ],
      'variables': {
        'test_size': 'small',
      },
      # Copy explicitly.
      # install_testdata.gypi does not support multiple directories of data
      'copies': [
        {
          'destination': '<(mozc_data_dir)/data/dictionary/',
          'files': [ '../data/dictionary/connection.txt', ],
        },
        {
          'destination': '<(mozc_data_dir)/data/test/dictionary/',
          'files': [ '../data/test/dictionary/connection.txt', ],
        },
      ],
    },

    # Test cases meta target: this target is referred from gyp/tests.gyp
    {
      'target_name': 'converter_all_test',
      'type': 'none',
      'dependencies': [
        'cached_connector_test',
        'character_form_manager_test',
        'connector_test',
        'converter_test',
        'quality_regression_test',
        'sparse_connector_test',
      ],
    },
  ],
}
