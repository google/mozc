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
      'target_name': 'dictionary',
      'type': 'static_library',
      'sources': [
        '<(proto_out_dir)/<(relative_dir)/user_dictionary_storage.pb.cc',
        'dictionary.cc',
        'dictionary_compiler.cc',
        'dictionary_interface.cc',
        'text_dictionary.cc',
        'user_dictionary.cc',
        'user_dictionary_importer.cc',
        'user_dictionary_storage.cc',
        'user_dictionary_util.cc',
      ],
      'dependencies': [
        '<(DEPTH)/third_party/rx/rx.gyp:rx',
        '../base/base.gyp:base',
        '../session/session.gyp:genproto_session',
        'file/dictionary_file.gyp:dictionary_file',
        'genproto_dictionary',
        'system/system_dictionary.gyp:system_dictionary_builder',
      ],
      'actions': [
        {
          'action_name': 'gen_pos_map',
          'variables': {
            'input_files': [
              '../data/rules/user_pos.def',
              '../data/rules/third_party_pos_map.def'
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
      'conditions': [['two_pass_build==0', {
        'dependencies': [
          '../protobuf/protobuf.gyp:install_protoc',
        ],
      }]],
      'rules': [
        {
          'rule_name': 'genproto',
          'extension': 'proto',
          'inputs': [
            '../build_tools/run_after_chdir.py',
          ],
          'outputs': [
            '<(proto_out_dir)/<(relative_dir)/<(RULE_INPUT_ROOT).pb.h',
            '<(proto_out_dir)/<(relative_dir)/<(RULE_INPUT_ROOT).pb.cc',
          ],
          'action': [
            'python', '../build_tools/run_after_chdir.py',
            '<(DEPTH)',
            '<(relative_dir)/<(mozc_build_tools_dir)/protoc<(EXECUTABLE_SUFFIX)',
            '<(relative_dir)/<(RULE_INPUT_NAME)',
            '--cpp_out=<(proto_out_dir)',
          ],
          'message': 'Generating C++ code from <(RULE_INPUT_PATH)',
        },
      ],
    },
  ],
}
