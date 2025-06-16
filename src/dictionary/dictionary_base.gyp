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

# dictionary_base.gyp defines targets for lower layers to link to the dictionary
# modules, so modules in lower layers do not depend on ones in higher layers,
# avoiding circular dependencies.
{
  'variables': {
    'relative_dir': 'dictionary',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  'targets': [
    {
      'target_name': 'text_dictionary_loader',
      'type': 'static_library',
      'toolsets': ['target', 'host'],
      'sources': [
        'dictionary_token.h',
        'text_dictionary_loader.cc',
      ],
      'dependencies': [
        '<(mozc_oss_src_dir)/base/absl.gyp:absl_strings',
        '<(mozc_oss_src_dir)/base/base.gyp:base',
        '<(mozc_oss_src_dir)/base/base.gyp:multifile',
        '<(mozc_oss_src_dir)/base/base.gyp:number_util',
        'pos_matcher.gyp:pos_matcher',
      ],
    },
    {
      'target_name': 'user_pos',
      'type': 'static_library',
      'toolsets': ['target', 'host'],
      'sources' : [
        'user_pos.cc',
      ],
      'dependencies': [
        '<(mozc_oss_src_dir)/base/absl.gyp:absl_strings',
        '<(mozc_oss_src_dir)/base/base.gyp:base',
      ],
    },
    {
      'target_name': 'gen_pos_map',
      'type': 'none',
      'toolsets': ['host'],
      'sources': [
        '<(mozc_oss_src_dir)/build_tools/code_generator_util.py',
        'gen_pos_map.py',
      ],

      'actions': [
        {
          'action_name': 'gen_pos_map',
          'variables': {
            'user_pos': '<(mozc_oss_src_dir)/data/rules/user_pos.def',
            'third_party_pos_map': '<(mozc_oss_src_dir)/data/rules/third_party_pos_map.def',
            'pos_map_header': '<(gen_out_dir)/pos_map.inc',
          },
          'inputs': [
            'gen_pos_map.py',
            '<(user_pos)',
            '<(third_party_pos_map)',
          ],
          'outputs': [
            '<(pos_map_header)',
          ],
          'action': [
            '<(python)', 'gen_pos_map.py',
            '--user_pos_file=<(user_pos)',
            '--third_party_pos_map_file=<(third_party_pos_map)',
            '--output=<(pos_map_header)',
          ],
          'message': ('Generating <(pos_map_header)'),
        },
      ],
    },
    {
      'target_name': 'user_dictionary',
      'type': 'static_library',
      'sources': [
        '<(gen_out_dir)/pos_map.inc',
        'user_dictionary.cc',
        'user_dictionary_importer.cc',
        'user_dictionary_session.cc',
        'user_dictionary_session_handler.cc',
        'user_dictionary_storage.cc',
        'user_dictionary_util.cc',
      ],
      'dependencies': [
        '<(mozc_oss_src_dir)/base/absl.gyp:absl_status',
        '<(mozc_oss_src_dir)/base/absl.gyp:absl_strings',
        '<(mozc_oss_src_dir)/base/absl.gyp:absl_synchronization',
        '<(mozc_oss_src_dir)/base/base.gyp:base',
        '<(mozc_oss_src_dir)/base/base.gyp:config_file_stream',
        '<(mozc_oss_src_dir)/base/base.gyp:number_util',
        '<(mozc_oss_src_dir)/config/config.gyp:config_handler',
        '<(mozc_oss_src_dir)/protocol/protocol.gyp:config_proto',
        '<(mozc_oss_src_dir)/protocol/protocol.gyp:user_dictionary_storage_proto',
        '<(mozc_oss_src_dir)/request/request.gyp:conversion_request',
        'gen_pos_map#host',
        'pos_matcher.gyp:pos_matcher',
      ],
    },
  ],
}
