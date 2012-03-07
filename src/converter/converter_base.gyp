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

# converter_base.gyp defines targets for lower layers to link to the converter
# modules, so modules in lower layers do not depend on ones in higher layers,
# avoiding circular dependencies.
{
  'variables': {
    'relative_dir': 'converter',
    'relative_mozc_dir': '',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
    'gen_out_mozc_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_mozc_dir)',
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
        '../base/base.gyp:config_file_stream',
        '../config/config.gyp:config_handler',
        '../session/session_base.gyp:session_protocol',
        # storage.gyp:storage is depended by character_form_manager.
        # TODO(komatsu): delete this line.
        '../storage/storage.gyp:storage',
      ]
    },
    {
      'target_name': 'segmenter_base',
      'type': 'static_library',
      'sources': [
        'segmenter_base.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
    },
    {
      'target_name': 'segmenter',
      'type': 'static_library',
      'sources': [
        '<(gen_out_dir)/boundary_data.h',
        '<(gen_out_dir)/segmenter_data.h',
        'segmenter.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'gen_boundary_data',
        'gen_segmenter_data',
        'segmenter_base',
      ],
    },
    {
      'target_name': 'test_segmenter',
      'type': 'static_library',
      'sources': [
        '<(gen_out_dir)/test_boundary_data.h',
        '<(gen_out_dir)/test_segmenter_data.h',
        'test_segmenter.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'gen_test_boundary_data',
        'gen_test_segmenter_data',
        'segmenter_base',
      ],
    },
    {
      'target_name': 'sparse_connector',
      'type': 'static_library',
      'sources': [
        'sparse_connector.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../storage/storage.gyp:storage',
      ],
    },
    {
      'target_name': 'cached_connector',
      'type': 'static_library',
      'sources': [
        'cached_connector.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'sparse_connector',
      ],
    },
    {
      'target_name': 'connector_base',
      'type': 'static_library',
      'sources': [
        'connector_base.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'cached_connector',
        'sparse_connector',
      ],
    },
    {
      'target_name': 'connector',
      'type': 'static_library',
      'sources': [
        'connector.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'connector_base',
        'gen_connection_data',
      ],
    },
    {
      'target_name': 'test_connector',
      'type': 'static_library',
      'sources': [
        '<(gen_out_dir)/embedded_test_connection_data.h',
        'test_connector.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'connector_base',
        'gen_embedded_test_connection_data',
      ],
    },
    {
      'target_name': 'segments',
      'type': 'static_library',
      'sources': [
        '<(gen_out_mozc_dir)/dictionary/pos_matcher.h',
        'candidate_filter.cc',
        'lattice.cc',
        'nbest_generator.cc',
        'node_allocator.h',
        'segments.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../dictionary/dictionary_base.gyp:gen_pos_matcher',
        '../transliteration/transliteration.gyp:transliteration',
        'character_form_manager',
        'connector',
        'segmenter',
      ],
    },
    {
      'target_name': 'immutable_converter',
      'type': 'static_library',
      'sources': [
        '<(gen_out_dir)/user_segment_history_rewriter_rule.h',
        'immutable_converter.cc',
        'key_corrector.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../config/config.gyp:config_handler',
        '../config/config.gyp:config_protocol',
        '../dictionary/dictionary.gyp:suffix_dictionary',
        '../dictionary/dictionary_base.gyp:gen_pos_matcher',
        '../rewriter/rewriter_base.gyp:gen_rewriter_files',
        '../session/session_base.gyp:session_protocol',
        'segments',
      ],
    },
    {
      'target_name': 'gen_connection_data',
      'type': 'none',
      'conditions': [
        ['use_separate_connection_data==1',{
            'dependencies': [
              'gen_separate_connection_data',
            ],
          },
          {
            'dependencies': [
              'gen_embedded_connection_data',
            ],
          },
        ]
      ],
    },
    {
      'target_name': 'gen_embedded_connection_data',
      'type': 'none',
      'actions': [
        {
          'action_name': 'gen_embedded_connection_data',
          'variables': {
            'input_files%': [
              '../data/dictionary/connection.txt',
              '../data/dictionary/id.def',
              '../data/rules/special_pos.def',
            ],
            'use_1byte_cost_flag': 'false',
          },
          'inputs': [
            '<@(input_files)',
          ],
          # HACK: If gen_converter_data_main is added to inputs, build with
          # vcbuild fails because gyp on Windows generates a project file which
          # excludes this action from executed actions for some reasons. Build
          # with vcbuild succeeds without specifying this input so we omit the
          # input on Windows.
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
            '--use_1byte_cost=<(use_1byte_cost_flag)',
          ],
          'message': 'Generating <(gen_out_dir)/embedded_connection_data.h.',
        },
      ],
    },
    {
      # This target is copied from 'gen_embedded_connection_data'.
      # We need both targets because some targets depend on
      # gen_embedded_test_connection_data directly.
      'target_name': 'gen_separate_connection_data',
      'type': 'none',
      'actions': [
        {
          'action_name': 'gen_separate_connection_data',
          'variables': {
            'input_files%': [
              '../data/dictionary/connection.txt',
              '../data/dictionary/id.def',
              '../data/rules/special_pos.def',
            ],
            'use_1byte_cost_flag': 'false',
          },
          'inputs': [
            '<@(input_files)',
          ],
          # HACK: If gen_converter_data_main is added to inputs, build with
          # vcbuild fails because gyp on Windows generates a project file which
          # excludes this action from executed actions for some reasons. Build
          # with vcbuild succeeds without specifying this input so we omit the
          # input on Windows.
          'outputs': [
            '<(gen_out_dir)/connection.data',
          ],
          'action': [
            # Use the pre-built version. See comments in mozc.gyp for why.
            '<(mozc_build_tools_dir)/gen_connection_data_main',
            '--logtostderr',
            '--input=<(input_files)',
            '--output=<(gen_out_dir)/connection.data',
            '--use_1byte_cost=<(use_1byte_cost_flag)',
          ],
          'message': 'Generating <(gen_out_dir)/connection.data.',
        },
      ],
    },
    # TODO(toshiyuki): make .gypi file.
    {
      'target_name': 'gen_embedded_test_connection_data',
      'type': 'none',
      'actions': [
        {
          'action_name': 'gen_embedded_test_connection_data',
          'variables': {
            'input_files%': [
              '../data/test/dictionary/connection.txt',
              '../data/test/dictionary/id.def',
              '../data/rules/special_pos.def',
            ],
            'use_1byte_cost_flag': 'false',
          },
          'inputs': [
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/embedded_test_connection_data.h',
          ],
          'action': [
            '<(mozc_build_tools_dir)/gen_connection_data_main',
            '--logtostderr',
            '--input=<(input_files)',
            '--make_header',
            '--output=<(gen_out_dir)/embedded_test_connection_data.h',
            '--use_1byte_cost=<(use_1byte_cost_flag)',
          ],
          'message': 'Generating <(gen_out_dir)/embedded_test_connection_data.h.',
        },
      ],
    },
    {
      'target_name': 'gen_segmenter_data',
      'type': 'none',
      'actions': [
        {
          'action_name': 'gen_segmenter_data',
          'inputs': [
            # HACK: Specifying this file is redundant but gyp requires actions
            # to specify at least one file in inputs.
            'gen_segmenter_bitarray_main.cc',
          ],
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
      'target_name': 'gen_test_segmenter_data',
      'type': 'none',
      'actions': [
        {
          'action_name': 'gen_test_segmenter_data',
          'inputs': [
            # HACK: Specifying this file is redundant but gyp requires actions
            # to specify at least one file in inputs.
            'gen_test_segmenter_bitarray_main.cc',
          ],
          'outputs': [
            '<(gen_out_dir)/test_segmenter_data.h',
          ],
          'action': [
            '<(mozc_build_tools_dir)/gen_test_segmenter_bitarray_main',
            '--logtostderr',
            '--test_output=<(gen_out_dir)/test_segmenter_data.h',
          ],
          'message': 'Generating <(gen_out_dir)/test_segmenter_data.h.',
        },
      ],
    },
    {
      'target_name': 'sparse_connector_builder',
      'type': 'static_library',
      'sources': [
        'sparse_connector.cc',
        'sparse_connector_builder.cc',
      ],
      'dependencies': [
        '../storage/storage.gyp:storage',
      ],
    },
    {
      'target_name': 'gen_connection_data_main',
      'type': 'executable',
      'sources': [
        'gen_connection_data_main.cc',
      ],
      'dependencies': [
        'sparse_connector_builder',
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
      'target_name': 'gen_segmenter_bitarray',
      'type': 'static_library',
      'sources': [
        'gen_segmenter_bitarray.cc',
      ],
      'dependencies' : [
        '../base/base.gyp:base',
      ]
    },
    {
      'target_name': 'gen_segmenter_bitarray_main',
      'type': 'executable',
      'sources': [
        'gen_segmenter_bitarray_main.cc',
      ],
      'dependencies': [
        'gen_segmenter_bitarray',
        'gen_segmenter_inl',
      ],
    },
    {
      'target_name': 'gen_test_segmenter_bitarray_main',
      'type': 'executable',
      'sources': [
        'gen_test_segmenter_bitarray_main.cc',
      ],
      'dependencies': [
        'gen_segmenter_bitarray',
        'gen_test_segmenter_inl',
      ],
    },
    {
      'target_name': 'gen_segmenter_inl',
      'type': 'none',
      'actions': [
        {
          'action_name': 'gen_segmenter_inl',
          'variables': {
            'input_files%': [
              '../data/dictionary/id.def',
              '../data/rules/special_pos.def',
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
      'target_name': 'gen_test_segmenter_inl',
      'type': 'none',
      'actions': [
        {
          'action_name': 'gen_test_segmenter_inl',
          'variables': {
            'input_files%': [
              '../data/test/dictionary/id.def',
              '../data/rules/special_pos.def',
              '../data/rules/segmenter.def',
            ],
          },
          'inputs': [
            'gen_segmenter_code.py',
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/test_segmenter_inl.h',
          ],
          'action': [
            'python', '../build_tools/redirect.py',
            '<(gen_out_dir)/test_segmenter_inl.h',
            'gen_segmenter_code.py',
            '<@(input_files)',
          ],
          'message': 'Generating <(gen_out_dir)/test_segmenter_inl.h.',
        },
      ],
    },
    {
      'target_name': 'gen_boundary_data',
      'type': 'none',
      'actions': [
        {
          'action_name': 'gen_boundary_data',
          'variables': {
            # ordering-sensitive
            'input_files%': [
              '../data/rules/boundary.def',
              '../data/dictionary/id.def',
            ],
          },
          'inputs': [
            'gen_boundary_data.py',
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/boundary_data.h',
          ],
          'action': [
            'python', '../build_tools/redirect.py',
            '<(gen_out_dir)/boundary_data.h',
            'gen_boundary_data.py',
            '<@(input_files)',
          ],
          'message': 'Generating <(gen_out_dir)/boundary_data.h.',
        },
      ],
    },
    {
      'target_name': 'gen_test_boundary_data',
      'type': 'none',
      'actions': [
        {
          'action_name': 'gen_test_boundary_data',
          'variables': {
            # ordering-sensitive
            'input_files': [
              '../data/rules/boundary.def',
              '../data/test/dictionary/id.def',
            ],
          },
          'inputs': [
            'gen_boundary_data.py',
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/test_boundary_data.h',
          ],
          'action': [
            'python', '../build_tools/redirect.py',
            '<(gen_out_dir)/test_boundary_data.h',
            'gen_boundary_data.py',
            '<@(input_files)',
          ],
          'message': 'Generating <(gen_out_dir)/test_boundary_data.h.',
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
      ],
    },
    {
      'target_name': 'install_gen_test_segmenter_bitarray_main',
      'type': 'none',
      'variables': {
        'bin_name': 'gen_test_segmenter_bitarray_main'
      },
      'includes' : [
        '../gyp/install_build_tool.gypi',
      ],
    },
  ],
}
