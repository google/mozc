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
      'targets': [
        {
          'target_name': 'imm_util',
          'type': 'static_library',
          'sources': [
            'imm_registrar.cc',
            'imm_util.cc',
            'input_dll.cc',
            'keyboard_layout_id.cc',
          ],
          'dependencies': [
            '../../base/base.gyp:base',
          ]
        },
        {
          'target_name': 'imm_util_test',
          'type': 'executable',
          'sources': [
            'input_dll_test.cc',
            'keyboard_layout_id_test.cc',
          ],
          'dependencies': [
            '../../base/base.gyp:base',
            '../../session/session_base.gyp:session_protocol',
            '../../testing/testing.gyp:gtest_main',
            'imm_util',
          ],
          'variables': {
            'test_size': 'small',
          },
        },
        {
          'target_name': 'ime_base',
          'type': 'static_library',
          'sources': [
            'conversion_mode_util.cc',
            'migration_util.cc',
            'string_util.cc',
            'uninstall_helper.cc',
          ],
          'conditions': [
            ['branding=="GoogleJapaneseInput"', {
              'sources': [
                'omaha_util.cc',
              ],
            }],
          ],
          'dependencies': [
            '../../base/base.gyp:base',
            '../../config/config.gyp:config_protocol',
            '../../session/session_base.gyp:session_protocol',
            'imm_util',
          ],
        },
        {
          'target_name': 'win32_base_test',
          'type': 'executable',
          'sources': [
            'conversion_mode_util_test.cc',
            'string_util_test.cc',
            'uninstall_helper_test.cc',
          ],
          'dependencies': [
            '../../base/base.gyp:base',
            '../../session/session_base.gyp:session_protocol',
            '../../testing/testing.gyp:gtest_main',
            'ime_base',
          ],
          'conditions': [
            ['branding=="GoogleJapaneseInput"', {
              'sources': [
                'omaha_util_test.cc',
              ],
              'dependencies': [
                '../../testing/sidestep/sidestep.gyp:sidestep',
              ],
            }],
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
            'imm_util_test',
            'win32_base_test',
          ],
        }],
      ],
    },
  ],
}
