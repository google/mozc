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
      'conditions': [
        ['use_separate_connection_data==1', {
          'dependencies': [
            'converter.gyp:connection_data_injected_environment',
          ],
        }],
        ['use_separate_dictionary==1', {
          'dependencies': [
            '../dictionary/dictionary.gyp:dictionary_data_injected_environment',
          ],
        }],
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
      'conditions': [
        ['use_separate_connection_data==1', {
          'dependencies': [
            'converter.gyp:connection_data_injected_environment',
          ],
        }],
      ],
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
        'sparse_connector_test',
      ],
    },
  ],
}
