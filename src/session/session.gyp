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

{
  'variables': {
    'relative_dir': 'session',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  'targets': [
    {
      'target_name': 'session',
      'type': 'static_library',
      'sources': [
        'japanese_session_factory.cc',
        'session.cc',
        'session_converter.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../converter/converter.gyp:converter',
        '../rewriter/calculator/calculator.gyp:calculator',
        '../storage/storage.gyp:storage',
        '../transliteration/transliteration.gyp:transliteration',
        '../usage_stats/usage_stats.gyp:usage_stats',
        '../config/config.gyp:config_handler',
        '../config/config.gyp:genproto_config',
        'session_base.gyp:genproto_session',
        'session_base.gyp:key_parser',
        'session_base.gyp:keymap',
        'session_base.gyp:keymap_factory',
        'session_base.gyp:session_protocol',
        'session_handler',
        'session_internal',
      ],
    },
    {
      'target_name': 'session_internal',
      'type' : 'static_library',
      'sources': [
        'internal/candidate_list.cc',
        'internal/ime_context.cc',
        'internal/session_output.cc',
      ],
      'dependencies': [
        '../composer/composer.gyp:composer',
        '../config/config.gyp:genproto_config',
        '../protobuf/protobuf.gyp:protobuf',
        'session_base.gyp:genproto_session',
        'session_base.gyp:session_protocol',
      ],
    },
    {
      'target_name': 'session_handler',
      'type': 'static_library',
      'sources': [
        'session_factory_manager.cc',
        'session_handler.cc',
        'session_observer_handler.cc',
        'session_watch_dog.cc',
      ],
      'dependencies': [
        '../client/client.gyp:client',
        '../config/config.gyp:genproto_config',
        'session_base.gyp:genproto_session',
        'session_base.gyp:session_protocol',
      ],
    },
    {
      'target_name': 'session_handler_stress_test_main',
      'type': 'executable',
      'sources': [
        'session_handler_stress_test_main.cc'
      ],
      'dependencies': [
        'random_keyevents_generator',
        'session_server',
      ],
    },
    {
      'target_name': 'session_server',
      'type': 'static_library',
      'sources': [
        'session_server.cc',
        'session_usage_observer.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:config_file_stream',
        '../client/client.gyp:client',
        '../config/config.gyp:config_handler',
        '../config/config.gyp:genproto_config',
        'session_base.gyp:genproto_session',
        'session_base.gyp:keymap',
        'session_base.gyp:keymap_factory',
        'session_base.gyp:session_protocol',
        'session_handler',
      ],
    },
    {
      'target_name': 'random_keyevents_generator',
      'type': 'static_library',
      'sources': [
        '<(gen_out_dir)/session_stress_test_data.h',
        'random_keyevents_generator.cc',
      ],
      'dependencies': [
        '../config/config.gyp:genproto_config',
        'gen_session_stress_test_data',
        'session',
        'session_base.gyp:genproto_session',
        'session_base.gyp:session_protocol',
      ],
    },
    {
      'target_name': 'session_server_main',
      'type': 'executable',
      'sources': [
        'session_server_main.cc',
      ],
      'dependencies': [
        'session_server',
      ],
    },
    {
      'target_name': 'gen_session_stress_test_data',
      'type': 'none',
      'actions': [
        {
          'action_name': 'gen_session_stress_test_data',
          'inputs': [
            '../data/test/stress_test/sentences.txt',
          ],
          'outputs': [
            '<(gen_out_dir)/session_stress_test_data.h'
          ],
          'action': [
            'python', '../build_tools/redirect.py',
            '<(gen_out_dir)/session_stress_test_data.h',
            'gen_session_stress_test_data.py',
            '../data/test/stress_test/sentences.txt',
          ],
          'message': 'Generating <(gen_out_dir)/session_stress_test_data.h',
        },
      ],
    },
  ],
}
