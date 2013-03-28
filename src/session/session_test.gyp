# Copyright 2010-2013, Google Inc.
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
      'target_name': 'session_handler_test_util',
      'type' : 'static_library',
      'sources': [
        'session_handler_test_util.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../config/config.gyp:config_handler',
        '../config/config.gyp:config_protocol',
        '../engine/engine.gyp:engine_factory',
        '../engine/engine.gyp:mock_data_engine_factory',
        '../testing/testing.gyp:testing',
        'session.gyp:session',
        'session.gyp:session_handler',
        'session.gyp:session_usage_observer',
        'session_base.gyp:session_protocol',
      ],
    },
    {
      'target_name': 'session_server_test',
      'type': 'executable',
      'sources': [
        'session_server_test.cc',
      ],
      'dependencies': [
        '../testing/testing.gyp:gtest_main',
        'session.gyp:session',
        'session.gyp:session_server',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'session_test',
      'type': 'executable',
      'sources': [
        'session_test.cc',
      ],
      'dependencies': [
        '../converter/converter_base.gyp:converter_mock',
        '../data_manager/data_manager.gyp:user_pos_manager',
        '../engine/engine.gyp:mock_converter_engine',
        "../engine/engine.gyp:mock_data_engine_factory",
        '../rewriter/rewriter.gyp:rewriter',
        '../testing/testing.gyp:gtest_main',
        '../usage_stats/usage_stats_test.gyp:usage_stats_testing_util',
        'session.gyp:session',
        'session.gyp:session_server',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'session_regression_test',
      'type': 'executable',
      'sources': [
        'session_regression_test.cc',
      ],
      'dependencies': [
        '../engine/engine.gyp:engine_factory',
        '../testing/testing.gyp:gtest_main',
        'session.gyp:session',
        'session.gyp:session_server',
      ],
      'variables': {
        'test_size': 'large',
      },
    },
    {
      'target_name': 'session_handler_test',
      'type': 'executable',
      'sources': [
        'session_handler_test.cc',
      ],
      'dependencies': [
        '../converter/converter_base.gyp:converter_mock',
        '../engine/engine.gyp:mock_converter_engine',
        '../testing/testing.gyp:gtest_main',
        "../usage_stats/usage_stats_test.gyp:usage_stats_testing_util",
        'session.gyp:session',
        'session.gyp:session_server',
        'session_handler_test_util',
      ],
      'variables': {
        'test_size': 'small',
      },
      'conditions': [
        ['target_platform=="NaCl" and _toolset=="target"', {
          'dependencies!': [
            'session.gyp:session_server',
          ],
        }],
      ],
    },
    {
      'target_name': 'session_converter_test',
      'type': 'executable',
      'sources': [
        'session_converter_test.cc',
      ],
      'dependencies': [
        '../base/base.gyp:testing_util',
        '../converter/converter_base.gyp:converter_mock',
        '../testing/testing.gyp:gtest_main',
        '../usage_stats/usage_stats_test.gyp:usage_stats_testing_util',
        'session.gyp:session',
        'session_base.gyp:request_test_util',
      ],
    },
    {
      'target_name': 'session_module_test',
      'type': 'executable',
      'sources': [
        'ime_switch_util_test.cc',
        'key_event_util_test.cc',
        'key_parser_test.cc',
        'output_util_test.cc',
        'session_observer_handler_test.cc',
        'session_usage_observer_test.cc',
        'session_usage_stats_util_test.cc',
        'session_watch_dog_test.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base_test.gyp:scheduler_stub',
        '../client/client.gyp:client_mock',
        '../config/config.gyp:config_handler',
        '../config/config.gyp:stats_config_util',
        '../testing/testing.gyp:gtest_main',
        '../usage_stats/usage_stats_base.gyp:usage_stats',
        '../usage_stats/usage_stats_test.gyp:usage_stats_testing_util',
        'session.gyp:session_handler',
        'session.gyp:session_usage_observer',
        'session_base.gyp:ime_switch_util',
        'session_base.gyp:key_event_util',
        'session_base.gyp:key_parser',
        'session_base.gyp:keymap',
        'session_base.gyp:keymap_factory',
        'session_base.gyp:output_util',
        'session_base.gyp:session_protocol',
        'session_base.gyp:session_usage_stats_util',
      ],
      'conditions': [
        ['target_platform=="Android"', {
          'sources!': [
            'session_watch_dog_test.cc',
          ],
        }],
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'session_internal_test',
      'type': 'executable',
      'sources': [
        'internal/candidate_list_test.cc',
        'internal/ime_context_test.cc',
        'internal/keymap_test.cc',
        'internal/keymap_factory_test.cc',
        'internal/session_output_test.cc',
        'internal/key_event_transformer_test.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:testing_util',
        '../config/config.gyp:config_protocol',
        '../converter/converter_base.gyp:converter_mock',
        '../engine/engine.gyp:mock_converter_engine',
        '../testing/testing.gyp:gtest_main',
        'session.gyp:session',
        'session_base.gyp:session_protocol',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'session_handler_stress_test',
      'type': 'executable',
      'sources': [
        'session_handler_stress_test.cc'
      ],
      'dependencies': [
        '../engine/engine.gyp:engine_factory',
        '../testing/testing.gyp:gtest_main',
        'session.gyp:random_keyevents_generator',
        'session.gyp:session',
        'session.gyp:session_server',
        'session_handler_test_util',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'random_keyevents_generator_test',
      'type': 'executable',
      'sources': [
        'random_keyevents_generator_test.cc',
      ],
      'dependencies': [
        '../testing/testing.gyp:gtest_main',
        'session.gyp:random_keyevents_generator',
      ],
      'variables': {
        'test_size': 'large',
      },
    },
    {
      'target_name': 'session_converter_stress_test',
      'type': 'executable',
      'sources': [
        'session_converter_stress_test.cc'
      ],
      'dependencies': [
        '../engine/engine.gyp:mock_data_engine_factory',
        '../testing/testing.gyp:gtest_main',
        'session.gyp:session',
      ],
      'variables': {
        'test_size': 'large',
      },
    },
    {
      'target_name': 'generic_storage_manager_test',
      'type': 'executable',
      'sources': [
        'generic_storage_manager_test.cc'
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/testing.gyp:gtest_main',
        'session_base.gyp:generic_storage_manager',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'request_test_util_test',
      'type': 'executable',
      'sources': [
        'request_test_util_test.cc'
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/testing.gyp:gtest_main',
        'session_base.gyp:request_test_util',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'install_session_handler_scenario_test_data',
      'type': 'none',
      'variables': {
        'test_data': [
          '../<(test_data_subdir)/auto_partial_suggestion.txt',
          '../<(test_data_subdir)/b7132535_scenario.txt',
          '../<(test_data_subdir)/b7321313_scenario.txt',
          '../<(test_data_subdir)/change_request.txt',
          '../<(test_data_subdir)/clear_user_prediction.txt',
          '../<(test_data_subdir)/composition_display_as.txt',
          '../<(test_data_subdir)/conversion.txt',
          '../<(test_data_subdir)/conversion_display_as.txt',
          '../<(test_data_subdir)/conversion_with_history_segment.txt',
          '../<(test_data_subdir)/conversion_with_long_history_segments.txt',
          '../<(test_data_subdir)/delete_history.txt',
          '../<(test_data_subdir)/desktop_t13n_candidates.txt',
          '../<(test_data_subdir)/insert_characters.txt',
          '../<(test_data_subdir)/mobile_qwerty_transliteration_scenario.txt',
          '../<(test_data_subdir)/mobile_t13n_candidates.txt',
          '../<(test_data_subdir)/on_off_cancel.txt',
          '../<(test_data_subdir)/partial_suggestion.txt',
          '../<(test_data_subdir)/pending_character.txt',
          '../<(test_data_subdir)/segment_focus.txt',
          '../<(test_data_subdir)/segment_width.txt',
          '../<(test_data_subdir)/twelvekeys_switch_inputmode_scenario.txt',
          '../<(test_data_subdir)/twelvekeys_toggle_hiragana_preedit_scenario.txt',
        ],
        'test_data_subdir': 'data/test/session/scenario',
      },
      'includes': ['../gyp/install_testdata.gypi'],
    },
    {
      'target_name': 'install_session_handler_usage_stats_scenario_test_data',
      'type': 'none',
      'variables': {
        'test_data': [
          "../<(test_data_subdir)/conversion.txt",
          "../<(test_data_subdir)/prediction.txt",
          "../<(test_data_subdir)/suggestion.txt",
          "../<(test_data_subdir)/composition.txt",
          "../<(test_data_subdir)/select_prediction.txt",
          "../<(test_data_subdir)/select_minor_conversion.txt",
          "../<(test_data_subdir)/select_minor_prediction.txt",
          "../<(test_data_subdir)/mouse_select_from_suggestion.txt",
          "../<(test_data_subdir)/select_t13n_by_key.txt",
          "../<(test_data_subdir)/select_t13n_on_cascading_window.txt",
          "../<(test_data_subdir)/switch_kana_type.txt",
          "../<(test_data_subdir)/multiple_segments.txt",
          "../<(test_data_subdir)/select_candidates_in_multiple_segments.txt",
          "../<(test_data_subdir)/select_candidates_in_multiple_segments_and_expand_segment.txt",
          "../<(test_data_subdir)/continue_input.txt",
          "../<(test_data_subdir)/continuous_input.txt",
          "../<(test_data_subdir)/multiple_sessions.txt",
          "../<(test_data_subdir)/backspace_after_commit.txt",
          "../<(test_data_subdir)/backspace_after_commit_after_backspace.txt",
          "../<(test_data_subdir)/multiple_backspace_after_commit.txt",
          "../<(test_data_subdir)/zero_query_suggestion.txt",
          "../<(test_data_subdir)/auto_partial_suggestion.txt",
          "../<(test_data_subdir)/insert_space.txt",
          "../<(test_data_subdir)/numpad_in_direct_input_mode.txt",
        ],
        'test_data_subdir': 'data/test/session/scenario/usage_stats',
      },
      'includes': ['../gyp/install_testdata.gypi'],
    },
    {
      'target_name': 'session_handler_scenario_test',
      'type': 'executable',
      'sources': [
        'session_handler_scenario_test.cc'
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/testing.gyp:gtest_main',
        '../usage_stats/usage_stats_test.gyp:usage_stats_testing_util',
        'install_session_handler_scenario_test_data',
        'install_session_handler_usage_stats_scenario_test_data',
        'session.gyp:session_handler',
        'session_base.gyp:request_test_util',
        'session_base.gyp:session_protocol',
        'session_handler_test_util',
      ],
      'variables': {
        'test_size': 'large',
      },
    },
    {
      'target_name': 'session_handler_stress_test_main',
      'type': 'executable',
      'sources': [
        'session_handler_stress_test_main.cc'
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../config/config.gyp:config_handler',
        '../config/config.gyp:config_protocol',
        '../testing/testing.gyp:gtest_main',
        'session.gyp:random_keyevents_generator',
        'session.gyp:session_handler',
        'session_base.gyp:session_protocol',
        'session_handler_test_util',
      ],
    },

    # Test cases meta target: this target is referred from gyp/tests.gyp
    {
      'target_name': 'session_all_test',
      'type': 'none',
      'dependencies': [
        'generic_storage_manager_test',
        'random_keyevents_generator_test',
        'request_test_util_test',
        'session_converter_stress_test',
        'session_converter_test',
        'session_handler_scenario_test',
        'session_handler_stress_test',
        'session_handler_test',
        'session_internal_test',
        'session_module_test',
        'session_regression_test',
        'session_server_test',
        'session_test',
      ],
    },
  ],
}
