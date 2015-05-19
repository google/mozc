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
    'relative_dir': 'data_manager',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  'targets': [
    {
      'target_name': 'pos_group_data',
      'type': 'none',
      'toolsets': ['host'],
      'actions': [
        {
          'action_name': 'gen_pos_group',
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
            '<(gen_out_dir)/pos_group_data.h',
          ],
          'action': [
            'python', '../build_tools/redirect.py',
            '<(gen_out_dir)/pos_group_data.h',
            '../dictionary/gen_pos_rewrite_rule.py',
            '<@(input_files)',
          ],
        },
      ],
    },
    {
      'target_name': 'user_pos_manager',
      'type': 'static_library',
      'hard_dependency': 1,
      'toolsets': ['target', 'host'],
      'sources': [
        'user_pos_manager.cc',
        '../dictionary/pos_group.h',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../dictionary/dictionary_base.gyp:user_pos',
        '../dictionary/dictionary_base.gyp:user_pos_data',
        'pos_group_data#host',
      ],
      'export_dependent_settings': [
        'pos_group_data#host',
      ],
    },
    {
      'target_name': 'user_dictionary_manager',
      'type': 'static_library',
      'sources': [
        'user_dictionary_manager.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../dictionary/dictionary_base.gyp:pos_matcher',
        '../dictionary/dictionary_base.gyp:user_dictionary',
        '../dictionary/dictionary_base.gyp:user_pos_data',
        'user_pos_manager',
      ],
    },
  ],
}
