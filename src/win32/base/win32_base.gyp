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

# This build file is expected to be used on Windows only.
{
  'variables': {
    'relative_dir': 'win32/base',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  'targets': [
    {
      'target_name': 'win32_base_dummy',
      'type': 'none',
      'sources': [
        # empty
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      # TODO(yukawa): Refactor following targets when the implementation of
      #     TSF Mozc is completed.
      'targets': [
        {
          'target_name': 'input_dll_import_lib',
          'type': 'shared_library',
          'sources': [
            'input_dll.cc',
            'input_dll.def',
          ],
          'dependencies': [
            '<(mozc_oss_src_dir)/base/base.gyp:base',
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'AdditionalOptions': [
                '/ignore:4070',
              ],
            },
          },
        },
        {
          'target_name': 'msctf_dll_import_lib',
          'type': 'shared_library',
          'sources': [
            'msctf_dll.cc',
            'msctf_dll.def',
          ],
          'dependencies': [
            '<(mozc_oss_src_dir)/base/base.gyp:base',
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'AdditionalOptions': [
                '/ignore:4070',
              ],
            },
          },
        },
        {
          'target_name': 'msctf_dll_test',
          'type': 'executable',
          'sources': [
            'msctf_dll_test.cc',
          ],
          'dependencies': [
            '<(mozc_oss_src_dir)/base/base.gyp:base',
            '<(mozc_oss_src_dir)/testing/testing.gyp:gtest_main',
            'msctf_dll_import_lib',
          ],
          'variables': {
            'test_size': 'small',
          },
        },
        {
          'target_name': 'imframework_util',
          'type': 'static_library',
          'sources': [
            'imm_reconvert_string.cc',
            'imm_util.cc',
            'keyboard_layout_id.cc',
            'tsf_profile.cc',
            'tsf_registrar.cc',
          ],
          'dependencies': [
            '<(mozc_oss_src_dir)/base/base.gyp:base',
            'input_dll_import_lib',
            'msctf_dll_import_lib',
          ],
        },
        {
          'target_name': 'imframework_util_test',
          'type': 'executable',
          'sources': [
            'imm_reconvert_string_test.cc',
            'input_dll_test.cc',
            'keyboard_layout_id_test.cc',
          ],
          'dependencies': [
            '<(mozc_oss_src_dir)/base/base.gyp:base',
            '<(mozc_src_dir)/protocol/protocol.gyp:commands_proto',
            '<(mozc_oss_src_dir)/testing/testing.gyp:gtest_main',
            'imframework_util',
          ],
          'variables': {
            'test_size': 'small',
          },
        },
        {
          'target_name': 'ime_impl_base',
          'type': 'static_library',
          'sources': [
            'config_snapshot.cc',
            'conversion_mode_util.cc',
            'deleter.cc',
            'indicator_visibility_tracker.cc',
            'keyboard.cc',
            'keyevent_handler.cc',
            'string_util.cc',
            'surrogate_pair_observer.cc',
            'win32_window_util.cc',
          ],
          'dependencies': [
            '<(mozc_oss_src_dir)/base/absl.gyp:absl_strings',
            '<(mozc_oss_src_dir)/base/base.gyp:base',
            '<(mozc_oss_src_dir)/config/config.gyp:config_handler',
            '<(mozc_src_dir)/protocol/protocol.gyp:commands_proto',
            '<(mozc_src_dir)/protocol/protocol.gyp:config_proto',
            '<(mozc_oss_src_dir)/session/session_base.gyp:key_info_util',
          ],
        },
        {
          'target_name': 'ime_impl_base_test',
          'type': 'executable',
          'sources': [
            'conversion_mode_util_test.cc',
            'deleter_test.cc',
            'indicator_visibility_tracker_test.cc',
            'keyboard_test.cc',
            'keyevent_handler_test.cc',
            'string_util_test.cc',
            'surrogate_pair_observer_test.cc',
          ],
          'dependencies': [
            '<(mozc_oss_src_dir)/base/base_test.gyp:clock_mock',
            '<(mozc_oss_src_dir)/client/client.gyp:client',
            '<(mozc_oss_src_dir)/testing/testing.gyp:gtest_main',
            '<(mozc_oss_src_dir)/testing/testing.gyp:mozctest',
            'ime_impl_base',
          ],
        },
        {
          'target_name': 'ime_base',
          'type': 'static_library',
          'sources': [
            'omaha_util.cc',
            'uninstall_helper.cc',
          ],
          'dependencies': [
            '<(mozc_oss_src_dir)/base/base.gyp:base',
            'imframework_util',
            'msctf_dll_import_lib',
          ],
        },
        {
          'target_name': 'win32_base_test',
          'type': 'executable',
          'sources': [
            'omaha_util_test.cc',
            'uninstall_helper_test.cc',
          ],
          'dependencies': [
            '<(mozc_oss_src_dir)/base/base.gyp:base',
            '<(mozc_src_dir)/protocol/protocol.gyp:commands_proto',
            '<(mozc_oss_src_dir)/testing/testing.gyp:gtest_main',
            'ime_base',
          ],
          'variables': {
            'test_size': 'small',
          },
        },
        {
          'target_name': 'text_icon',
          'type': 'static_library',
          'sources': [
            'text_icon.cc',
          ],
          'dependencies': [
            '<(mozc_oss_src_dir)/base/absl.gyp:absl_strings',
            '<(mozc_oss_src_dir)/base/base.gyp:base',
          ],
        },
        {
          'target_name': 'text_icon_test',
          'type': 'executable',
          'sources': [
            'text_icon_test.cc',
          ],
          'dependencies': [
            '<(mozc_oss_src_dir)/base/win32/base_win32.gyp:win_font_test_helper',
            '<(mozc_oss_src_dir)/testing/testing.gyp:gtest_main',
            'text_icon',
          ],
          'variables': {
            'test_size': 'small',
          },
        },
      ],
    }],
  ],
  'targets': [
    # Test cases meta target: this target is referred from gyp/tests.gyp
    {
      'target_name': 'win32_base_all_test',
      'type': 'none',
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            'ime_impl_base_test',
            'imframework_util_test',
            'msctf_dll_test',
            'text_icon_test',
            'win32_base_test',
          ],
        }],
      ],
    },
  ],
}
