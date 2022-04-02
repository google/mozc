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
  'variables': {
    'relative_dir': 'session',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  'targets': [
    {
      'target_name': 'session',
      'type': 'static_library',
      'sources': [
        'session.cc',
        'session_converter.cc',
      ],
      'dependencies': [
        '../base/absl.gyp:absl_strings',
        '../base/base.gyp:base',
        '../base/base.gyp:version',
        '../composer/composer.gyp:key_parser',
        '../config/config.gyp:config_handler',
        '../converter/converter_base.gyp:converter_util',
        '../protocol/protocol.gyp:commands_proto',
        '../protocol/protocol.gyp:config_proto',
        '../request/request.gyp:conversion_request',
        '../transliteration/transliteration.gyp:transliteration',
        '../usage_stats/usage_stats_base.gyp:usage_stats',
        'session_base.gyp:keymap',
        'session_base.gyp:keymap_factory',
        'session_base.gyp:session_usage_stats_util',
        'session_internal',
      ],
    },
    {
      'target_name': 'session_internal',
      'type' : 'static_library',
      'hard_dependency': 1,
      'sources': [
        'internal/candidate_list.cc',
        'internal/ime_context.cc',
        'internal/session_output.cc',
        'internal/key_event_transformer.cc',
      ],
      'dependencies': [
        '../base/absl.gyp:absl_strings',
        '../base/base.gyp:base',
        '../composer/composer.gyp:composer',
        '../config/config.gyp:config_handler',
        '../protocol/protocol.gyp:commands_proto',
        '../protocol/protocol.gyp:config_proto',
      ],
    },
    {
      # iOS is not supported.
      'target_name': 'session_watch_dog',
      'type': 'static_library',
      'sources': [
        'session_watch_dog.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../client/client.gyp:client',
      ],
    },
    {
      'target_name': 'session_handler',
      'type': 'static_library',
      'sources': [
        'session_handler.cc',
        'session_observer_handler.cc',
      ],
      'dependencies': [
        '../base/absl.gyp:absl_strings',
        '../composer/composer.gyp:composer',
        '../config/config.gyp:character_form_manager',
        '../config/config.gyp:config_handler',
        '../dictionary/dictionary_base.gyp:user_dictionary',
        '../engine/engine.gyp:engine_factory',
        '../protocol/protocol.gyp:commands_proto',
        '../protocol/protocol.gyp:config_proto',
        '../protocol/protocol.gyp:engine_builder_proto',
        '../protocol/protocol.gyp:user_dictionary_storage_proto',
        '../usage_stats/usage_stats_base.gyp:usage_stats',
        ':session_watch_dog',
      ],
      'conditions': [
        ['target_platform=="iOS"', {
          'dependencies!': [
            ':session_watch_dog',
          ],
        }],
      ],
    },
    {
      'target_name': 'session_handler_tool',
      'type': 'static_library',
      'sources': [
        'session_handler_tool.cc',
      ],
      'dependencies': [
        ':session',
        ':session_handler',
        ':session_usage_observer',
        '../base/absl.gyp:absl_strings',
        '../base/base.gyp:base',
        '../base/base.gyp:number_util',
        '../config/config.gyp:config_handler',
        '../engine/engine.gyp:engine_factory',
        '../protocol/protocol.gyp:commands_proto',
        '../protocol/protocol.gyp:config_proto',
      ],
    },
    {
      'target_name': 'session_usage_observer',
      'type': 'static_library',
      'hard_dependency': 1,
      'sources': [
        'session_usage_observer.cc',
      ],
      'dependencies': [
        '../base/absl.gyp:absl_synchronization',
        '../base/base.gyp:base',
        '../base/base.gyp:number_util',
        '../config/config.gyp:stats_config_util',
        '../protocol/protocol.gyp:state_proto',
        '../usage_stats/usage_stats_base.gyp:usage_stats',
        '../usage_stats/usage_stats_base.gyp:usage_stats_protocol',
      ],
    },
    {
      'target_name': 'session_server',
      'type': 'static_library',
      'sources': [
        'session_server.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../usage_stats/usage_stats_base.gyp:usage_stats_uploader',
        '../protocol/protocol.gyp:commands_proto',
        'session_handler',
        'session_usage_observer',
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
        '../base/absl.gyp:absl_strings',
        '../base/base.gyp:japanese_util',
        '../protocol/protocol.gyp:commands_proto',
        '../protocol/protocol.gyp:config_proto',
        'gen_session_stress_test_data#host',
        'session',
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
      'toolsets': ['host'],
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
            '<(python)', 'gen_session_stress_test_data.py',
            '--input',
            '../data/test/stress_test/sentences.txt',
            '--output',
            '<(gen_out_dir)/session_stress_test_data.h',
          ],
          'message': 'Generating <(gen_out_dir)/session_stress_test_data.h',
        },
      ],
    },
  ],
}
