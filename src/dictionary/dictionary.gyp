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
    'relative_dir': 'dictionary',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  'targets': [
    {
      'target_name': 'user_dictionary',
      'type': 'static_library',
      'sources': [
        '<(proto_out_dir)/<(relative_dir)/user_dictionary_storage.pb.cc',
        'user_dictionary.cc',
        'user_dictionary_importer.cc',
        'user_dictionary_storage.cc',
        'user_dictionary_util.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../session/session.gyp:config_handler',
        '../session/session.gyp:genproto_session',
        '../usage_stats/usage_stats.gyp:usage_stats',
        'user_pos_data',
        'pos_map',
        'genproto_dictionary',
      ],
      'conditions': [['two_pass_build==0', {
        'dependencies': [
          'install_gen_user_pos_data_main',
        ],
      }]],
    },
    {
      'target_name': 'text_dictionary_loader',
      'type': 'static_library',
      'sources': [
        'text_dictionary_loader.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
    },
    {
      'target_name': 'dictionary',
      'type': 'static_library',
      'sources': [
        'dictionary.cc',
        'dictionary_preloader.cc',
      ],
      'dependencies': [
        '<(DEPTH)/third_party/rx/rx.gyp:rx',
        '../base/base.gyp:base',
        '../session/session.gyp:genproto_session',
        'file/dictionary_file.gyp:dictionary_file',
        'user_dictionary',
        'genproto_dictionary',
        'system/system_dictionary.gyp:system_dictionary_builder',
        'text_dictionary_loader',
      ],
      'conditions': [['two_pass_build==0', {
        'dependencies': [
          'install_gen_system_dictionary_data_main',
        ],
      }]],
      'actions': [
        {
          'action_name': 'gen_embedded_dictionary_data',
          'variables': {
            'input_files': [
              '../data/dictionary/dictionary0.txt',
              '../data/dictionary/dictionary1.txt',
              '../data/dictionary/zip_code_seed.tsv',
            ],
          },
          'inputs': [
            '<@(input_files)',
          ],
          'conditions': [['two_pass_build==0', {
            'inputs': [
              '<(mozc_build_tools_dir)/gen_system_dictionary_data_main',
            ],
          }]],
          'outputs': [
            '<(gen_out_dir)/embedded_dictionary_data.h',
          ],
          'action': [
            # Use the pre-built version. See comments in mozc.gyp for why.
            '<(mozc_build_tools_dir)/gen_system_dictionary_data_main',
            '--logtostderr',
            '--input=<(input_files)',
            '--include_zip_code',
            '--make_header',
            '--output=<(gen_out_dir)/embedded_dictionary_data.h',
          ],
          'message': 'Generating <(gen_out_dir)/embedded_dictionary_data.h.',
        },
      ],
    },
    {
      'target_name': 'gen_user_pos_data_main',
      'type': 'executable',
      'sources': [
        'gen_user_pos_data_main.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
    },
    {
      'target_name': 'install_gen_user_pos_data_main',
      'type': 'none',
      'variables': {
        'bin_name': 'gen_user_pos_data_main'
      },
      'includes' : [
        '../gyp/install_build_tool.gypi'
      ]
    },
    {
      'target_name': 'user_pos_data',
      'type': 'static_library',
      'sources': [
        'user_pos.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'conditions': [['two_pass_build==0', {
        'dependencies': [
          'install_gen_user_pos_data_main',
        ],
      }]],
      'actions': [
        {
          'action_name': 'gen_user_pos_data',
          'variables': {
            'input_files': [
               '../data/dictionary/id.def',
               '../data/rules/user_pos.def',
               '../data/rules/cforms.def',
            ],
          },
          'inputs': [
            '<@(input_files)',
          ],
          'conditions': [['two_pass_build==0', {
            'inputs': [
              '<(mozc_build_tools_dir)/gen_user_pos_data_main',
            ],
          }]],
          'outputs': [
            '<(gen_out_dir)/user_pos_data.h',
          ],
          'action': [
            'python', '../build_tools/redirect.py',
            '<(gen_out_dir)/user_pos_data.h',
            '<(mozc_build_tools_dir)/gen_user_pos_data_main',
            '<@(input_files)',
          ],
          'message': 'Generating <(gen_out_dir)/user_pos_data.h.',
        },
      ],
    },
    {
      'target_name': 'pos_map',
      'type': 'none',
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
      'target_name': 'genproto_dictionary',
      'type': 'none',
      'sources': [
        'user_dictionary_storage.proto',
      ],
      'includes': [
        '../protobuf/genproto.gypi',
      ],
    },
    {
      'target_name': 'gen_system_dictionary_data_main',
      'type': 'executable',
      'sources': [
        'gen_system_dictionary_data_main.cc',
        'zip_code_dictionary_builder.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../converter/converter.gyp:gen_pos_matcher',
        'system/system_dictionary.gyp:system_dictionary_builder',
        'text_dictionary_loader',
      ],
    },
    {
      'target_name': 'install_gen_system_dictionary_data_main',
      'type': 'none',
      'variables': {
        'bin_name': 'gen_system_dictionary_data_main'
      },
      'includes' : [
        '../gyp/install_build_tool.gypi'
      ],
    },
    {
      'target_name': 'dictionary_test',
      'type': 'executable',
      'sources': [
        'dictionary_preloader_test.cc',
        'dictionary_test.cc',
        'text_dictionary_loader_test.cc',
        'user_dictionary_importer_test.cc',
        'user_dictionary_storage_test.cc',
        'user_dictionary_test.cc',
        'user_dictionary_util_test.cc',
        'user_pos_test.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../protobuf/protobuf.gyp:protobuf',
        '../testing/testing.gyp:gtest_main',
        'dictionary',
        'text_dictionary_loader',
        'user_dictionary',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
  ],
}
