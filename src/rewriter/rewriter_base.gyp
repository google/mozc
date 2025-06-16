# Copyright 2010-2021, Google Inc.
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
      'dependencies': [
        '<(mozc_oss_src_dir)/dictionary/pos_matcher.gyp:pos_util',
        'gen_usage_rewriter_dictionary_main#host',
      ],
      'toolsets': ['host'],
      'actions': [
        {
          'action_name': 'gen_usage_rewriter_data',
          'variables': {
            'generator': '<(PRODUCT_DIR)/gen_usage_rewriter_dictionary_main<(EXECUTABLE_SUFFIX)',
            'usage_data_file': [
              '<(DEPTH)/third_party/japanese_usage_dictionary/usage_dict.txt',
            ],
            'cforms_file': [
              '<(mozc_oss_src_dir)/data/rules/cforms.def',
            ],
          },
          'inputs': [
            '<(generator)',
            '<@(usage_data_file)',
            '<@(cforms_file)',
          ],
          'outputs': [
            '<(gen_out_dir)/usage_base_conj_suffix.data',
            '<(gen_out_dir)/usage_conj_index.data',
            '<(gen_out_dir)/usage_conj_suffix.data',
            '<(gen_out_dir)/usage_item_array.data',
            '<(gen_out_dir)/usage_string_array.data',
          ],
          'action': [
            '<(generator)',
            '--usage_data_file=<@(usage_data_file)',
            '--cforms_file=<@(cforms_file)',
            '--output_base_conjugation_suffix=<(gen_out_dir)/usage_base_conj_suffix.data',
            '--output_conjugation_suffix=<(gen_out_dir)/usage_conj_suffix.data',
            '--output_conjugation_index=<(gen_out_dir)/usage_conj_index.data',
            '--output_usage_item_array=<(gen_out_dir)/usage_item_array.data',
            '--output_string_array=<(gen_out_dir)/usage_string_array.data',
          ],
        },
      ],
    },
    {
      'target_name': 'gen_existence_data',
      'type': 'static_library',
      'toolsets': ['host'],
      'sources': [
        'gen_existence_data.cc'
      ],
      'dependencies': [
        '<(mozc_oss_src_dir)/storage/storage.gyp:storage#host',
        '<(mozc_oss_src_dir)/base/base.gyp:codegen_bytearray_stream#host',
      ],
    },
    {
      'target_name': 'gen_collocation_data_main',
      'type': 'executable',
      'toolsets': ['host'],
      'sources': [
        'gen_collocation_data_main.cc',
      ],
      'dependencies': [
        '<(mozc_oss_src_dir)/base/absl.gyp:absl_strings',
        'gen_existence_data',
      ],
    },
    {
      'target_name': 'gen_collocation_suppression_data_main',
      'type': 'executable',
      'toolsets': ['host'],
      'sources': [
        'gen_collocation_suppression_data_main.cc',
      ],
      'dependencies': [
        'gen_existence_data',
      ],
    },
    {
      'target_name': 'gen_symbol_rewriter_dictionary_main',
      'type': 'executable',
      'toolsets': ['host'],
      'sources': [
        'dictionary_generator.cc',
        'gen_symbol_rewriter_dictionary_main.cc',
      ],
      'dependencies': [
        '<(mozc_oss_src_dir)/base/base.gyp:base',
        '<(mozc_oss_src_dir)/base/base.gyp:japanese_util',
        '<(mozc_oss_src_dir)/base/base.gyp:serialized_string_array',
        '<(mozc_oss_src_dir)/data_manager/data_manager_base.gyp:data_manager',
        '<(mozc_oss_src_dir)/data_manager/data_manager_base.gyp:serialized_dictionary',
        '<(mozc_oss_src_dir)/dictionary/dictionary_base.gyp:user_pos',
        '<(mozc_oss_src_dir)/dictionary/pos_matcher.gyp:pos_matcher',
      ],
    },
    {
      'target_name': 'gen_usage_rewriter_dictionary_main',
      'type': 'executable',
      'toolsets': ['host'],
      'sources': [
        'gen_usage_rewriter_dictionary_main.cc',
      ],
      'dependencies': [
        '<(mozc_oss_src_dir)/base/base.gyp:base',
        '<(mozc_oss_src_dir)/base/base.gyp:serialized_string_array',
      ],
    },
    {
      'target_name': 'gen_emoticon_rewriter_data_main',
      'type': 'executable',
      'toolsets': ['host'],
      'sources': [
        'gen_emoticon_rewriter_data.cc',
      ],
      'dependencies': [
        '<(mozc_oss_src_dir)/base/absl.gyp:absl_strings',
        '<(mozc_oss_src_dir)/base/base.gyp:base',
        '<(mozc_oss_src_dir)/data_manager/data_manager_base.gyp:serialized_dictionary',
      ],
    },
    {
      'target_name': 'gen_single_kanji_noun_prefix_data_main',
      'type': 'executable',
      'toolsets': ['host'],
      'sources': [
        'gen_single_kanji_noun_prefix_data.cc',
      ],
      'dependencies': [
        '<(mozc_oss_src_dir)/base/base.gyp:base',
        '<(mozc_oss_src_dir)/data_manager/data_manager_base.gyp:serialized_dictionary',
      ],
    },
  ],
}
