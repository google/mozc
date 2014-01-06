# Copyright 2010-2014, Google Inc.
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
      'target_name': '<(dataset_tag)_user_pos_manager',
      'type': 'static_library',
      'toolsets': [ 'target', 'host' ],
      'sources': [
        '<(current_dir)/<(dataset_tag)_user_pos_manager.cc',
      ],
      'dependencies': [
        '<(mozc_dir)/base/base.gyp:base',
        '<(mozc_dir)/dictionary/dictionary_base.gyp:pos_matcher',
        '<(mozc_dir)/dictionary/dictionary_base.gyp:user_pos',
        'gen_embedded_pos_matcher_data_for_<(dataset_tag)#host',
        'gen_embedded_user_pos_data_for_<(dataset_tag)#host',
      ],
    },
    {
      'target_name': 'gen_<(dataset_tag)_embedded_data_light',
      'type': 'none',
      'toolsets': ['host'],
      'dependencies': [
        'gen_embedded_pos_matcher_data_for_<(dataset_tag)#host',
        'gen_embedded_user_pos_data_for_<(dataset_tag)#host',
      ],
    },
    {
      'target_name': 'gen_embedded_user_pos_data_for_<(dataset_tag)',
      'type': 'none',
      'toolsets': ['host'],
      'dependencies': [
        '<(mozc_dir)/dictionary/dictionary_base.gyp:pos_util',
      ],
      'actions': [
        {
          'action_name': 'gen_embedded_user_pos_data_for_<(dataset_tag)',
          'variables': {
            'id_def': '<(platform_data_dir)/id.def',
            'special_pos': '<(common_data_dir)/rules/special_pos.def',
            'user_pos': '<(common_data_dir)/rules/user_pos.def',
            'cforms': '<(common_data_dir)/rules/cforms.def',
            'user_pos_data': '<(gen_out_dir)/user_pos_data.h',
          },
          'inputs': [
            '<(mozc_dir)/dictionary/gen_user_pos_data.py',
            '<(id_def)',
            '<(special_pos)',
            '<(user_pos)',
            '<(cforms)',
          ],
          'outputs': [
            '<(user_pos_data)',
          ],
          'action': [
            'python', '<(mozc_dir)/dictionary/gen_user_pos_data.py',
            '--id_file=<(id_def)',
            '--special_pos_file=<(special_pos)',
            '--user_pos_file=<(user_pos)',
            '--cforms_file=<(cforms)',
            '--output=<(user_pos_data)',
          ],
          'message': '[<(dataset_tag)] Generating <(user_pos_data).',
        },
      ],
    },
    {
      'target_name': 'gen_embedded_pos_matcher_data_for_<(dataset_tag)',
      'type': 'none',
      'toolsets': ['host'],
      'dependencies': [
        '<(mozc_dir)/dictionary/dictionary_base.gyp:pos_util',
      ],
      'actions': [
        {
          'action_name': 'gen_embedded_pos_matcher_data_for_<(dataset_tag)',
          'variables': {
            'id_def': '<(platform_data_dir)/id.def',
            'special_pos': '<(common_data_dir)/rules/special_pos.def',
            'pos_matcher_rule': '<(common_data_dir)/rules/pos_matcher_rule.def',
            'pos_matcher_data': '<(gen_out_dir)/pos_matcher_data.h',
          },
          'inputs': [
            '<(mozc_dir)/dictionary/gen_pos_matcher_code.py',
            '<(id_def)',
            '<(special_pos)',
            '<(pos_matcher_rule)'
          ],
          'outputs': [
            '<(pos_matcher_data)',
          ],
          'action': [
            'python',
            '<(mozc_dir)/dictionary/gen_pos_matcher_code.py',
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
