# Copyright 2010-2014, Google Inc.
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
    'relative_dir': 'win32/custom_action',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
    'conditions': [
      ['branding=="GoogleJapaneseInput"', {
        'custom_action_dll_product_name_win': 'GoogleIMEJaInstallerHelper',
      }, {  # else
        'custom_action_dll_product_name_win': 'mozc_installer_helper',
      }],
    ],
  },
  'conditions': [
    ['OS!="win"', {
      'targets': [
        {
          'target_name': 'mozc_installer_helper_dummy_target',
          'type': 'none',
        },
      ],
    }],
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'gen_custom_action_resource_header',
          'variables': {
            'gen_resource_proj_name': 'custom_action',
            'gen_main_resource_path': 'win32/custom_action/custom_action.rc',
            'gen_output_resource_path':
                '<(gen_out_dir)/custom_action_autogen.rc',
          },
          'includes': [
            '../../gyp/gen_win32_resource_header.gypi',
          ],
        },
        {
          'target_name': 'mozc_custom_action32',
          'product_name': '<(custom_action_dll_product_name_win)32',
          'includes': [
            'custom_action.gypi',
          ],
        },
        {
          'target_name': 'mozc_custom_action64',
          'product_name': '<(custom_action_dll_product_name_win)64',
          'includes': [
            'custom_action.gypi',
          ],
        },
      ],
    },],
  ],
}
