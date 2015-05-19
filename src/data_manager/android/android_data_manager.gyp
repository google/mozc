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
    'relative_dir': 'data_manager/android',
    'relative_mozc_dir': '',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
    'gen_out_mozc_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_mozc_dir)',
    # The following variables are passed to ../data_manager.gypi.
    'current_dir': '.',
    'mozc_dir': '../..',
    'common_data_dir': '<(mozc_dir)/data',
    'platform_data_dir': '<(mozc_dir)/data/dictionary_android',
    'boundary_def': '<(mozc_dir)/data/dictionary_android/mobile_boundary.def',
    'dataset_tag': 'android',
    'use_1byte_cost_for_connection_data': 'true',
    'dictionary_files': [
      '<(platform_data_dir)/dictionary00.txt',
      '<(platform_data_dir)/dictionary01.txt',
      '<(platform_data_dir)/dictionary02.txt',
      '<(platform_data_dir)/dictionary03.txt',
      '<(platform_data_dir)/dictionary04.txt',
      '<(platform_data_dir)/dictionary05.txt',
      '<(platform_data_dir)/dictionary06.txt',
      '<(platform_data_dir)/dictionary07.txt',
      '<(platform_data_dir)/dictionary08.txt',
      '<(platform_data_dir)/dictionary09.txt',
      '<(platform_data_dir)/reading_correction.tsv',
    ],
  },
  'targets': [
    {
      'target_name': 'gen_separate_connection_data_for_android',
      'type': 'none',
      'toolsets': ['host'],
      'sources': [
        '<(mozc_dir)/build_tools/code_generator_util.py',
        '<(mozc_dir)/data_manager/gen_connection_data.py',
      ],
      'actions': [
        {
          'action_name': 'gen_separate_connection_data_for_android',
          'variables': {
            'text_connection_file': '<(platform_data_dir)/connection.txt',
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
            '<(gen_out_dir)/connection_data.data',
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
            '<(target_compiler)',
            '--use_1byte_cost',
            '<(use_1byte_cost_flag)',
          ],
          'message': ('[<(dataset_tag)] Generating ' +
                      '<(gen_out_dir)/connection_data.data'),
        },
      ],
    },
    {
      'target_name': 'gen_connection_data_for_android',
      'type': 'none',
      'toolsets': ['host'],
      'conditions': [
        ['use_separate_connection_data==1',{
            'dependencies': [
              'gen_separate_connection_data_for_android#host',
            ],
          },
          {
            'dependencies': [
              'gen_embedded_connection_data_for_android#host',
            ],
          },
        ]
      ],
    },
    {
      'target_name': 'gen_separate_dictionary_data_for_android',
      'type': 'none',
      'toolsets': ['host'],
      'actions': [
        {
          'action_name': 'gen_separate_dictionary_data',
          'variables': {
             'input_files%': '<(dictionary_files)',
          },
          'inputs': [
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/system.dictionary',
          ],
          'action': [
            '<(mozc_build_tools_dir)/gen_system_dictionary_data_main',
            '--logtostderr',
            '--input=<(input_files)',
            '--output=<(gen_out_dir)/system.dictionary',
          ],
          'message': 'Generating <(gen_out_dir)/system.dictionary.',
        },
      ],
    },
    {
      'target_name': 'gen_dictionary_data_for_android',
      'type': 'none',
      'toolsets': ['host'],
      'conditions': [
        ['use_separate_connection_data==1',{
            'dependencies': [
              'gen_separate_dictionary_data_for_android#host',
            ],
          },
          {
            'dependencies': [
              'gen_embedded_dictionary_data_for_android#host',
            ],
          },
        ]
      ],
    },
  ],
  # This 'includes' defines the following targets:
  #   - android_data_manager  (type: static_library)
  #   - gen_android_embedded_data  (type: none)
  'includes': [ '../data_manager.gypi' ],
}
