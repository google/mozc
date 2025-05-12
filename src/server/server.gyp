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
    'relative_dir': 'server',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
    'conditions': [
      ['branding=="GoogleJapaneseInput"', {
        'converter_product_name_win': 'GoogleIMEJaConverter',
      }, {  # else
        'converter_product_name_win': 'mozc_server',
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'mozc_server',
      'type': 'executable',
      'sources': [
        'mozc_server_main.cc',
      ],
      'dependencies': [
        '<(mozc_oss_src_dir)/base/base.gyp:base',
        '<(mozc_oss_src_dir)/engine/engine.gyp:engine_factory',
        '<(mozc_oss_src_dir)/session/session.gyp:session',
        'mozc_server_lib',
      ],
      'conditions': [
        ['OS=="mac"', {
          'product_name': '<(branding)Converter',
          'sources': [
            'Info.plist',
          ],
          'dependencies': [
            'gen_mozc_server_info_plist',
          ],
          'mac_bundle': 1,
          'mac_bundle_resources': [
            '<(mozc_oss_src_dir)/data/images/mac/product_icon.icns'
          ],
          'xcode_settings': {
            'INFOPLIST_FILE': '<(gen_out_dir)/Info.plist',
          },
          'variables': {
            # This product name is used in postbuilds_mac.gypi.
            'product_name': '<(branding)Converter',
          },
          'includes': [
            '../gyp/postbuilds_mac.gypi',
          ],
        }],
        ['OS=="win"', {
          'product_name': '<(converter_product_name_win)',
          'sources': [
            '<(gen_out_dir)/mozc_server_autogen.rc',
          ],
          'dependencies': [
            'gen_mozc_server_resource_header',
          ],
          'msvs_settings': {
            'VCManifestTool': {
              'AdditionalManifestFiles': 'mozc_server.exe.manifest',
              'EmbedManifest': 'true',
            },
          },
        }],
      ],
    },
    {
      'target_name': 'mozc_server_lib',
      'type': 'static_library',
      'sources': [
        'mozc_server.cc',
      ],
      'dependencies': [
        '<(mozc_oss_src_dir)/session/session.gyp:session_server',
        '<(mozc_oss_src_dir)/protocol/protocol.gyp:state_proto',
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'genproto_server',
          'type': 'none',
          'sources': [
            'win32_service_state.proto',
          ],
          'includes': [
            '../protobuf/genproto.gypi',
          ],
        },
        {
          'target_name': 'server_protocol',
          'type': 'static_library',
          'hard_dependency': 1,
          'sources': [
            '<(proto_out_dir)/<(relative_dir)/win32_service_state.pb.cc',
          ],
          'dependencies': [
            '<(mozc_oss_src_dir)/protobuf/protobuf.gyp:protobuf',
            'genproto_server',
          ],
          'export_dependent_settings': [
            'genproto_server',
          ],
        },
        {
          'target_name': 'gen_mozc_server_resource_header',
          'variables': {
            'gen_resource_proj_name': 'mozc_server',
            'gen_main_resource_path': 'server/mozc_server.rc',
            'gen_output_resource_path':
                '<(gen_out_dir)/mozc_server_autogen.rc',
          },
          'includes': [
            '../gyp/gen_win32_resource_header.gypi',
          ],
        },
      ],
    }],
    ['OS=="mac"', {
      'targets': [
        {
          'target_name': 'gen_mozc_server_info_plist',
          'type': 'none',
          'actions': [
            {
              'action_name': 'generate',
              'inputs': [
                'Info.plist',
              ],
              'outputs': [
                '<(gen_out_dir)/Info.plist',
              ],
              'action': [
                '<(python)', '<(mozc_oss_src_dir)/build_tools/tweak_info_plist.py',
                '--output', '<(gen_out_dir)/Info.plist',
                '--input', 'Info.plist',
                '--version_file', '<(mozc_src_dir)/mozc_version.txt',
                '--branding', '<(branding)',
              ],
            },
          ],
        },
      ],
    }],
  ],
}
