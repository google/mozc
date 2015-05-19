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

# rewriter_base.gyp defines targets for lower layers to link to the rewriter
# modules, so modules in lower layers do not depend on ones in higher layers,
# avoiding circular dependencies.
{
  'variables': {
    'relative_dir': 'rewriter',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  'targets': [
    {
      'target_name': 'gen_rewriter_files',
      'type': 'none',
      'actions': [
        {
          'action_name': 'gen_collocation_data',
          'variables': {
            'input_files%': [
              '../data/dictionary/collocation.txt',
            ],
          },
          'inputs': [
            '<@(input_files)',
          ],
          'conditions': [
          ],
          'outputs': [
            '<(gen_out_dir)/embedded_collocation_data.h',
          ],
          'action': [
            'python', '../build_tools/redirect.py',
            '<(gen_out_dir)/embedded_collocation_data.h',
            '<(mozc_build_tools_dir)/gen_collocation_data_main',
            '<@(input_files)',
          ],
        },
        {
          'action_name': 'gen_single_kanji_rewriter_data',
          'variables': {
            'input_files': [
              '../data/single_kanji/single_kanji.tsv',
            ],
          },
          'inputs': [
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/single_kanji_rewriter_data.h',
          ],
          'action': [
            '<(mozc_build_tools_dir)/gen_single_kanji_rewriter_dictionary_main',
            '--logtostderr',
            '--input=<(input_files)',
            '--output=<(gen_out_dir)/single_kanji_rewriter_data.h',
            '--min_prob=0.0',
          ],
        },
        {
          'action_name': 'gen_symbol_rewriter_data',
          'variables': {
            'input_files': [
              '../data/symbol/symbol.tsv',
              '../data/rules/sorting_map.tsv',
              '../data/symbol/ordering_rule.txt',
            ],
          },
          'inputs': [
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/symbol_rewriter_data.h',
          ],
          'action': [
            '<(mozc_build_tools_dir)/gen_symbol_rewriter_dictionary_main',
            '<@(input_files)',
            '--logtostderr',
            '--output=<(gen_out_dir)/symbol_rewriter_data.h',
          ],
        },
        {
          'action_name': 'gen_emoticon_rewriter_data',
          'variables': {
            'input_file': '../data/emoticon/emoticon.tsv',
            'output_file': '<(gen_out_dir)/emoticon_rewriter_data.h',
          },
          'inputs': [
            'embedded_dictionary_compiler.py',
            'gen_emoticon_rewriter_data.py',
            '<(input_file)',
          ],
          'outputs': [
            '<(output_file)'
          ],
          'action': [
            'python', 'gen_emoticon_rewriter_data.py',
            '--input=<(input_file)',
            '--output=<(output_file)',
          ],
        },
        {
          'action_name': 'gen_user_segment_history_rewriter_rule',
          'variables': {
            'input_files': [
              '../data/dictionary/id.def',
              '../data/rules/special_pos.def',
              '../data/rules/user_segment_history_pos_group.def',
            ],
          },
          'inputs': [
            '../dictionary/gen_pos_rewrite_rule.py',
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/user_segment_history_rewriter_rule.h',
          ],
          'action': [
            'python', '../build_tools/redirect.py',
            '<(gen_out_dir)/user_segment_history_rewriter_rule.h',
            '../dictionary/gen_pos_rewrite_rule.py',
            '<@(input_files)',
          ],
        },
      ],
      'conditions': [
        ['target_platform!="Android"', {
          'actions': [
            {
              'action_name': 'gen_usage_rewriter_data',
              'variables': {
                'usage_data_file': [
                  '<(DEPTH)/third_party/japanese_usage_dictionary/usage_dict.txt',
                ],
                'cforms_file': [
                  '../data/rules/cforms.def',
                ],
              },
              'inputs': [
                '<@(usage_data_file)',
                '<@(cforms_file)',
              ],
              'outputs': [
                '<(gen_out_dir)/usage_rewriter_data.h',
              ],
              'action': [
                '<(mozc_build_tools_dir)/gen_usage_rewriter_dictionary_main',
                '--usage_data_file=<@(usage_data_file)',
                '--cforms_file=<@(cforms_file)',
                '--logtostderr',
                '--output=<(gen_out_dir)/usage_rewriter_data.h',
              ],
            },
          ],
        }],
      ],
    },
    {
      'target_name': 'gen_collocation_data_main',
      'type': 'executable',
      'sources': [
        'gen_collocation_data_main.cc',
      ],
      'dependencies': [
        '../storage/storage.gyp:storage',
      ],
    },
    {
      'target_name': 'install_gen_collocation_data_main',
      'type': 'none',
      'variables': {
        'bin_name': 'gen_collocation_data_main'
      },
      'includes' : [
        '../gyp/install_build_tool.gypi',
      ]
    },
    {
      'target_name': 'gen_single_kanji_rewriter_dictionary_main',
      'type': 'executable',
      'sources': [
        'embedded_dictionary.cc',
        'gen_single_kanji_rewriter_dictionary_main.cc',
      ],
       'dependencies': [
         '../base/base.gyp:base',
         '../data_manager/data_manager.gyp:user_dictionary_manager',
         '../dictionary/dictionary_base.gyp:user_pos_data',
       ],
    },
    {
      'target_name': 'install_gen_single_kanji_rewriter_dictionary_main',
      'type': 'none',
      'variables': {
        'bin_name': 'gen_single_kanji_rewriter_dictionary_main'
      },
      'includes' : [
        '../gyp/install_build_tool.gypi',
      ]
    },
    {
      'target_name': 'gen_symbol_rewriter_dictionary_main',
      'type': 'executable',
      'sources': [
        'dictionary_generator.cc',
        'embedded_dictionary.cc',
        'gen_symbol_rewriter_dictionary_main.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../data_manager/data_manager.gyp:user_dictionary_manager',
        '../dictionary/dictionary_base.gyp:user_pos_data',
      ],
    },
    {
      'target_name': 'install_gen_symbol_rewriter_dictionary_main',
      'type': 'none',
      'variables': {
        'bin_name': 'gen_symbol_rewriter_dictionary_main'
      },
      'includes' : [
        '../gyp/install_build_tool.gypi',
      ]
    },
    {
      'target_name': 'gen_usage_rewriter_dictionary_main',
      'type': 'executable',
      'sources': [
        'gen_usage_rewriter_dictionary_main.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
    },
    {
      'target_name': 'install_gen_usage_rewriter_dictionary_main',
      'type': 'none',
      'variables': {
        'bin_name': 'gen_usage_rewriter_dictionary_main'
      },
      'includes' : [
        '../gyp/install_build_tool.gypi',
      ]
    },
  ],
}
