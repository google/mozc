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

{
  'variables': {
    'relative_dir': 'session',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  'targets': [
    {
      'target_name': 'session_test_util',
      'type' : 'static_library',
      'sources': [
        'session_test_util.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../composer/composer.gyp:composer',
        'session_base.gyp:request_test_util',
        'session_base.gyp:session_protocol',
      ],
    },
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
        '../testing/testing.gyp:testing',
        'session.gyp:session',
        'session.gyp:session_handler',
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
        '../data_manager/data_manager.gyp:user_pos_manager',
        '../rewriter/rewriter.gyp:rewriter',
        '../testing/testing.gyp:gtest_main',
        'session.gyp:session',
        'session.gyp:session_server',
        'session_test_util',
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
        ':session_test_util',
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
        '../testing/testing.gyp:gtest_main',
        'session.gyp:session',
        'session.gyp:session_server',
        'session_handler_test_util',
      ],
      'variables': {
        'test_size': 'small',
      },
      'conditions': [
        ['use_separate_connection_data==1', {
          'dependencies': [
            '../converter/converter.gyp:connection_data_injected_environment',
          ],
        }],
        ['use_separate_dictionary==1', {
          'dependencies': [
            '../dictionary/dictionary.gyp:dictionary_data_injected_environment',
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
        '../testing/testing.gyp:gtest_main',
        'session.gyp:session',
        'session_test_util',
      ],
    },
    {
      'target_name': 'session_module_test',
      'type': 'executable',
      'sources': [
        'ime_switch_util_test.cc',
        'key_event_util_test.cc',
        'key_parser_test.cc',
        'request_handler_test.cc',
        'session_observer_handler_test.cc',
        'session_usage_observer_test.cc',
        'session_watch_dog_test.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../client/client.gyp:client_mock',
        '../testing/testing.gyp:gtest_main',
        '../usage_stats/usage_stats.gyp:usage_stats_protocol',
        'session.gyp:session',
        'session.gyp:session_server',
        'session_base.gyp:ime_switch_util',
        'session_base.gyp:key_event_util',
        'session_base.gyp:key_parser',
        'session_base.gyp:session_protocol',
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
        'test_data': [
          '../<(test_data_subdir)/session_usage_observer_testcase1.txt',
          '../<(test_data_subdir)/session_usage_observer_testcase2.txt',
          '../<(test_data_subdir)/session_usage_observer_testcase3.txt',
          '../<(test_data_subdir)/session_usage_observer_testcase4.txt',
          '../<(test_data_subdir)/session_usage_observer_testcase5.txt',
          '../<(test_data_subdir)/session_usage_observer_testcase6.txt',
          '../<(test_data_subdir)/session_usage_observer_testcase7.txt',
          '../<(test_data_subdir)/session_usage_observer_testcase8.txt',
          '../<(test_data_subdir)/session_usage_observer_testcase9.txt',
          '../<(test_data_subdir)/session_usage_observer_testcase10.txt',
          '../<(test_data_subdir)/session_usage_observer_testcase11.txt',
          '../<(test_data_subdir)/session_usage_observer_testcase12.txt',
          '../<(test_data_subdir)/session_usage_observer_testcase13.txt',
          '../<(test_data_subdir)/session_usage_observer_testcase14.txt',
          '../<(test_data_subdir)/session_usage_observer_testcase15.txt',
          '../<(test_data_subdir)/session_usage_observer_testcase16.txt',
          '../<(test_data_subdir)/session_usage_observer_testcase17.txt',
        ],
        'test_data_subdir': 'data/test/session',
      },
      'includes': ['../gyp/install_testdata.gypi'],
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
        '../config/config.gyp:config_protocol',
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
        '../testing/testing.gyp:gtest_main',
        'session.gyp:random_keyevents_generator',
        'session.gyp:session',
        'session.gyp:session_server',
        'session_handler_test_util',
      ],
      'variables': {
        'test_size': 'small',
      },
      'conditions': [
        ['use_separate_connection_data==1', {
          'dependencies': [
            '../converter/converter.gyp:connection_data_injected_environment',
          ],
        }],
        ['use_separate_dictionary==1', {
          'dependencies': [
            '../dictionary/dictionary.gyp:dictionary_data_injected_environment',
          ],
        }],
      ],
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
