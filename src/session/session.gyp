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
        '<(gen_out_dir)/../dictionary/pos_matcher_impl.inc',
        'session.cc',
      ],
      'dependencies': [
        '<(mozc_oss_src_dir)/base/absl.gyp:absl_log',
        '<(mozc_oss_src_dir)/base/absl.gyp:absl_strings',
        '<(mozc_oss_src_dir)/base/base.gyp:base',
        '<(mozc_oss_src_dir)/base/base.gyp:version',
        '<(mozc_oss_src_dir)/composer/composer.gyp:key_parser',
        '<(mozc_oss_src_dir)/config/config.gyp:config_handler',
        '<(mozc_oss_src_dir)/converter/converter_base.gyp:segments',
        '<(mozc_oss_src_dir)/dictionary/dictionary_base.gyp:pos_matcher',
        '<(mozc_oss_src_dir)/engine/engine.gyp:engine_converter',
        '<(mozc_oss_src_dir)/protocol/protocol.gyp:commands_proto',
        '<(mozc_oss_src_dir)/protocol/protocol.gyp:config_proto',
        '<(mozc_oss_src_dir)/request/request.gyp:conversion_request',
        '<(mozc_oss_src_dir)/transliteration/transliteration.gyp:transliteration',
        '<(mozc_oss_src_dir)/usage_stats/usage_stats_base.gyp:usage_stats',
        'session_base.gyp:keymap',
        'session_base.gyp:session_usage_stats_util',
        'session_internal',
      ],
    },
    {
      'target_name': 'session_internal',
      'type' : 'static_library',
      'hard_dependency': 1,
      'sources': [
        'ime_context.cc',
        'key_event_transformer.cc',
      ],
      'dependencies': [
        '<(mozc_oss_src_dir)/base/absl.gyp:absl_strings',
        '<(mozc_oss_src_dir)/base/base.gyp:base',
        '<(mozc_oss_src_dir)/composer/composer.gyp:composer',
        '<(mozc_oss_src_dir)/config/config.gyp:config_handler',
        '<(mozc_oss_src_dir)/protocol/protocol.gyp:commands_proto',
        '<(mozc_oss_src_dir)/protocol/protocol.gyp:config_proto',
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
        '<(mozc_oss_src_dir)/base/base.gyp:base',
        '<(mozc_oss_src_dir)/client/client.gyp:client',
        '<(mozc_oss_src_dir)/base/absl.gyp:absl_synchronization',
        '<(mozc_oss_src_dir)/base/absl.gyp:absl_time',
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
        '<(mozc_oss_src_dir)/base/absl.gyp:absl_strings',
        '<(mozc_oss_src_dir)/composer/composer.gyp:composer',
        '<(mozc_oss_src_dir)/config/config.gyp:character_form_manager',
        '<(mozc_oss_src_dir)/config/config.gyp:config_handler',
        '<(mozc_oss_src_dir)/engine/engine.gyp:engine_factory',
        '<(mozc_oss_src_dir)/protocol/protocol.gyp:commands_proto',
        '<(mozc_oss_src_dir)/protocol/protocol.gyp:config_proto',
        '<(mozc_oss_src_dir)/protocol/protocol.gyp:engine_builder_proto',
        '<(mozc_oss_src_dir)/protocol/protocol.gyp:user_dictionary_storage_proto',
        '<(mozc_oss_src_dir)/usage_stats/usage_stats_base.gyp:usage_stats',
        ':session_watch_dog',
        'session_base.gyp:keymap',
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
        '<(mozc_oss_src_dir)/base/absl.gyp:absl_strings',
        '<(mozc_oss_src_dir)/base/base.gyp:base',
        '<(mozc_oss_src_dir)/base/base.gyp:number_util',
        '<(mozc_oss_src_dir)/config/config.gyp:config_handler',
        '<(mozc_oss_src_dir)/engine/engine.gyp:engine_factory',
        '<(mozc_oss_src_dir)/protocol/protocol.gyp:commands_proto',
        '<(mozc_oss_src_dir)/protocol/protocol.gyp:config_proto',
        '<(mozc_oss_src_dir)/request/request.gyp:request_test_util',
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
        '<(mozc_oss_src_dir)/base/absl.gyp:absl_synchronization',
        '<(mozc_oss_src_dir)/base/base.gyp:base',
        '<(mozc_oss_src_dir)/base/base.gyp:number_util',
        '<(mozc_oss_src_dir)/config/config.gyp:stats_config_util',
        '<(mozc_oss_src_dir)/protocol/protocol.gyp:state_proto',
        '<(mozc_oss_src_dir)/usage_stats/usage_stats_base.gyp:usage_stats',
        '<(mozc_oss_src_dir)/usage_stats/usage_stats_base.gyp:usage_stats_protocol',
      ],
    },
    {
      'target_name': 'session_server',
      'type': 'static_library',
      'sources': [
        'session_server.cc',
      ],
      'dependencies': [
        '<(mozc_oss_src_dir)/base/absl.gyp:absl_strings',
        '<(mozc_oss_src_dir)/base/base.gyp:base',
        '<(mozc_oss_src_dir)/engine/engine.gyp:engine_factory',
        '<(mozc_oss_src_dir)/usage_stats/usage_stats_base.gyp:usage_stats_uploader',
        '<(mozc_oss_src_dir)/protocol/protocol.gyp:commands_proto',
        '<(mozc_oss_src_dir)/protocol/protocol.gyp:state_proto',
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
        '<(mozc_oss_src_dir)/base/absl.gyp:absl_random',
        '<(mozc_oss_src_dir)/base/absl.gyp:absl_strings',
        '<(mozc_oss_src_dir)/base/base.gyp:japanese_util',
        '<(mozc_oss_src_dir)/protocol/protocol.gyp:commands_proto',
        '<(mozc_oss_src_dir)/protocol/protocol.gyp:config_proto',
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
            '<(mozc_oss_src_dir)/data/test/stress_test/sentences.txt',
          ],
          'outputs': [
            '<(gen_out_dir)/session_stress_test_data.h'
          ],
          'action': [
            '<(python)', 'gen_session_stress_test_data.py',
            '--input',
            '<(mozc_oss_src_dir)/data/test/stress_test/sentences.txt',
            '--output',
            '<(gen_out_dir)/session_stress_test_data.h',
          ],
          'message': 'Generating <(gen_out_dir)/session_stress_test_data.h',
        },
      ],
    },
  ],
}
