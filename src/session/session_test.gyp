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
      'target_name': 'session_handler_test_util',
      'type' : 'static_library',
      'sources': [
        'session_handler_test_util.cc',
      ],
      'dependencies': [
        '<(mozc_oss_src_dir)/base/base.gyp:base',
        '<(mozc_oss_src_dir)/config/config.gyp:config_handler',
        '<(mozc_oss_src_dir)/engine/engine.gyp:engine_factory',
        '<(mozc_oss_src_dir)/engine/engine.gyp:mock_data_engine_factory',
        '<(mozc_oss_src_dir)/protocol/protocol.gyp:commands_proto',
        '<(mozc_oss_src_dir)/protocol/protocol.gyp:config_proto',
        '<(mozc_oss_src_dir)/testing/testing.gyp:mozctest',
        '<(mozc_oss_src_dir)/testing/testing.gyp:testing',
        'session.gyp:session',
        'session.gyp:session_handler',
      ],
    },
    {
      'target_name': 'session_test',
      'type': 'executable',
      'sources': [
        'session_test.cc',
      ],
      'dependencies': [
        '<(mozc_oss_src_dir)/base/absl.gyp:absl_strings',
        '<(mozc_oss_src_dir)/data_manager/testing/mock_data_manager.gyp:mock_data_manager',
        '<(mozc_oss_src_dir)/engine/engine.gyp:engine',
        '<(mozc_oss_src_dir)/engine/engine.gyp:mock_data_engine_factory',
        '<(mozc_oss_src_dir)/request/request.gyp:request_test_util',
        '<(mozc_oss_src_dir)/rewriter/rewriter.gyp:rewriter',
        '<(mozc_oss_src_dir)/testing/testing.gyp:gtest_main',
        '<(mozc_oss_src_dir)/testing/testing.gyp:mozctest',
        'session.gyp:session',
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
        '<(mozc_oss_src_dir)/data_manager/testing/mock_data_manager.gyp:mock_data_manager',
        '<(mozc_oss_src_dir)/engine/engine.gyp:mock_data_engine_factory',
        '<(mozc_oss_src_dir)/request/request.gyp:request_test_util',
        '<(mozc_oss_src_dir)/testing/testing.gyp:gtest_main',
        '<(mozc_oss_src_dir)/testing/testing.gyp:mozctest',
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
        '<(mozc_oss_src_dir)/base/base_test.gyp:clock_mock',
        '<(mozc_oss_src_dir)/data_manager/oss/oss_data_manager.gyp:oss_data_manager',
        '<(mozc_oss_src_dir)/data_manager/testing/mock_data_manager.gyp:mock_data_manager',
        '<(mozc_oss_src_dir)/testing/testing.gyp:gtest_main',
        '<(mozc_oss_src_dir)/testing/testing.gyp:mozctest',
        'session.gyp:session',
        'session.gyp:session_server',
        'session_handler_test_util',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      # iOS is not supported.
      'target_name': 'session_watch_dog_test',
      'type': 'executable',
      'sources': [
        'session_watch_dog_test.cc',
      ],
      'dependencies': [
        '<(mozc_oss_src_dir)/base/absl.gyp:absl_time',
        '<(mozc_oss_src_dir)/base/base.gyp:base',
        '<(mozc_oss_src_dir)/testing/testing.gyp:gtest_main',
        'session.gyp:session_watch_dog',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'session_key_handling_test',
      'type': 'executable',
      'sources': [
        'key_info_util_test.cc',
      ],
      'dependencies': [
        '<(mozc_oss_src_dir)/base/base.gyp:base',
        '<(mozc_oss_src_dir)/config/config.gyp:config_handler',
        '<(mozc_oss_src_dir)/protocol/protocol.gyp:commands_proto',
        '<(mozc_oss_src_dir)/testing/testing.gyp:gtest_main',
        'session_base.gyp:key_info_util',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'session_internal_test',
      'type': 'executable',
      'sources': [
        'ime_context_test.cc',
        'keymap_test.cc',
        'key_event_transformer_test.cc',
      ],
      'dependencies': [
        '<(mozc_oss_src_dir)/base/base.gyp:base',
        '<(mozc_oss_src_dir)/protocol/protocol.gyp:commands_proto',
        '<(mozc_oss_src_dir)/protocol/protocol.gyp:config_proto',
        '<(mozc_oss_src_dir)/testing/testing.gyp:gtest_main',
        '<(mozc_oss_src_dir)/testing/testing.gyp:mozctest',
        '<(mozc_oss_src_dir)/testing/testing.gyp:testing_util',
        'session.gyp:session',
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
        '<(mozc_oss_src_dir)/engine/engine.gyp:engine_factory',
        '<(mozc_oss_src_dir)/request/request.gyp:request_test_util',
        '<(mozc_oss_src_dir)/testing/testing.gyp:gtest_main',
        '<(mozc_oss_src_dir)/testing/testing.gyp:mozctest',
        'session.gyp:random_keyevents_generator',
        'session.gyp:session',
        'session.gyp:session_handler_tool',
        'session.gyp:session_server',
        'session_handler_test_util',
      ],
      'variables': {
        'test_size': 'large',
      },
    },
    {
      'target_name': 'random_keyevents_generator_test',
      'type': 'executable',
      'sources': [
        'random_keyevents_generator_test.cc',
      ],
      'dependencies': [
        '<(mozc_oss_src_dir)/protocol/protocol.gyp:commands_proto',
        '<(mozc_oss_src_dir)/testing/testing.gyp:gtest_main',
        'session.gyp:random_keyevents_generator',
      ],
      'variables': {
        'test_size': 'large',
      },
    },
    {
      'target_name': 'session_handler_scenario_test',
      'type': 'executable',
      'sources': [
        'session_handler_scenario_test.cc'
      ],
      'dependencies': [
        '<(mozc_oss_src_dir)/base/absl.gyp:absl_status',
        '<(mozc_oss_src_dir)/base/absl.gyp:absl_strings',
        '<(mozc_oss_src_dir)/base/base.gyp:base',
        '<(mozc_oss_src_dir)/base/base.gyp:number_util',
        '<(mozc_oss_src_dir)/data/test/session/scenario/scenario.gyp:install_session_handler_scenario_test_data',
        '<(mozc_oss_src_dir)/engine/engine.gyp:mock_data_engine_factory',
        '<(mozc_oss_src_dir)/protocol/protocol.gyp:candidate_window_proto',
        '<(mozc_oss_src_dir)/protocol/protocol.gyp:commands_proto',
        '<(mozc_oss_src_dir)/request/request.gyp:request_test_util',
        '<(mozc_oss_src_dir)/testing/testing.gyp:gtest_main',
        '<(mozc_oss_src_dir)/testing/testing.gyp:mozctest',
        'session.gyp:session_handler',
        'session.gyp:session_handler_tool',
        'session_handler_test_util',
      ],
      'variables': {
        'test_size': 'large',
      },
    },

    # Test cases meta target: this target is referred from gyp/tests.gyp
    {
      'target_name': 'session_all_test',
      'type': 'none',
      'dependencies': [
        ## Stress tests and scenario tests are disabled,
        ## because they take long time (~100sec in total).
        ## Those tests are checked by other tools (e.g. Bazel test).
        # 'session_handler_scenario_test',
        # 'session_handler_stress_test',
        'random_keyevents_generator_test',
        'session_handler_test',
        'session_key_handling_test',
        'session_internal_test',
        'session_regression_test',
        'session_test',
        'session_watch_dog_test',
      ],
      'conditions': [
      ],
    },
  ],
}
