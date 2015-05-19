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
        '../base/base.gyp:base',
        'pos_matcher',
      ],
    },
    {
      'target_name': 'pos_util',
      'type': 'none',
      'toolsets': ['host'],
      'sources': [
        '../build_tools/code_generator_util.py',
        'pos_util.py',
      ],
    },
    {
      'target_name': 'gen_pos_matcher',
      'type': 'none',
      'toolsets': ['host'],
      'dependencies': [
        'pos_util',
      ],
      'actions': [
        {
          'action_name': 'gen_pos_matcher',
          'variables': {
            'id_def': '../data/dictionary/id.def',
            'special_pos': '../data/rules/special_pos.def',
            'pos_matcher_rule': '../data/rules/pos_matcher_rule.def',
            'base_pos_matcher_header': '<(gen_out_dir)/base_pos_matcher.h',
            'pos_matcher_data': '<(gen_out_dir)/pos_matcher_data.h',
          },
          'inputs': [
            'gen_pos_matcher_code.py',
            '<(id_def)',
            '<(special_pos)',
            '<(pos_matcher_rule)'
          ],
          'outputs': [
            '<(base_pos_matcher_header)',
            '<(pos_matcher_data)',
          ],
          'action': [
            'python', 'gen_pos_matcher_code.py',
            '--id_file=<(id_def)',
            '--special_pos_file=<(special_pos)',
            '--pos_matcher_rule_file=<(pos_matcher_rule)',
            '--output_pos_matcher_data=<(pos_matcher_data)',
            '--output_base_pos_matcher_h=<(base_pos_matcher_header)',
          ],
          'message': ('Generating <(pos_matcher_data) and ' +
                      '<(base_pos_matcher_header)'),
        },
      ],
    },
    {
      'target_name': 'pos_matcher',
      'type': 'static_library',
      'toolsets': ['target', 'host'],
      'hard_dependency': 1,
      'sources': [
        'pos_matcher.cc',
      ],
      'dependencies': [
        'gen_pos_matcher#host',
      ],
      'export_dependent_settings': [
        'gen_pos_matcher#host',
      ]
    },
    {
      'target_name': 'install_dictionary_test_data',
      'type': 'none',
      'variables': {
        'test_data_subdir': 'data/dictionary',
        'test_data': [
          '../<(test_data_subdir)/suggestion_filter.txt',
          '../<(test_data_subdir)/dictionary00.txt',
          '../<(test_data_subdir)/dictionary01.txt',
          '../<(test_data_subdir)/dictionary02.txt',
          '../<(test_data_subdir)/dictionary03.txt',
          '../<(test_data_subdir)/dictionary04.txt',
          '../<(test_data_subdir)/dictionary05.txt',
          '../<(test_data_subdir)/dictionary06.txt',
          '../<(test_data_subdir)/dictionary07.txt',
          '../<(test_data_subdir)/dictionary08.txt',
          '../<(test_data_subdir)/dictionary09.txt',
        ],
      },
      'includes': [ '../gyp/install_testdata.gypi' ],
    },
    {
      'target_name': 'user_pos',
      'type': 'static_library',
      'toolsets': ['target', 'host'],
      'sources' : [
        'user_pos.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
    },
    {
      'target_name': 'gen_user_pos_data',
      'type': 'none',
      'toolsets': ['host'],
      'dependencies': [
        'pos_util',
      ],
      'actions': [
        {
          'action_name': 'gen_user_pos_data',
          'variables': {
            'id_def': '../data/dictionary/id.def',
            'special_pos': '../data/rules/special_pos.def',
            'user_pos': '../data/rules/user_pos.def',
            'cforms': '../data/rules/cforms.def',
            'user_pos_data': '<(gen_out_dir)/user_pos_data.h',
          },
          'inputs': [
            'gen_user_pos_data.py',
            '<(id_def)',
            '<(special_pos)',
            '<(user_pos)',
            '<(cforms)',
          ],
          'outputs': [
            '<(user_pos_data)',
          ],
          'action': [
            'python', 'gen_user_pos_data.py',
            '--id_file=<(id_def)',
            '--special_pos_file=<(special_pos)',
            '--user_pos_file=<(user_pos)',
            '--cforms_file=<(cforms)',
            '--output=<(user_pos_data)',
          ],
          'message': 'Generating <(user_pos_data).',
        },
      ],
    },
    {
      'target_name': 'user_pos_data',
      'type': 'none',
      'toolsets': ['target', 'host'],
      'hard_dependency': 1,
      'dependencies': [
        '../base/base.gyp:base',
        'gen_user_pos_data#host',
      ],
      'export_dependent_settings': [
        'gen_user_pos_data#host',
      ],
    },
    {
      'target_name': 'genproto_dictionary',
      'type': 'none',
      'toolsets': ['host'],
      'sources': [
        'user_dictionary_storage.proto',
      ],
      'includes': [
        '../protobuf/genproto.gypi',
      ],
    },
    {
      'target_name': 'dictionary_protocol',
      'type': 'static_library',
      'hard_dependency': 1,
      'sources': [
        '<(proto_out_dir)/<(relative_dir)/user_dictionary_storage.pb.cc',
      ],
      'dependencies': [
        '../protobuf/protobuf.gyp:protobuf',
        'genproto_dictionary#host',
      ],
      'export_dependent_settings': [
        'genproto_dictionary#host',
      ],
    },
    {
      'target_name': 'gen_pos_map',
      'type': 'none',
      'toolsets': ['host'],
      'actions': [
        {
          'action_name': 'gen_pos_map',
          'variables': {
            'input_files': [
              '../data/rules/user_pos.def',
              '../data/rules/third_party_pos_map.def',
            ],
          },
          'inputs': [
            'gen_pos_map.py',
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/pos_map.h',
          ],
          'action': [
            'python', '../build_tools/redirect.py',
            '<(gen_out_dir)/pos_map.h',
            'gen_pos_map.py',
            '<@(input_files)',
          ],
        },
      ],
    },
    {
      'target_name': 'suppression_dictionary',
      'type': 'static_library',
      'sources': [
        'suppression_dictionary.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
    },
    {
      'target_name': 'user_dictionary',
      'type': 'static_library',
      'sources': [
        '<(gen_out_dir)/pos_map.h',
        'user_dictionary.cc',
        'user_dictionary_importer.cc',
        'user_dictionary_storage.cc',
        'user_dictionary_util.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:config_file_stream',
        '../config/config.gyp:config_handler',
        '../config/config.gyp:config_protocol',
        '../usage_stats/usage_stats.gyp:usage_stats',
        'dictionary_protocol',
        'gen_pos_map#host',
        'gen_pos_matcher#host',
        'suppression_dictionary',
      ],
    },
  ],
}
