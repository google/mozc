# Copyright 2010-2011, Google Inc.
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
    'relative_dir': 'session',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  'targets': [
    {
      'target_name': 'key_parser',
      'type': 'static_library',
      'sources': [
        'key_parser.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../config/config.gyp:genproto_config',
        'genproto_session',
        'session_protocol',
      ],
    },
    {
      'target_name': 'keymap',
      'type': 'static_library',
      'sources': [
        'internal/keymap.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../config/config.gyp:genproto_config',
        'genproto_session',
        'key_parser',
        'session_protocol',
      ],
    },
    {
      'target_name': 'keymap_factory',
      'type': 'static_library',
      'sources': [
        'internal/keymap_factory.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../config/config.gyp:genproto_config',
        'genproto_session',
        'keymap',
        'session_protocol',
      ],
    },
    {
      'target_name': 'ime_switch_util',
      'type': 'static_library',
      'sources': [
        'ime_switch_util.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:config_file_stream',
        '../config/config.gyp:config_handler',
        '../config/config.gyp:genproto_config',
        'genproto_session',
        'key_parser',
        'keymap',
        'session_protocol',
      ],
    },
    {
      'target_name': 'genproto_session',
      'type': 'none',
      'sources': [
        'candidates.proto',
        'commands.proto',
        'state.proto',
      ],
      'includes': [
        '../protobuf/genproto.gypi',
      ],
      'dependencies': [
        '../config/config.gyp:genproto_config',
      ],
    },
    {
      'target_name': 'session_protocol',
      'type': 'static_library',
      'sources': [
        '<(proto_out_dir)/<(relative_dir)/candidates.pb.cc',
        '<(proto_out_dir)/<(relative_dir)/commands.pb.cc',
        '<(proto_out_dir)/<(relative_dir)/state.pb.cc',
      ],
      'dependencies': [
        '../protobuf/protobuf.gyp:protobuf',
        'genproto_session',
        '../config/config.gyp:config_protocol',
      ],
    },
  ],
}
