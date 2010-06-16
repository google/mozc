# Copyright 2010, Google Inc.
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
      'target_name': 'character_form_manager',
      'type': 'static_library',
      'sources': [
        'character_form_manager.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../session/session.gyp:config_handler',
        # This is needed. GYP is not smart enough about indirect dependencies.
        '../session/session.gyp:genproto_session',
        # storage.gyp:storage is depended by character_form_manager.
        # TODO(komatsu): delete this line.
        '../storage/storage.gyp:storage',
      ]
    },
    {
      'target_name': 'character_form_manager_test',
      'type': 'executable',
      'sources': [
        'character_form_manager_test.cc',
      ],
      'dependencies': [
        'character_form_manager',
        '../testing/testing.gyp:gtest_main',
      ],
    },
    {
      'target_name': 'converter',
      'type': 'static_library',
      'sources': [
        '<(gen_out_dir)/embedded_connection_data.h',
        '<(gen_out_dir)/segmenter_data.h',
        '<(gen_out_dir)/segmenter_inl.h',
        'candidate_filter.cc',
        'connector.cc',
        'sparse_connector.cc',
        'converter.cc',
        'lattice.cc',
        'converter_mock.cc',
        'focus_candidate_handler.cc',
        'immutable_converter.cc',
        'key_corrector.cc',
        'nbest_generator.cc',
        'segmenter.cc',
        'segments.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../dictionary/dictionary.gyp:dictionary',
        '../prediction/prediction.gyp:prediction',
        '../rewriter/rewriter.gyp:rewriter',
        '../session/session.gyp:session_protocol',
        'character_form_manager',
        'gen_segmenter_inl',
        'gen_pos_matcher',
      ],
      'conditions': [['two_pass_build==0', {
        'dependencies': [
          'install_gen_connection_data_main',
          'install_gen_segmenter_bitarray_main',
        ],
      }]],
      'actions': [
        {
          'action_name': 'gen_embedded_connection_data',
          'variables': {
            'input_files': [
               '../data/dictionary/connection.txt',
             ],
          },
          'inputs': [
            '<@(input_files)',
          ],
          # HACK: If gen_converter_data_main is added to inputs, build with
          # vcbuild fails because gyp on Windows generates a project file which
          # excludes this action from executed actions for some reasons. Build
          # with vcbuild succeeds without specifying this input so we omit the
          # input on Windows.
          'conditions': [['two_pass_build==0 and OS!="win"', {
            'inputs': [
               '<(mozc_build_tools_dir)/gen_connection_data_main',
            ],
          }]],
          'outputs': [
            '<(gen_out_dir)/embedded_connection_data.h',
          ],
          'action': [
            # Use the pre-built version. See comments in mozc.gyp for why.
            '<(mozc_build_tools_dir)/gen_connection_data_main',
            '--logtostderr',
            '--input=<(input_files)',
            '--make_header',
            '--output=<(gen_out_dir)/embedded_connection_data.h',
          ],
          'message': 'Generating <(gen_out_dir)/embedded_connection_data.h.',
        },
        {
          'action_name': 'gen_segmenter_data',
          'inputs': [
            # HACK: Specifying this file is redundant but gyp requires actions
            # to specify at least one file in inputs.
            'gen_segmenter_bitarray_main.cc',
          ],
          'conditions': [['two_pass_build==0', {
            'inputs': [
              '<(mozc_build_tools_dir)/gen_segmenter_bitarray_main',
            ],
          }]],
          'outputs': [
            '<(gen_out_dir)/segmenter_data.h',
          ],
          'action': [
            '<(mozc_build_tools_dir)/gen_segmenter_bitarray_main',
            '--logtostderr',
            '--output=<(gen_out_dir)/segmenter_data.h',
          ],
          'message': 'Generating <(gen_out_dir)/segmenter_data.h.',
        },
      ],
    },
    {
      'target_name': 'gen_connection_data_main',
      'type': 'executable',
      'sources': [
        'sparse_connector.cc',
        'sparse_connector_builder.cc',
        'gen_connection_data_main.cc',
      ],
      'dependencies': [
        '../storage/storage.gyp:storage',
      ],
    },
    {
      'target_name': 'install_gen_connection_data_main',
      'type': 'none',
      'variables': {
       'bin_name': 'gen_connection_data_main'
      },
      'includes' : [
        '../gyp/install_build_tool.gypi'
      ]
    },
    {
      'target_name': 'gen_segmenter_bitarray_main',
      'type': 'executable',
      'sources': [
        'gen_segmenter_bitarray_main.cc',
        'key_corrector.cc',
      ],
      'dependencies': [
        'gen_segmenter_inl',
        '../storage/storage.gyp:storage',
      ],
    },
    {
      'target_name': 'gen_segmenter_inl',
      'type': 'none',
      'actions': [
        {
          'action_name': 'gen_segmenter_inl',
          'variables': {
            'input_files': [
              '../data/dictionary/id.def',
              '../data/rules/segmenter.def',
            ],
          },
          'inputs': [
            'gen_segmenter_code.py',
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/segmenter_inl.h',
          ],
          'action': [
            'python', '../build_tools/redirect.py',
            '<(gen_out_dir)/segmenter_inl.h',
            'gen_segmenter_code.py',
            '<@(input_files)',
          ],
          'message': 'Generating <(gen_out_dir)/segmenter_inl.h.',
        },
      ],
    },
    {
      'target_name': 'gen_pos_matcher',
      'type': 'none',
      'actions': [
        {
          'action_name': 'gen_pos_matcher',
          'variables': {
            'input_files': [
              '../data/dictionary/id.def',
              '../data/rules/pos_matcher_rule.def',
            ],
          },
          'inputs': [
            'gen_pos_matcher_code.py',
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/pos_matcher.h',
          ],
          'action': [
            'python', '../build_tools/redirect.py',
            '<(gen_out_dir)/pos_matcher.h',
            'gen_pos_matcher_code.py',
            '<@(input_files)',
          ],
          'message': 'Generating <(gen_out_dir)/pos_matcher.h.',
        },
      ],
    },
    {
      'target_name': 'install_gen_segmenter_bitarray_main',
      'type': 'none',
      'variables': {
        'bin_name': 'gen_segmenter_bitarray_main'
      },
      'includes' : [
        '../gyp/install_build_tool.gypi',
      ]
    },
  ],
}
