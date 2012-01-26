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
    'relative_dir': 'sync',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  'targets': [
    {
      'target_name': 'sync',
      'type': 'static_library',
      'sources': [
        '<(proto_out_dir)/<(relative_dir)/sync.pb.cc',
        'config_adapter.cc',
        'contact_list_util.cc',
        'contact_syncer.cc',
        'inprocess_service.cc',
        'learning_preference_adapter.cc',
        'learning_preference_sync_util.cc',
        'mock_syncer.cc',
        'sync_handler.cc',
        'sync_status_manager.cc',
        'sync_util.cc',
        'syncer.cc',
        'user_dictionary_adapter.cc',
        'user_dictionary_sync_util.cc',
        'user_history_adapter.cc',
        'user_history_sync_util.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:config_file_stream',
        '../client/client.gyp:client',
        '../config/config.gyp:config_handler',
        '../config/config.gyp:genproto_config',
        '../converter/converter.gyp:converter',
        '../ipc/ipc.gyp:ipc',
        '../net/net.gyp:net',
        '../protobuf/protobuf.gyp:protobuf',
        '../dictionary/dictionary.gyp:user_dictionary',
        '../dictionary/dictionary.gyp:genproto_dictionary',
        '../prediction/prediction.gyp:prediction',
        '../prediction/prediction.gyp:genproto_prediction',
        '../rewriter/rewriter.gyp:rewriter',
        '../session/session_base.gyp:genproto_session',
        '../session/session_base.gyp:session_protocol',
        '../storage/storage.gyp:storage',
        'genproto_sync',
        'gen_sync_data',
        'oauth2',
      ],
    },
    {
      'target_name': 'oauth2',
      'type': 'static_library',
      'sources': [
        '<(gen_out_dir)/oauth2_client.cc',
        'oauth2.cc',
        'oauth2_util.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../protobuf/protobuf.gyp:protobuf',
        '../storage/storage.gyp:storage',
        '<(DEPTH)/third_party/jsoncpp/jsoncpp.gyp:jsoncpp',
        'gen_sync_data',
      ],
    },
    {
      'target_name': 'oauth2_token_util',
      'type': 'static_library',
      'sources': [
        'oauth2_token_util.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
    },
    {
      'target_name': 'genproto_sync',
      'type': 'none',
      'sources': [
        'sync.proto',
      ],
      'includes': [
        '../protobuf/genproto.gypi',
      ],
    },
    {
      'target_name': 'gen_sync_data',
      'type': 'none',
      'actions': [
        {
          'action_name': 'gen_client_data',
          'inputs': [
            '../data/sync/client_data.txt',
            '../build_tools/replace_sync_data.py',
            'oauth2_client_template.cc',
          ],
          'outputs': [
            '<(gen_out_dir)/oauth2_client.cc',
          ],
          'action': [
            'python', '../build_tools/replace_sync_data.py',
            '--sync_data_file', '../data/sync/client_data.txt',
            '--input', 'oauth2_client_template.cc',
            '--output', '<(gen_out_dir)/oauth2_client.cc',
          ],
        },
      ],
    },
    {
      'target_name': 'sync_handler_main',
      'type': 'executable',
      'sources': [
        'sync_handler_main.cc',
       ],
      'dependencies': [
        'sync',
      ],
    },
    {
      'target_name': 'sync_test',
      'type': 'executable',
      'sources': [
         'config_adapter_test.cc',
         'inprocess_service_test.cc',
         'learning_preference_adapter_test.cc',
         'learning_preference_sync_util_test.cc',
         'oauth2_test.cc',
         'oauth2_util_test.cc',
         'oauth2_token_util_test.cc',
         'sync_handler_test.cc',
         'sync_status_manager_test.cc',
         'sync_util_test.cc',
         'syncer_test.cc',
         'user_dictionary_adapter_test.cc',
         'user_dictionary_sync_util_test.cc',
         'user_history_adapter_test.cc',
         'user_history_sync_util_test.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:config_file_stream',
        '../client/client.gyp:client_mock',
        '../config/config.gyp:config_handler',
        '../config/config.gyp:genproto_config',
        '../languages/japanese/japanese.gyp:language_dependent_spec_japanese',
        '../net/net.gyp:http_client_mock',
        '../testing/testing.gyp:gtest_main',
        'oauth2_token_util',
        'sync',
      ],
      'variables': {
        'test_size': 'large',
      }
    },
    # Test cases meta target: this target is referred from gyp/tests.gyp
    {
      'target_name': 'sync_all_test',
      'type': 'none',
      'dependencies': [
        'sync_test',
      ],
    },
  ],
}
