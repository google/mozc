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

{
  'targets': [
    {
      'target_name': 'gen_<(dataset_tag)_embedded_pos_list',
      'type': 'none',
      'toolsets': ['host'],
      'dependencies': [
        'gen_separate_user_pos_data_for_<(dataset_tag)#host',
      ],
      'actions': [
        {
          'action_name': 'gen_<(dataset_tag)_embedded_pos_list',
          'variables': {
            'pos_list': '<(gen_out_dir)/pos_list.data',
          },
          'inputs': [
            '<(pos_list)',
          ],
          'outputs': [
            '<(gen_out_dir)/pos_list.inc',
          ],
          'action': [
            '<(python)', '<(mozc_oss_src_dir)/build_tools/embed_file.py',
            '--input=<(pos_list)',
            '--name=kPosArray',
            '--output=<(gen_out_dir)/pos_list.inc',
          ],
        },
      ],
    },
    {
      'target_name': 'gen_user_pos_manager_data_for_<(dataset_tag)',
      'type': 'none',
      'toolsets': ['host'],
      'dependencies': [
        '../data_manager_base.gyp:dataset_writer_main',
        'gen_separate_user_pos_data_for_<(dataset_tag)#host',
        'gen_separate_pos_matcher_data_for_<(dataset_tag)#host',
      ],
      'actions': [
        {
          'action_name': 'gen_user_pos_manager_data_for_<(dataset_tag)',
          'variables': {
            'generator': '<(PRODUCT_DIR)/dataset_writer_main<(EXECUTABLE_SUFFIX)',
            'pos_matcher': '<(gen_out_dir)/pos_matcher.data',
            'user_pos_token': '<(gen_out_dir)/user_pos_token_array.data',
            'user_pos_string': '<(gen_out_dir)/user_pos_string_array.data',
          },
          'inputs': [
            '<(pos_matcher)',
            '<(user_pos_token)',
            '<(user_pos_string)',
          ],
          'outputs': [
            '<(gen_out_dir)/user_pos_manager.data',
          ],
          'action': [
            '<(generator)',
            '--output=<(gen_out_dir)/user_pos_manager.data',
            'pos_matcher:32:<(pos_matcher)',
            'user_pos_token:32:<(user_pos_token)',
            'user_pos_string:32:<(user_pos_string)',
          ],
        },
      ],
    },
    {
      'target_name': 'gen_separate_user_pos_data_for_<(dataset_tag)',
      'type': 'none',
      'toolsets': ['host'],
      'dependencies': [
        '<(mozc_oss_src_dir)/dictionary/pos_matcher.gyp:pos_util',
      ],
      'actions': [
        {
          'action_name': 'gen_separate_user_pos_data_for_<(dataset_tag)',
          'variables': {
            'id_def': '<(platform_data_dir)/id.def',
            'special_pos': '<(mozc_oss_src_dir)/data/rules/special_pos.def',
            'user_pos': '<(mozc_oss_src_dir)/data/rules/user_pos.def',
            'cforms': '<(mozc_oss_src_dir)/data/rules/cforms.def',
            'token_array_data': '<(gen_out_dir)/user_pos_token_array.data',
            'string_array_data': '<(gen_out_dir)/user_pos_string_array.data',
            'pos_list': '<(gen_out_dir)/pos_list.data',
          },
          'inputs': [
            '<(mozc_oss_src_dir)/dictionary/gen_user_pos_data.py',
            '<(id_def)',
            '<(special_pos)',
            '<(user_pos)',
            '<(cforms)',
          ],
          'outputs': [
            '<(token_array_data)',
            '<(string_array_data)',
            '<(pos_list)',
          ],
          'action': [
            '<(python)', '<(mozc_oss_src_dir)/dictionary/gen_user_pos_data.py',
            '--id_file=<(id_def)',
            '--special_pos_file=<(special_pos)',
            '--user_pos_file=<(user_pos)',
            '--cforms_file=<(cforms)',
            '--output_token_array=<(token_array_data)',
            '--output_string_array=<(string_array_data)',
            '--output_pos_list=<(pos_list)',
          ],
          'message': '[<(dataset_tag)] Generating user pos data.',
        },
      ],
    },
    {
      'target_name': 'gen_separate_pos_matcher_data_for_<(dataset_tag)',
      'type': 'none',
      'toolsets': ['host'],
      'dependencies': [
        '<(mozc_oss_src_dir)/dictionary/pos_matcher.gyp:pos_util',
      ],
      'actions': [
        {
          'action_name': 'gen_separate_pos_matcher_data_for_<(dataset_tag)',
          'variables': {
            'id_def': '<(platform_data_dir)/id.def',
            'special_pos': '<(mozc_oss_src_dir)/data/rules/special_pos.def',
            'pos_matcher_rule': '<(mozc_oss_src_dir)/data/rules/pos_matcher_rule.def',
            'pos_matcher_data': '<(gen_out_dir)/pos_matcher.data',
          },
          'inputs': [
            '<(mozc_oss_src_dir)/dictionary/gen_pos_matcher_code.py',
            '<(id_def)',
            '<(special_pos)',
            '<(pos_matcher_rule)'
          ],
          'outputs': [
            '<(pos_matcher_data)',
          ],
          'action': [
            '<(python)',
            '<(mozc_oss_src_dir)/dictionary/gen_pos_matcher_code.py',
            '--id_file=<(id_def)',
            '--special_pos_file=<(special_pos)',
            '--pos_matcher_rule_file=<(pos_matcher_rule)',
            '--output_pos_matcher_data=<(pos_matcher_data)',
          ],
          'message': ('[<(dataset_tag)] Generating <(pos_matcher_data)'),
        },
      ],
    },
  ],
}
