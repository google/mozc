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
  'targets': [
    {
      'target_name': 'config_handler_test',
      'type': 'executable',
      'sources': [
        'config_handler_test.cc',
      ],
      'dependencies': [
        '../base/absl.gyp:absl_random',
        '../base/absl.gyp:absl_strings',
        '../base/absl.gyp:absl_synchronization',
        "../base/absl.gyp:absl_time",
        '../base/base.gyp:base',
        '../base/base.gyp:clock',
        '../base/base_test.gyp:clock_mock',
        '../testing/testing.gyp:gtest_main',
        '../testing/testing.gyp:mozctest',
        'config.gyp:config_handler',
        'install_stats_config_util_test_data',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'install_stats_config_util_test_data',
      'type': 'none',
      'variables': {
        'test_data_subdir': 'data/test/config',
        'test_data': [
          '<(mozc_oss_src_dir)/<(test_data_subdir)/mac_config1.db',
          '<(mozc_oss_src_dir)/<(test_data_subdir)/linux_config1.db',
          '<(mozc_oss_src_dir)/<(test_data_subdir)/win_config1.db',
        ],
      },
      'includes': [ '../gyp/install_testdata.gypi' ],
    },
    {
      'target_name': 'stats_config_util_test',
      'type': 'executable',
      'sources': [
        'stats_config_util_test.cc',
      ],
      'dependencies': [
        '../testing/testing.gyp:gtest_main',
        '../testing/testing.gyp:mozctest',
        'config.gyp:stats_config_util',
        'install_stats_config_util_test_data',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'character_form_manager_test',
      'type': 'executable',
      'sources': [
        'character_form_manager_test.cc',
      ],
      'dependencies': [
        '../testing/testing.gyp:gtest_main',
        '../testing/testing.gyp:mozctest',
        'config.gyp:character_form_manager',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    # Test cases meta target: this target is referred from gyp/tests.gyp
    {
      'target_name': 'config_all_test',
      'type': 'none',
      'dependencies': [
        'character_form_manager_test',
        'config_handler_test',
        'stats_config_util_test',
      ],
    },
  ]
}
