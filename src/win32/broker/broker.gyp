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
    'relative_dir': 'win32',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
    'conditions': [
      ['branding=="GoogleJapaneseInput"', {
        'broker_product_name_win': 'GoogleIMEJaBroker',
      }, {  # else
        'broker_product_name_win': 'mozc_broker',
      }],
    ],
  },
  'conditions': [
    ['OS!="win"', {
      'targets': [
        {
          'target_name': 'mozc_broker_dummy_target',
          'type': 'none',
        },
      ],
    }],
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'gen_mozc_broker_resource_header',
          'variables': {
            'gen_resource_proj_name': 'mozc_broker',
            'gen_main_resource_path': 'win32/broker/mozc_broker.rc',
            'gen_output_resource_path':
                '<(gen_out_dir)/mozc_broker_autogen.rc',
          },
          'includes': [
            '../../gyp/gen_win32_resource_header.gypi',
          ],
        },
        {
          'target_name': 'mozc_broker',
          'product_name': '<(broker_product_name_win)',
          'type': 'executable',
          'sources': [
            'mozc_broker_main.cc',
          ],
          'dependencies': [
            '<(mozc_oss_src_dir)/base/base.gyp:base',
          ],
          'conditions': [
            ['OS=="win"', {
              'sources': [
                '<(gen_out_dir)/mozc_broker_autogen.rc',
                'prelauncher.cc',
              ],
              'dependencies': [
                '<(mozc_oss_src_dir)/client/client.gyp:client',
                '<(mozc_oss_src_dir)/protocol/protocol.gyp:config_proto',
                '<(mozc_oss_src_dir)/renderer/renderer.gyp:renderer_client',
                '../base/win32_base.gyp:ime_base',
                'gen_mozc_broker_resource_header',
              ],
              'msvs_settings': {
                'VCManifestTool': {
                  'AdditionalManifestFiles': 'mozc_broker.exe.manifest',
                  'EmbedManifest': 'true',
                },
              },
            }],
          ],
        },
      ],
    },],
  ],
}
