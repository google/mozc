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
    'relative_dir': 'base/win32',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  'targets': [
    # Test cases meta target: this target is referred from gyp/tests.gyp
    {
      'target_name': 'base_win32_all_test',
      'type': 'none',
      'dependencies': [],
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
              'com_test',
              'hresult_test',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'com_implements',
          'type': 'static_library',
          'sources': [
            'com_implements.cc',
          ],
        },
        {
          'target_name': 'win_font_test_helper',
          'type': 'static_library',
          'sources': [
            'win_font_test_helper.cc',
          ],
          'dependencies': [
            '../../testing/testing.gyp:mozctest',
            '../base.gyp:base',
          ],
          'copies': [
            {
              'files': [
                '<(DEPTH)/third_party/ipa_font/ipaexg.ttf',
                '<(DEPTH)/third_party/ipa_font/ipaexm.ttf',
              ],
              'destination': '<(SHARED_INTERMEDIATE_DIR)/third_party/ipa_font',
            },
          ],
        },
        {
          'target_name': 'hresult_test',
          'type': 'executable',
          'sources': [
            'hresult_test.cc',
            'hresultor_test.cc',
          ],
          'dependencies': [
            '../absl.gyp:absl_strings',
            '../base.gyp:base',
            '../../testing/testing.gyp:gtest_main',
          ],
          'variables': {
            'test_size': 'small',
          },
        },
        {
          'target_name': 'com_test',
          'type': 'executable',
          'sources': [
            'com_implements_test.cc',
            'com_test.cc',
          ],
          'dependencies': [
            ':com_implements',
            '../base.gyp:base',
            '../../testing/testing.gyp:gtest_main',
          ],
          'variables': {
            'test_size': 'small',
          },
        },
      ]},
    ],
  ],
}
