# Copyright 2010-2015, Google Inc.
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

# session_base.gyp defines targets for lower layers to link to the session
# modules, so modules in lower layers do not depend on ones in higher layers,
# avoiding circular dependencies.
{
  'variables': {
    'relative_dir': 'protocol',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  'targets': [
    {
      'target_name': 'genproto_candidates_proto',
      'type': 'none',
      'toolsets': ['host'],
      'sources': [
        'candidates.proto',
      ],
      'includes': [
        '../protobuf/genproto.gypi',
      ],
      'dependencies': [
        '../config/config.gyp:genproto_config',
      ],
    },
    {
      'target_name': 'candidates_proto',
      'type': 'static_library',
      'hard_dependency': 1,
      'sources': [
        '<(proto_out_dir)/<(relative_dir)/candidates.pb.cc',
      ],
      'dependencies': [
        '../protobuf/protobuf.gyp:protobuf',
        'genproto_candidates_proto#host',
      ],
      'export_dependent_settings': [
        'genproto_candidates_proto#host',
      ],
    },
    {
      'target_name': 'genproto_commands_proto',
      'type': 'none',
      'toolsets': ['host'],
      'sources': [
        'commands.proto',
      ],
      'includes': [
        '../protobuf/genproto.gypi',
      ],
      'dependencies': [
        '../config/config.gyp:genproto_config',
        'genproto_user_dictionary_storage_proto',
      ],
    },
    {
      'target_name': 'commands_proto',
      'type': 'static_library',
      'hard_dependency': 1,
      'sources': [
        '<(proto_out_dir)/<(relative_dir)/commands.pb.cc',
      ],
      'dependencies': [
        '../config/config.gyp:config_protocol',
        '../protobuf/protobuf.gyp:protobuf',
        'candidates_proto',
        'genproto_commands_proto#host',
        'user_dictionary_storage_proto',
      ],
      'export_dependent_settings': [
        'genproto_commands_proto#host',
      ],
    },
    {
      'target_name': 'genproto_renderer_proto',
      'type': 'none',
      'toolsets': ['host'],
      'sources': [
        'renderer_command.proto',
        'renderer_style.proto',
      ],
      'includes': [
        '../protobuf/genproto.gypi',
      ],
    },
    {
      'target_name': 'renderer_proto',
      'type': 'static_library',
      'hard_dependency': 1,
      'sources': [
        '<(proto_out_dir)/<(relative_dir)/renderer_command.pb.cc',
        '<(proto_out_dir)/<(relative_dir)/renderer_style.pb.cc',
      ],
      'dependencies': [
        '../config/config.gyp:config_protocol',
        '../protobuf/protobuf.gyp:protobuf',
        '../protocol/protocol.gyp:commands_proto',
        'genproto_renderer_proto#host'
      ],
      'export_dependent_settings': [
        'genproto_renderer_proto#host',
      ],
    },
    {
      'target_name': 'genproto_user_dictionary_storage_proto',
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
      'target_name': 'user_dictionary_storage_proto',
      'type': 'static_library',
      'hard_dependency': 1,
      'sources': [
        '<(proto_out_dir)/<(relative_dir)/user_dictionary_storage.pb.cc',
      ],
      'dependencies': [
        '../protobuf/protobuf.gyp:protobuf',
        'genproto_user_dictionary_storage_proto#host',
      ],
      'export_dependent_settings': [
        'genproto_user_dictionary_storage_proto#host',
      ],
    },
  ],
}
