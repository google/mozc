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
    'relative_dir': 'rewriter',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  'targets': [
    {
      'target_name': 'rewriter',
      'type': 'static_library',
      'sources': [
        '<(gen_out_dir)/embedded_collocation_data.h',
        '<(gen_out_dir)/single_kanji_rewriter_data.h',
        '<(gen_out_dir)/symbol_rewriter_data.h',
        '<(gen_out_dir)/user_segment_history_rewriter_rule.h',
        'collocation_rewriter.cc',
        'collocation_util.cc',
        'date_rewriter.cc',
        'dictionary_generator.cc',
        'version_rewriter.cc',
        'embedded_dictionary.cc',
        'number_rewriter.cc',
        'rewriter.cc',
        'single_kanji_rewriter.cc',
        'symbol_rewriter.cc',
        'user_boundary_history_rewriter.cc',
        'user_segment_history_rewriter.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        # Although rewriter actually depends on converter, the next line
        # should be uncomment.  However the next line makes dependency
        # circulation.
        # TODO(taku): Fix the dependency circulation.
        # '../converter/converter.gyp:converter',
        '../session/session.gyp:config_handler',
        '../storage/storage.gyp:storage',
        '../dictionary/dictionary.gyp:dictionary',
        '../usage_stats/usage_stats.gyp:usage_stats',
      ],
      'conditions': [['two_pass_build==0', {
        'dependencies': [
          'install_gen_collocation_data_main',
          'install_gen_single_kanji_rewriter_dictionary_main',
          'install_gen_symbol_rewriter_dictionary_main',
        ],
      }]],
      'actions': [
        {
          'action_name': 'gen_collocation_data',
          'variables': {
            'input_files': [
              '../data/dictionary/collocation.txt',
            ],
          },
          'inputs': [
            '<@(input_files)',
          ],
          'conditions': [['two_pass_build==0', {
            'inputs': [
              '<(mozc_build_tools_dir)/gen_collocation_data_main',
            ],
          }]],
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
          'conditions': [['two_pass_build==0', {
            'inputs': [
              '<(mozc_build_tools_dir)/gen_single_kanji_rewriter_dictionary_main',
            ],
          }]],
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
          'conditions': [['two_pass_build==0', {
            'inputs': [
              '<(mozc_build_tools_dir)/gen_symbol_rewriter_dictionary_main',
            ],
          }]],
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
          'action_name': 'gen_user_segment_history_rewriter_rule',
          'variables': {
            'input_files': [
              '../data/dictionary/id.def',
              '../data/rules/user_segment_history_pos_group.def',
            ],
          },
          'inputs': [
            '../converter/gen_pos_rewrite_rule.py',
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/user_segment_history_rewriter_rule.h',
          ],
          'action': [
            'python', '../build_tools/redirect.py',
            '<(gen_out_dir)/user_segment_history_rewriter_rule.h',
            '../converter/gen_pos_rewrite_rule.py',
            '<@(input_files)',
          ],
        },
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
         '../dictionary/dictionary.gyp:user_pos_data',
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
        '../dictionary/dictionary.gyp:user_pos_data',
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
      'target_name': 'rewriter_test',
      'type': 'executable',
      'sources': [
        'collocation_util_test.cc',
        'date_rewriter_test.cc',
        'dictionary_generator_test.cc',
        'number_rewriter_test.cc',
        'user_boundary_history_rewriter_test.cc',
        'user_segment_history_rewriter_test.cc',
      ],
      'dependencies': [
        '../converter/converter.gyp:converter',
        '../testing/testing.gyp:gtest_main',
        'rewriter',
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
        'rewriter_test',
      ],
    },
  ],
}
