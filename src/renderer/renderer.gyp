# Copyright 2010-2011, Google Inc.
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
    'relative_dir': 'renderer',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
    'conditions': [
      ['branding=="GoogleJapaneseInput"', {
        'renderer_product_name_win': 'GoogleIMEJaRenderer',
      }, {  # else
        'renderer_product_name_win': 'mozc_renderer',
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'table_layout',
      'type': 'static_library',
      'sources': [
        'table_layout.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
    },
    {
      'target_name': 'window_util',
      'type': 'static_library',
      'sources': [
        'window_util.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
    },
    {
      'target_name': 'renderer',
      'type': 'static_library',
      'sources': [
        'renderer_client.cc',
        'renderer_server.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../client/client.gyp:client',
        '../config/config.gyp:config_handler',
        '../config/config.gyp:genproto_config',
        '../ipc/ipc.gyp:ipc',
        '../session/session_base.gyp:genproto_session'
      ],
    },
    {
      'target_name': 'renderer_test',
      'type': 'executable',
      'sources': [
        'renderer_client_test.cc',
        'renderer_server_test.cc',
        'table_layout_test.cc',
        'window_util_test.cc',
      ],
      'dependencies': [
        '../ipc/ipc.gyp:ipc_test_util',
        '../languages/japanese/japanese.gyp:language_dependent_spec_japanese',
        '../testing/testing.gyp:gtest_main',
        'renderer',
        'table_layout',
        'window_util',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'renderer_style_handler_test',
      'type': 'executable',
      'sources': [
        'renderer_style_handler_test.cc',
        'renderer_style_handler.cc',
        '<(proto_out_dir)/<(relative_dir)/renderer_style.pb.cc',
      ],
      'dependencies': [
        '../testing/testing.gyp:gtest_main',
        'renderer',
        'genproto_renderer',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'genproto_renderer',
      'type': 'none',
      'sources': [
        'renderer_style.proto',
      ],
      'includes': [
        '../protobuf/genproto.gypi',
      ],
    },
    # Test cases meta target: this target is referred from gyp/tests.gyp
    {
      'target_name': 'renderer_all_test',
      'type': 'none',
      'dependencies': [
        'renderer_test',
      ],
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            'win32_renderer_util_test',
          ],
        }],
        ['OS=="mac"', {
          'dependencies': [
            'renderer_style_handler_test',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'gen_mozc_renderer_resource_header',
          'variables': {
            'gen_resource_proj_name': 'mozc_renderer',
            'gen_main_resource_path': 'renderer/mozc_renderer.rc',
            'gen_output_resource_path':
                '<(gen_out_dir)/mozc_renderer_autogen.rc',
          },
          'includes': [
            '../gyp/gen_win32_resource_header.gypi',
          ],
        },
        {
          'target_name': 'win32_renderer_util',
          'type': 'static_library',
          'sources': [
            'win32/win32_renderer_util.cc',
          ],
          'dependencies': [
            '../base/base.gyp:base',
            '../config/config.gyp:genproto_config',
            '../session/session_base.gyp:genproto_session',
            '../session/session_base.gyp:session_protocol',
            '../config/config.gyp:genproto_config',
            '../config/config.gyp:config_protocol',
            '../win32/base/win32_base.gyp:ime_base',
          ],
        },
        {
          'target_name': 'win32_renderer_util_test',
          'type': 'executable',
          'sources': [
            'win32/win32_renderer_util_test.cc',
          ],
          'dependencies': [
            '../testing/testing.gyp:gtest_main',
            'win32_renderer_util',
          ],
          'variables': {
            'test_size': 'small',
          },
        },
        {
          'target_name': 'mozc_renderer',
          'product_name': '<(renderer_product_name_win)',
          'type': 'executable',
          'sources': [
            'mozc_renderer_main.cc',
            'mozc_renderer.exe.manifest',
            'renderer_style_handler.cc',
            'win32/win32_server.cc',
            'win32/window_manager.cc',
            'win32/candidate_window.cc',
            'win32/composition_window.cc',
            'win32/infolist_window.cc',
            'win32/text_renderer.cc',
            '<(gen_out_dir)/mozc_renderer_autogen.rc',
            '<(proto_out_dir)/<(relative_dir)/renderer_style.pb.cc',
          ],
          'dependencies': [
            '../base/base.gyp:base',
            '../client/client.gyp:client',
            '../config/config.gyp:genproto_config',
            '../config/config.gyp:stats_config_util',
            '../ipc/ipc.gyp:ipc',
            '../session/session_base.gyp:genproto_session',
            'gen_mozc_renderer_resource_header',
            'renderer',
            'table_layout',
            'win32_renderer_util',
            'window_util',
            'genproto_renderer',
          ],
          'includes': [
            '../gyp/postbuilds_win.gypi',
          ],
        },
      ],
    }],
    ['OS=="mac"', {
      'targets': [
        {
          'target_name': 'mozc_renderer',
          'type': 'executable',
          'mac_bundle': 1,
          'product_name': '<(branding)Renderer',
          'sources': [
            'mozc_renderer_main.cc',
            'mac/mac_server.mm',
            'mac/mac_server_send_command.mm',
            'mac/CandidateController.mm',
            'mac/CandidateWindow.mm',
            'mac/CandidateView.mm',
            'mac/InfolistWindow.mm',
            'mac/InfolistView.mm',
            'mac/RendererBaseWindow.mm',
            'mac/mac_view_util.mm',
            'renderer_style_handler.cc',
            '<(proto_out_dir)/<(relative_dir)/renderer_style.pb.cc',
          ],
          'mac_bundle_resources': [
            '../data/images/mac/candidate_window_logo.png',
            '../data/images/mac/product_icon.icns',
          ],
          'dependencies': [
            '../base/base.gyp:base',
            '../client/client.gyp:client',
            '../config/config.gyp:genproto_config',
            '../config/config.gyp:stats_config_util',
            '../ipc/ipc.gyp:ipc',
            '../session/session_base.gyp:genproto_session',
            'gen_renderer_files',
            'renderer',
            'table_layout',
            'window_util',
            'genproto_renderer',
          ],
          'xcode_settings': {
            'INFOPLIST_FILE': '<(gen_out_dir)/Info.plist',
          },
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/Carbon.framework',
            ],
          },

          'variables': {
            # This product name is used in postbuilds_mac.gypi.
            'product_name': '<(branding)Renderer',
          },
          'includes': [
            '../gyp/postbuilds_mac.gypi',
          ],
        },
        {
          'target_name': 'gen_renderer_files',
          'type': 'none',
          'actions': [
            {
              'action_name': 'generate_infoplist',
              'inputs': [
                'mac/Info.plist',
              ],
              'outputs': [
                '<(gen_out_dir)/Info.plist',
              ],
              'action': [
                'python', '../build_tools/tweak_info_plist.py',
                '--output', '<(gen_out_dir)/Info.plist',
                '--input', 'mac/Info.plist',
                '--version_file', '../mozc_version.txt',
                '--branding', '<(branding)',
              ],
            },
          ],
        },
      ],
    }],
  ],
}
