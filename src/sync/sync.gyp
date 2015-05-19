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
      'target_name': 'oauth2',
      'type': 'static_library',
      'sources': [
        '<(gen_out_dir)/oauth2_client.cc',
        'oauth2.cc',
        'oauth2_util.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:http_client',
        '../net/net.gyp:jsoncpp',
        '../storage/storage.gyp:storage',
        'gen_sync_data#host',
      ],
      'conditions': [
        ['target_platform!="Android"', {
          'dependencies': [
            '../base/base.gyp:encryptor',  # 'oauth2_util.cc' depends on Encryptor except for Android.
          ],
        }],
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
      'target_name': 'oauth2_token_util_test',
      'type': 'executable',
      'variables': {
        'test_size': 'small',
      },
      'sources': [
        'oauth2_token_util_test.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/testing.gyp:gtest_main',
        'oauth2_token_util',
      ],
    },
    {
      'target_name': 'genproto_sync',
      'type': 'none',
      'toolsets': ['host'],
      'sources': [
        'sync.proto',
      ],
      'includes': [
        '../protobuf/genproto.gypi',
      ],
      'dependencies': [
        '../config/config.gyp:genproto_config',
        '../dictionary/dictionary_base.gyp:genproto_dictionary',
        '../prediction/prediction.gyp:genproto_prediction',
      ],
    },
    {
      'target_name': 'sync_protocol',
      'type': 'static_library',
      'hard_dependency': 1,
      'sources': [
        '<(proto_out_dir)/<(relative_dir)/sync.pb.cc',
      ],
      'dependencies': [
        '../config/config.gyp:config_protocol',
        '../dictionary/dictionary_base.gyp:dictionary_protocol',
        '../prediction/prediction.gyp:prediction_protocol',
        '../protobuf/protobuf.gyp:protobuf',
        'genproto_sync#host',
      ],
      'export_dependent_settings': [
        'genproto_sync#host',
      ],
    },
    {
      'target_name': 'gen_sync_data',
      'type': 'none',
      'toolsets': ['host'],
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
      'target_name': 'sync_base',
      'type': 'static_library',
      'sources': [
        'contact_list_util.cc',
        'contact_syncer.cc',
        'learning_preference_sync_util.cc',
        'mock_syncer.cc',
        'sync_status_manager.cc',
        'sync_util.cc',
        'user_dictionary_sync_util.cc',
        'user_history_sync_util.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../dictionary/dictionary_base.gyp:user_dictionary',
        '../prediction/prediction.gyp:prediction',
        '../prediction/prediction.gyp:prediction_protocol',
        '../session/session_base.gyp:session_protocol',
        'gen_sync_data#host',
        'oauth2',
        'oauth2_token_util',
        'sync_protocol',
      ],
    },
    {
      'target_name': 'sync_base_test',
      'type': 'executable',
      'variables': {
        'test_size': 'large',
      },
      'sources': [
        'learning_preference_sync_util_test.cc',
        'oauth2_test.cc',
        'oauth2_util_test.cc',
        'sync_status_manager_test.cc',
        'sync_util_test.cc',
        'user_dictionary_sync_util_test.cc',
        'user_history_sync_util_test.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:http_client_mock',
        '../testing/testing.gyp:gtest_main',
        'sync_base',
      ],
    },
    {
      'target_name': 'sync',
      'type': 'none',
      'dependencies': [
        'sync_base',
      ],
    },
    # Test cases meta target: this target is referred from gyp/tests.gyp
    {
      'target_name': 'sync_all_test',
      'type': 'none',
      'dependencies': [
        'oauth2_token_util_test',
        'sync_base_test',
      ],
    },
  ],
}
