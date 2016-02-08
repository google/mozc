# Copyright 2010-2016, Google Inc.
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

# Template gypi to generate embedded data build rules.
#
# To use this gypi, define the following variables and include this file.
# See mozc/data_manager/oss/oss_data_manager.gyp for example.
#
# - current_dir: Should be '.', which is the relative path where the file that
#       includes this gypi exists.
# - mozc_dir: Relatieve path to mozc top directory from the file that includes
#       this gypi.
# - common_data_dir: Should be '<(mozc_dir)/data', which contains common data
#       files for all data set.
# - platform_data_dir: Relative path to data directory containing dictionary and
#       other required data. For example, '<(mozc_dir)/data/dictionary_oss'.
# - boundary_def: Relative path to boundary rule definition file.
# - dataset_tag: Unique data set tag name.
# - use_1byte_cost_for_connection_data:
#       Set to '1' or 'true' to compress connection data.
#       Typically this variable is set by build_mozc.py as gyp's parameter.
# - dictionary_files: A list of dictionary source files.
# - gen_test_dictionary: 'true' or 'false'. When 'true', generate test
#       dictionary with test POS data.
# - magic_number: Magic number to be embedded in a data set file.
# - out_mozc_data: Output file name for mozc data set.
# - out_mozc_data_header: Output C++ header file of the embedded version of
#       mozc data set file.
# - mozc_data_vername: C++ variable name for the embedded mozc data set file.
#       This variable is defined in out_mozc_data_header.
{
  'targets': [
    {
      'target_name': '<(dataset_tag)_data_manager',
      'type': 'static_library',
      'sources': [
        '<(gen_out_dir)/boundary_data.h',
        '<(gen_out_dir)/embedded_connection_data.h',
        '<(gen_out_dir)/embedded_dictionary_data.h',
        '<(gen_out_dir)/segmenter_data.h',
        '<(gen_out_dir)/suffix_data.h',
        '<(gen_out_dir)/symbol_rewriter_data.h',
        '<(mozc_dir)/dictionary/pos_group.h',
        '<(dataset_tag)_data_manager.cc',
      ],
      'dependencies': [
        '<(dataset_tag)_data_manager_base.gyp:<(dataset_tag)_user_pos_manager',
        '<(mozc_dir)/base/base.gyp:base',
        '<(mozc_dir)/data_manager/data_manager.gyp:data_manager',
        '<(mozc_dir)/data_manager/data_manager.gyp:dataset_reader',
        '<(mozc_dir)/dictionary/dictionary.gyp:suffix_dictionary',
        '<(mozc_dir)/dictionary/dictionary_base.gyp:pos_matcher',
        '<(mozc_dir)/rewriter/rewriter.gyp:embedded_dictionary',
        'gen_<(dataset_tag)_embedded_data#host',
        'gen_embedded_mozc_dataset_for_<(dataset_tag)#host',
      ],
      'conditions': [
        ['target_platform!="Android"', {
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/rewriter/usage_rewriter_data.h',
          ]
        }],
      ],
      'defines': [
        'MOZC_DATASET_MAGIC_NUMBER="<(magic_number)"',
      ],
    },
    {
      'target_name': 'gen_embedded_mozc_dataset_for_<(dataset_tag)',
      'type': 'none',
      'toolsets': ['host'],
      'dependencies': [
        'gen_mozc_dataset_for_<(dataset_tag)',
      ],
      'actions': [
        {
          'action_name': 'gen_embedded_mozc_dataset_for_<(dataset_tag)',
          'variables': {
            'mozc_data': '<(gen_out_dir)/<(out_mozc_data)',
          },
          'inputs': [
            '<(mozc_data)',
          ],
          'outputs': [
            '<(gen_out_dir)/<(out_mozc_data_header)',
          ],
          'action': [
            'python', '<(mozc_dir)/build_tools/embed_file.py',
            '--input=<(gen_out_dir)/<(out_mozc_data)',
            '--name=<(mozc_data_varname)',
            '--output=<(gen_out_dir)/<(out_mozc_data_header)',
          ],
        },
      ],
    },
    {
      'target_name': 'gen_mozc_dataset_for_<(dataset_tag)',
      'type': 'none',
      'toolsets': ['host'],
      'dependencies': [
        '../data_manager.gyp:dataset_writer_main',
        'gen_separate_connection_data_for_<(dataset_tag)#host',
        'gen_separate_dictionary_data_for_<(dataset_tag)#host',
        'gen_separate_collocation_data_for_<(dataset_tag)#host',
        'gen_separate_collocation_suppression_data_for_<(dataset_tag)#host',
        'gen_separate_suggestion_filter_data_for_<(dataset_tag)#host',
      ],
      'actions': [
        {
          'action_name': 'gen_mozc_dataset_for_<(dataset_tag)',
          'variables': {
            'generator': '<(PRODUCT_DIR)/dataset_writer_main<(EXECUTABLE_SUFFIX)',
            'dictionary': '<(gen_out_dir)/system.dictionary',
            'connection': '<(gen_out_dir)/connection.data',
            'collocation': '<(gen_out_dir)/collocation_data.data',
            'collocation_supp': '<(gen_out_dir)/collocation_suppression_data.data',
            'suggestion_filter': '<(gen_out_dir)/suggestion_filter_data.data',
          },
          'inputs': [
            '<(dictionary)',
          ],
          'outputs': [
            '<(gen_out_dir)/<(out_mozc_data)',
          ],
          'action': [
            '<(generator)',
            '--magic=<(magic_number)',
            '--output=<(gen_out_dir)/<(out_mozc_data)',
            'coll:32:<(gen_out_dir)/collocation_data.data',
            'cols:32:<(gen_out_dir)/collocation_suppression_data.data',
            'conn:32:<(gen_out_dir)/connection.data',
            'dict:32:<(gen_out_dir)/system.dictionary',
            'sugg:32:<(gen_out_dir)/suggestion_filter_data.data',
          ],
        },
      ],
    },
    {
      'target_name': 'gen_<(dataset_tag)_embedded_data',
      'type': 'none',
      'toolsets': ['host'],
      'dependencies': [
        '<(dataset_tag)_data_manager_base.gyp:gen_<(dataset_tag)_embedded_data_light',
        'gen_embedded_boundary_data_for_<(dataset_tag)#host',
        'gen_embedded_collocation_data_for_<(dataset_tag)#host',
        'gen_embedded_collocation_suppression_data_for_<(dataset_tag)#host',
        'gen_embedded_connection_data_for_<(dataset_tag)#host',
        'gen_embedded_counter_suffix_data_for_<(dataset_tag)#host',
        'gen_embedded_dictionary_data_for_<(dataset_tag)#host',
        'gen_embedded_pos_group_data_for_<(dataset_tag)#host',
        'gen_embedded_reading_correction_data_for_<(dataset_tag)#host',
        'gen_embedded_segmenter_data_for_<(dataset_tag)#host',
        'gen_embedded_suffix_data_for_<(dataset_tag)#host',
        'gen_embedded_suggestion_filter_data_for_<(dataset_tag)#host',
        'gen_embedded_symbol_rewriter_data_for_<(dataset_tag)#host',
      ],
      'conditions': [
        ['target_platform!="Android"', {
          'dependencies': [
            '../../rewriter/rewriter_base.gyp:gen_rewriter_files#host',
          ]
        }],
      ],
    },
    {
      'target_name': 'gen_dictionary_data_for_<(dataset_tag)',
      'type': 'none',
      'toolsets': ['host'],
      'conditions': [
        ['use_separate_dataset==1',{
            'dependencies': [
              'gen_separate_dictionary_data_for_<(dataset_tag)#host',
            ],
          },
          {
            'dependencies': [
              'gen_embedded_dictionary_data_for_<(dataset_tag)#host',
            ],
          },
        ]
      ],
    },
    {
      'target_name': 'gen_connection_data_for_<(dataset_tag)',
      'type': 'none',
      'toolsets': ['host'],
      'conditions': [
        ['use_separate_dataset==1',{
            'dependencies': [
              'gen_separate_connection_data_for_<(dataset_tag)#host',
            ],
          },
          {
            'dependencies': [
              'gen_embedded_connection_data_for_<(dataset_tag)#host',
            ],
          },
        ]
      ],
    },
    {
      'target_name': 'gen_embedded_pos_group_data_for_<(dataset_tag)',
      'type': 'none',
      'toolsets': ['host'],
      'actions': [
        {
          'action_name': 'gen_embedded_pos_group_data_for_<(dataset_tag)',
          'variables': {
            'input_files': [
              '<(platform_data_dir)/id.def',
              '<(common_data_dir)/rules/special_pos.def',
              '<(common_data_dir)/rules/user_segment_history_pos_group.def',
            ],
          },
          'inputs': [
            '<(mozc_dir)/dictionary/gen_pos_rewrite_rule.py',
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/pos_group_data.h',
          ],
          'action': [
            'python', '<(mozc_dir)/build_tools/redirect.py',
            '<(gen_out_dir)/pos_group_data.h',
            '<(mozc_dir)/dictionary/gen_pos_rewrite_rule.py',
            '<@(input_files)',
          ],
        },
      ],
    },
    {
      'target_name': 'gen_connection_single_column_txt_for_<(dataset_tag)',
      'type': 'none',
      'toolsets': ['host'],
      'sources': [
        '<(mozc_dir)/build_tools/zlib_util.py',
      ],
      'actions': [
        {
          'action_name': 'gen_connection_single_column_txt_for_<(dataset_tag)',
          'variables': {
            'connection_deflate': '<(platform_data_dir)/connection.deflate',
          },
          'inputs': [
            '<(connection_deflate)',
          ],
          'outputs': [
            '<(gen_out_dir)/connection_single_column.txt',
          ],
          'action': [
            'python', '<(mozc_dir)/build_tools/zlib_util.py', 'decompress',
            '<(connection_deflate)',
            '<(gen_out_dir)/connection_single_column.txt',
          ],
          'message': ('[<(dataset_tag)] Decompressing ' +
                      '<(connection_deflate)'),
        },
      ],
    },
    {
      'target_name': 'gen_embedded_connection_data_for_<(dataset_tag)',
      'type': 'none',
      'toolsets': ['host'],
      'sources': [
        '<(mozc_dir)/build_tools/code_generator_util.py',
        '<(mozc_dir)/data_manager/gen_connection_data.py',
      ],
      'dependencies': [
        'gen_connection_single_column_txt_for_<(dataset_tag)#host',
      ],
      'actions': [
        {
          'action_name': 'gen_embedded_connection_data_for_<(dataset_tag)',
          'variables': {
            'text_connection_file': '<(gen_out_dir)/connection_single_column.txt',
            'id_file': '<(platform_data_dir)/id.def',
            'special_pos_file': '<(common_data_dir)/rules/special_pos.def',
            'use_1byte_cost_flag': '<(use_1byte_cost_for_connection_data)',
          },
          'inputs': [
            '<(text_connection_file)',
            '<(id_file)',
            '<(special_pos_file)',
          ],
          'outputs': [
            '<(gen_out_dir)/embedded_connection_data.h',
          ],
          'action': [
            'python', '<(mozc_dir)/data_manager/gen_connection_data.py',
            '--text_connection_file=<(text_connection_file)',
            '--id_file=<(id_file)',
            '--special_pos_file=<(special_pos_file)',
            '--header_output_file=<(gen_out_dir)/embedded_connection_data.h',
            '--target_compiler=<(compiler_target)',
            '--use_1byte_cost=<(use_1byte_cost_flag)',
          ],
          'message': ('[<(dataset_tag)] Generating ' +
                      '<(gen_out_dir)/embedded_connection_data.h'),
        },
      ],
    },
    {
      'target_name': 'gen_embedded_dictionary_data_for_<(dataset_tag)',
      'type': 'none',
      'toolsets': ['host'],
      'dependencies': [
        '<(DEPTH)/dictionary/dictionary.gyp:gen_system_dictionary_data_main#host',
      ],
      'actions': [
        {
          'action_name': 'gen_embedded_dictionary_data_for_<(dataset_tag)',
          'variables': {
            'input_files%': '<(dictionary_files)',
            'gen_test_dictionary_flag': '<(gen_test_dictionary)',
            'additional_inputs': [],
            'additional_actions': [],
            'generator': '<(PRODUCT_DIR)/gen_system_dictionary_data_main<(EXECUTABLE_SUFFIX)',
            'additional_inputs': ['<(PRODUCT_DIR)/gen_system_dictionary_data_main<(EXECUTABLE_SUFFIX)'],
            'conditions': [
              ['use_packed_dictionary==1', {
                'additional_inputs': [
                  '<(SHARED_INTERMEDIATE_DIR)/data_manager/packed/packed_data_light_<(dataset_tag)'
                ],
                'additional_actions': [
                  '--dataset=<(SHARED_INTERMEDIATE_DIR)/data_manager/packed/packed_data_light_<(dataset_tag)',
                ],
              }],
            ],
          },
          'inputs': [
            '<@(input_files)',
            '<@(additional_inputs)',
          ],
          'outputs': [
            '<(gen_out_dir)/embedded_dictionary_data.h',
          ],
          'action': [
            # Use the pre-built version. See comments in mozc.gyp for why.
            '<(generator)',
            '--input=<(input_files)',
            '--make_header',
            '--gen_test_dictionary=<(gen_test_dictionary_flag)',
            '--output=<(gen_out_dir)/embedded_dictionary_data.h',
            '<@(additional_actions)',
          ],
          'message': ('[<(dataset_tag)] Generating ' +
                      '<(gen_out_dir)/embedded_dictionary_data.h.'),
        },
      ],
    },
    {
      'target_name': 'gen_separate_connection_data_for_<(dataset_tag)',
      'type': 'none',
      'toolsets': ['host'],
      'sources': [
        '<(mozc_dir)/build_tools/code_generator_util.py',
        '<(mozc_dir)/data_manager/gen_connection_data.py',
      ],
      'dependencies': [
        'gen_connection_single_column_txt_for_<(dataset_tag)#host',
      ],
      'actions': [
        {
          'action_name': 'gen_separate_connection_data_for_<(dataset_tag)',
          'variables': {
            'text_connection_file': '<(gen_out_dir)/connection_single_column.txt',
            'id_file': '<(platform_data_dir)/id.def',
            'special_pos_file': '<(common_data_dir)/rules/special_pos.def',
            'use_1byte_cost_flag': '<(use_1byte_cost_for_connection_data)',
          },
          'inputs': [
            '<(text_connection_file)',
            '<(id_file)',
            '<(special_pos_file)',
          ],
          'outputs': [
            '<(gen_out_dir)/connection.data',
          ],
          'action': [
            'python', '<(mozc_dir)/data_manager/gen_connection_data.py',
            '--text_connection_file',
            '<(text_connection_file)',
            '--id_file',
            '<(id_file)',
            '--special_pos_file',
            '<(special_pos_file)',
            '--binary_output_file',
            '<@(_outputs)',
            '--target_compiler',
            '<(compiler_target)',
            '--use_1byte_cost',
            '<(use_1byte_cost_flag)',
          ],
          'message': ('[<(dataset_tag)] Generating ' +
                      '<(gen_out_dir)/connection.data'),
        },
      ],
    },
    {
      'target_name': 'gen_separate_dictionary_data_for_<(dataset_tag)',
      'type': 'none',
      'toolsets': ['host'],
      'dependencies': [
        '<(DEPTH)/dictionary/dictionary.gyp:gen_system_dictionary_data_main#host',
      ],
      'variables': {
        'additional_inputs%': [],
        'generator': '<(PRODUCT_DIR)/gen_system_dictionary_data_main<(EXECUTABLE_SUFFIX)',
        'additional_inputs': ['<(PRODUCT_DIR)/gen_system_dictionary_data_main<(EXECUTABLE_SUFFIX)'],
        'gen_test_dictionary_flag': '<(gen_test_dictionary)',
      },
      'actions': [
        {
          'action_name': 'gen_separate_dictionary_data',
          'variables': {
             'input_files%': '<(dictionary_files)',
          },
          'inputs': [
            '<@(input_files)',
            '<@(additional_inputs)',
          ],
          'outputs': [
            '<(gen_out_dir)/system.dictionary',
          ],
          'action': [
            '<(generator)',
            '--input=<(input_files)',
            '--output=<(gen_out_dir)/system.dictionary',
            '--gen_test_dictionary=<(gen_test_dictionary_flag)',
          ],
          'message': 'Generating <(gen_out_dir)/system.dictionary.',
        },
      ],
    },
    {
      'target_name': 'gen_<(dataset_tag)_segmenter_inl_header',
      'type': 'none',
      'toolsets': ['host'],
      'actions': [
        {
          'action_name': 'gen_<(dataset_tag)_segmenter_inl_header',
          'variables': {
            'input_files': [
              # Order is important; files are passed to argv in this order.
              '<(platform_data_dir)/id.def',
              '<(common_data_dir)/rules/special_pos.def',
              '<(common_data_dir)/rules/segmenter.def',
            ],
          },
          'inputs': [
            '<(mozc_dir)/converter/gen_segmenter_code.py',
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/<(dataset_tag)_segmenter_inl.h',
          ],
          'action': [
            'python', '<(mozc_dir)/build_tools/redirect.py',
            '<(gen_out_dir)/<(dataset_tag)_segmenter_inl.h',
            '<(mozc_dir)/converter/gen_segmenter_code.py',
            '<@(input_files)',
          ],
          'message': ('[<(dataset_tag)] Generating ' +
                      '<(gen_out_dir)/<(dataset_tag)_segmenter_inl.h.'),
        },
      ],
    },
    {
      # We abbreviate the target name because, in some build environment on
      # Windows, the temporary file name generated by GYP exceeds the path
      # length limit.
      'target_name': 'gen_<(dataset_tag)_sbm',
      'type': 'executable',
      'toolsets': ['host'],
      'sources': [
        '<(current_dir)/gen_<(dataset_tag)_segmenter_bitarray_main.cc',
      ],
      'dependencies': [
        '<(mozc_dir)/converter/converter_base.gyp:gen_segmenter_bitarray',
        'gen_<(dataset_tag)_segmenter_inl_header',
      ],
    },
    {
      'target_name': 'gen_embedded_segmenter_data_for_<(dataset_tag)',
      'type': 'none',
      'toolsets': ['host'],
      'dependencies': [
        'gen_<(dataset_tag)_sbm#host',
      ],
      'actions': [
        {
          'action_name': 'gen_embedded_segmenter_data_for_<(dataset_tag)',
          'variables': {
            'generator': '<(PRODUCT_DIR)/gen_<(dataset_tag)_sbm<(EXECUTABLE_SUFFIX)'
          },
          'inputs': [
            '<(generator)',
          ],
          'outputs': [
            '<(gen_out_dir)/segmenter_data.h',
          ],
          'action': [
            '<(generator)',
            '--output=<(gen_out_dir)/segmenter_data.h',
          ],
          'message': ('[<(dataset_tag)] Generating ' +
                      '<(gen_out_dir)/segmenter_data.h.'),
        },
      ],
    },
    {
      'target_name': 'gen_embedded_boundary_data_for_<(dataset_tag)',
      'type': 'none',
      'toolsets': ['host'],
      'actions': [
        {
          'action_name': 'gen_embedded_boundary_data_for_<(dataset_tag)',
          'variables': {
            # ordering-sensitive
            'input_files': [
              '<(boundary_def)',
              '<(platform_data_dir)/id.def',
            ],
          },
          'inputs': [
            '<(mozc_dir)/converter/gen_boundary_data.py',
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/boundary_data.h',
          ],
          'action': [
            'python', '<(mozc_dir)/build_tools/redirect.py',
            '<(gen_out_dir)/boundary_data.h',
            '<(mozc_dir)/converter/gen_boundary_data.py',
            '<@(input_files)',
          ],
          'message': ('[<(dataset_tag)] Generating ' +
                      '<(gen_out_dir)/boundary_data.h.'),
        },
      ],
    },
    {
      'target_name': 'gen_embedded_suffix_data_for_<(dataset_tag)',
      'type': 'none',
      'toolsets': ['host'],
      'actions': [
        {
          'action_name': 'gen_embedded_suffix_data_for_<(dataset_tag)',
          'variables': {
            'input_files': [
              '<(platform_data_dir)/suffix.txt',
            ],
          },
          'inputs': [
            '<(mozc_dir)/dictionary/gen_suffix_data.py',
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/suffix_data.h',
          ],
          'action': [
            'python', '<(mozc_dir)/build_tools/redirect.py',
            '<(gen_out_dir)/suffix_data.h',
            '<(mozc_dir)/dictionary/gen_suffix_data.py',
            '<@(input_files)',
          ],
          'message': ('[<(dataset_tag)] Generating ' +
                      '<(gen_out_dir)/suffix_data.h'),
        },
      ],
    },
    {
      'target_name': 'gen_embedded_reading_correction_data_for_<(dataset_tag)',
      'type': 'none',
      'toolsets': ['host'],
      'actions': [
        {
          'action_name': 'gen_reading_correction_data',
          'variables': {
            'input_files%': [
              '<(platform_data_dir)/reading_correction.tsv',
            ],
          },
          'inputs': [
            '<(platform_data_dir)/reading_correction.tsv',
          ],
          'outputs': [
            '<(gen_out_dir)/reading_correction_data.h',
          ],
          'action': [
            'python', '<(mozc_dir)/rewriter/gen_reading_correction_data.py',
            '--output=<(gen_out_dir)/reading_correction_data.h',
            '--input=<@(input_files)',
          ],
          'message': ('[<(dataset_tag)] Generating ' +
                      '<(gen_out_dir)/reading_correction_data.h'),
        },
      ],
    },
    {
      'target_name': 'gen_separate_collocation_data_for_<(dataset_tag)',
      'type': 'none',
      'toolsets': ['host'],
      'dependencies': [
        '../../rewriter/rewriter_base.gyp:gen_collocation_data_main#host',
      ],
      'actions': [
        {
          'action_name': 'gen_separate_collocation_data',
          'variables': {
            'generator' : '<(PRODUCT_DIR)/gen_collocation_data_main<(EXECUTABLE_SUFFIX)',
            'input_files%': [
              '<(platform_data_dir)/collocation.txt',
            ],
          },
          'inputs': [
            '<(generator)',
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/collocation_data.data',
          ],
          'action': [
            '<(generator)',
            '--collocation_data=<@(input_files)',
            '--output=<(gen_out_dir)/collocation_data.data',
            '--binary_mode',
          ],
          'message': ('[<(dataset_tag)] Generating ' +
                      '<(gen_out_dir)/collocation_data.data'),
        },
      ],
    },
    {
      'target_name': 'gen_embedded_collocation_data_for_<(dataset_tag)',
      'type': 'none',
      'toolsets': ['host'],
      'dependencies': [
        '../../rewriter/rewriter_base.gyp:gen_collocation_data_main#host',
      ],
      'actions': [
        {
          'action_name': 'gen_embedded_collocation_data',
          'variables': {
            'generator' : '<(PRODUCT_DIR)/gen_collocation_data_main<(EXECUTABLE_SUFFIX)',
            'input_files%': [
              '<(platform_data_dir)/collocation.txt',
            ],
          },
          'inputs': [
            '<(generator)',
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/embedded_collocation_data.h',
          ],
          'action': [
            'python', '<(mozc_dir)/build_tools/redirect.py',
            '<(gen_out_dir)/embedded_collocation_data.h',
            '<(generator)',
            '--collocation_data=<@(input_files)',
          ],
          'message': ('[<(dataset_tag)] Generating ' +
                      '<(gen_out_dir)/embedded_collocation_data.h'),
        },
      ],
    },
    {
      'target_name': 'gen_embedded_collocation_suppression_data_for_<(dataset_tag)',
      'type': 'none',
      'toolsets': ['host'],
      'dependencies': [
        '../../rewriter/rewriter_base.gyp:gen_collocation_suppression_data_main#host',
      ],
      'actions': [
        {
          'action_name': 'gen_collocation_suppression_data',
          'variables': {
            'generator' : '<(PRODUCT_DIR)/gen_collocation_suppression_data_main<(EXECUTABLE_SUFFIX)',
            'input_files%': [
              '<(platform_data_dir)/collocation_suppression.txt',
            ],
          },
          'inputs': [
            '<(generator)',
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/embedded_collocation_suppression_data.h',
          ],
          'action': [
            'python', '<(mozc_dir)/build_tools/redirect.py',
            '<(gen_out_dir)/embedded_collocation_suppression_data.h',
            '<(generator)',
            '--suppression_data=<@(input_files)',
          ],
          'message': ('[<(dataset_tag)] Generating ' +
                      '<(gen_out_dir)/embedded_collocation_suppression_data.h'),
        },
      ],
    },
    {
      'target_name': 'gen_separate_collocation_suppression_data_for_<(dataset_tag)',
      'type': 'none',
      'toolsets': ['host'],
      'dependencies': [
        '../../rewriter/rewriter_base.gyp:gen_collocation_suppression_data_main#host',
      ],
      'actions': [
        {
          'action_name': 'gen_separate_collocation_suppression_data',
          'variables': {
            'generator' : '<(PRODUCT_DIR)/gen_collocation_suppression_data_main<(EXECUTABLE_SUFFIX)',
            'input_files%': [
              '<(platform_data_dir)/collocation_suppression.txt',
            ],
          },
          'inputs': [
            '<(generator)',
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/collocation_suppression_data.data',
          ],
          'action': [
            '<(generator)',
            '--suppression_data=<@(input_files)',
            '--binary_mode',
            '--output=<(gen_out_dir)/collocation_suppression_data.data',
          ],
          'message': ('[<(dataset_tag)] Generating ' +
                      '<(gen_out_dir)/collocation_suppression_data.data'),
        },
      ],
    },
    {
      'target_name': 'gen_embedded_suggestion_filter_data_for_<(dataset_tag)',
      'type': 'none',
      'toolsets': ['host'],
      'dependencies': [
        '../../prediction/prediction_base.gyp:gen_suggestion_filter_main#host',
      ],
      'actions': [
        {
          'action_name': 'gen_suggestion_filter_data',
          'variables': {
            'generator' : '<(PRODUCT_DIR)/gen_suggestion_filter_main<(EXECUTABLE_SUFFIX)',
            'input_files%': [
              '<(platform_data_dir)/suggestion_filter.txt',
            ],
          },
          'inputs': [
            '<(generator)',
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/suggestion_filter_data.h',
          ],
          'action': [
            '<(generator)',
            '<@(input_files)',
            '<(gen_out_dir)/suggestion_filter_data.h',
            '--header',
          ],
          'message': ('[<(dataset_tag)] Generating ' +
                      '<(gen_out_dir)/suggestion_filter_data.h'),
        },
      ],
    },
    {
      'target_name': 'gen_separate_suggestion_filter_data_for_<(dataset_tag)',
      'type': 'none',
      'toolsets': ['host'],
      'dependencies': [
        '../../prediction/prediction_base.gyp:gen_suggestion_filter_main#host',
      ],
      'actions': [
        {
          'action_name': 'gen_separate_suggestion_filter_data',
          'variables': {
            'generator' : '<(PRODUCT_DIR)/gen_suggestion_filter_main<(EXECUTABLE_SUFFIX)',
            'input_files%': [
              '<(platform_data_dir)/suggestion_filter.txt',
            ],
          },
          'inputs': [
            '<(generator)',
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/suggestion_filter_data.data',
          ],
          'action': [
            '<(generator)',
            '--input=<@(input_files)',
            '--header=false',
            '--output=<(gen_out_dir)/suggestion_filter_data.data',
          ],
          'message': ('[<(dataset_tag)] Generating ' +
                      '<(gen_out_dir)/suggestion_filter_data.data'),
        },
      ],
    },
    {
      'target_name': 'gen_embedded_symbol_rewriter_data_for_<(dataset_tag)',
      'type': 'none',
      'toolsets': ['host'],
      'dependencies': [
        '../../rewriter/rewriter_base.gyp:gen_symbol_rewriter_dictionary_main#host',
      ],
      'actions': [
        {
          'action_name': 'gen_embedded_symbol_rewriter_data_for_<(dataset_tag)',
          'variables': {
            'generator' : '<(PRODUCT_DIR)/gen_symbol_rewriter_dictionary_main<(EXECUTABLE_SUFFIX)',
            'input_files': [
              '<(mozc_dir)/data/symbol/symbol.tsv',
              '<(mozc_dir)/data/rules/sorting_map.tsv',
              '<(mozc_dir)/data/symbol/ordering_rule.txt',
            ],
          },
          'inputs': [
            '<(generator)',
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/symbol_rewriter_data.h',
          ],
          'action': [
            '<(generator)',
            '<@(input_files)',
            '--output=<(gen_out_dir)/symbol_rewriter_data.h',
          ],
          'message': ('[<(dataset_tag)] Generating ' +
                      '<(gen_out_dir)/symbol_rewriter_data.h'),
          'conditions': [
            ['use_packed_dictionary==1', {
              'inputs': [
                '<(SHARED_INTERMEDIATE_DIR)/data_manager/packed/packed_data_light_<(dataset_tag)'
              ],
              'action': [
                '--dataset=<(SHARED_INTERMEDIATE_DIR)/data_manager/packed/packed_data_light_<(dataset_tag)',
              ],
            }],
          ],
        },
      ],
    },
    {
      'target_name': 'gen_embedded_counter_suffix_data_for_<(dataset_tag)',
      'type': 'none',
      'toolsets': ['host'],
      'actions': [
        {
          'action_name': 'gen_embedded_counter_suffix_data_for_<(dataset_tag)',
          'variables': {
            'id_file': '<(platform_data_dir)/id.def',
            'input_files%': '<(dictionary_files)',
          },
          'inputs': [
            '<(id_file)',
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/counter_suffix_data.h',
          ],
          'action': [
            'python', '<(mozc_dir)/rewriter/gen_counter_suffix_array.py',
            '--id_file=<(id_file)',
            '--output=<(gen_out_dir)/counter_suffix_data.h',
            '<@(input_files)',
          ],
          'message': ('[<(dataset_tag)] Generating ' +
                      '<(gen_out_dir)/counter_suffix_data.h'),
        },
      ],
    },
  ],
}
