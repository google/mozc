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
      'target_name': 'renderer_client',
      'type': 'static_library',
      'sources': [
        'renderer_client.cc',
      ],
      'dependencies': [
        '../base/absl.gyp:absl_synchronization',
        '../base/base.gyp:base',
        '../base/base.gyp:version',
        '../ipc/ipc.gyp:ipc',
        '../protocol/protocol.gyp:commands_proto',
        '../protocol/protocol.gyp:config_proto',
        '../protocol/protocol.gyp:renderer_proto',
      ],
    },
    {
      'target_name': 'renderer_server',
      'type': 'static_library',
      'sources': [
        'renderer_server.cc',
      ],
      'dependencies': [
        '../base/absl.gyp:absl_strings',
        '../base/absl.gyp:absl_time',
        '../base/base.gyp:base',
        '../client/client.gyp:client',
        '../config/config.gyp:config_handler',
        '../ipc/ipc.gyp:ipc',
        '../protocol/protocol.gyp:commands_proto',
        '../protocol/protocol.gyp:renderer_proto',
      ],
    },
    {
      'target_name': 'init_mozc_renderer',
      'type': 'static_library',
      'sources': [
        'init_mozc_renderer.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:crash_report_handler',
        '../config/config.gyp:stats_config_util',
      ],
    },
    {
      'target_name': 'renderer_client_test',
      'type': 'executable',
      'sources': [
        'renderer_client_test.cc',
      ],
      'dependencies': [
        '../base/absl.gyp:absl_strings',
        '../base/base.gyp:version',
        '../testing/testing.gyp:gtest_main',
        'renderer_client',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'renderer_server_test',
      'type': 'executable',
      'sources': [
        'renderer_server_test.cc',
      ],
      'dependencies': [
        '../base/absl.gyp:absl_strings',
        '../base/absl.gyp:absl_time',
        '../ipc/ipc.gyp:ipc_test_util',
        '../testing/testing.gyp:gtest_main',
        '../testing/testing.gyp:mozctest',
        'renderer_client',
        'renderer_server',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'table_layout_test',
      'type': 'executable',
      'sources': [
        'table_layout_test.cc',
      ],
      'dependencies': [
        '../testing/testing.gyp:gtest_main',
        'table_layout',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'window_util_test',
      'type': 'executable',
      'sources': [
        'window_util_test.cc',
      ],
      'dependencies': [
        '../testing/testing.gyp:gtest_main',
        'window_util',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'renderer_style_handler',
      'type': 'static_library',
      'sources': [
        'renderer_style_handler.cc',
      ],
      'dependencies': [
        '../protocol/protocol.gyp:renderer_proto',
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
      ],
      'dependencies': [
        '../protocol/protocol.gyp:renderer_proto',
        '../testing/testing.gyp:gtest_main',
        'renderer_style_handler',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    # Test cases meta target: this target is referred from gyp/tests.gyp
    {
      'target_name': 'renderer_all_test',
      'type': 'none',
      'dependencies': [
        'renderer_client_test',
        'renderer_server_test',
        'renderer_style_handler_test',
        'table_layout_test',
        'window_util_test',
      ],
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            'win32_renderer_core_test',
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
          'toolsets': ['host'],
          'variables': {
            'gen_resource_proj_name': 'mozc_renderer',
            'gen_main_resource_path': 'renderer/win32/mozc_renderer.rc',
            'gen_output_resource_path':
                '<(gen_out_dir)/mozc_renderer_autogen.rc',
          },
          'includes': [
            '../gyp/gen_win32_resource_header.gypi',
          ],
        },
        {
          'target_name': 'win32_renderer_core',
          'type': 'static_library',
          'sources': [
            'win32/win32_image_util.cc',
            'win32/win32_renderer_util.cc',
          ],
          'dependencies': [
            '../base/base.gyp:base',
            '../protocol/protocol.gyp:commands_proto',
            '../protocol/protocol.gyp:config_proto',
            '../protocol/protocol.gyp:renderer_proto',
          ],
        },
        {
          'target_name': 'win32_renderer_core_test',
          'type': 'executable',
          'sources': [
            'win32/win32_image_util_test.cc',
            'win32/win32_renderer_util_test.cc',
          ],
          'dependencies': [
            '../base/win32/base_win32.gyp:win_font_test_helper',
            '<(mozc_oss_src_dir)/data/test/renderer/win32/test_data.gyp:install_test_data',
            '<(mozc_oss_src_dir)/data/test/renderer/win32/test_data.gyp:test_spec_proto',
            '../protobuf/protobuf.gyp:protobuf',
            '../testing/testing.gyp:gtest_main',
            '../testing/testing.gyp:mozctest',
            'win32_renderer_core',
          ],
          'variables': {
            'test_size': 'small',
          },
          'msvs_settings': {
            'VCLinkerTool': {
              'AdditionalDependencies': [
                'gdiplus.lib',  # used in 'win32_image_util_test.cc'
              ],
            },
          },
        },
        {
          'target_name': 'win32_text_renderer',
          'type': 'static_library',
          'sources': [
            'win32/text_renderer.cc',
          ],
          'dependencies': [
            '../base/base.gyp:base',
            '../protocol/protocol.gyp:renderer_proto',
            'renderer_style_handler',
          ],
          'link_settings': {
            'msvs_settings': {
              'VCLinkerTool': {
                'AdditionalDependencies': [
                  'd2d1.lib',
                  'dwrite.lib',
                ],
              },
            },
          },
        },
        {
          'target_name': 'gen_pbgra32_bitmap',
          'type': 'executable',
          'sources': [
            'win32/gen_pbgra32_bitmap.cc',
          ],
          'dependencies': [
            '../base/base.gyp:base_core',
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'AdditionalDependencies': [
                'gdiplus.lib',  # used in 'gen_pbgra32_bitmap.cc'
              ],
              'SubSystem': '1',  # 1 == subSystemConsole
            },
          },
        },
        {
          'target_name': 'mozc_renderer',
          'product_name': '<(renderer_product_name_win)',
          'type': 'executable',
          'sources': [
            'win32/win32_renderer_main.cc',
            'win32/win32_server.cc',
            'win32/window_manager.cc',
            'win32/candidate_window.cc',
            'win32/infolist_window.cc',
            'win32/indicator_window.cc',
            '<(gen_out_dir)/mozc_renderer_autogen.rc',
          ],
          'dependencies': [
            '../base/absl.gyp:absl_strings',
            '../base/absl.gyp:absl_synchronization',
            '../base/base.gyp:base',
            '../base/base.gyp:crash_report_handler',
            '../client/client.gyp:client',
            '../config/config.gyp:stats_config_util',
            '../ipc/ipc.gyp:ipc',
            '../protocol/protocol.gyp:commands_proto',
            '../protocol/protocol.gyp:config_proto',
            '../protocol/protocol.gyp:renderer_proto',
            'gen_mozc_renderer_resource_header#host',
            'renderer_server',
            'renderer_style_handler',
            'table_layout',
            'win32_renderer_core',
            'win32_text_renderer',
            'window_util',
            'init_mozc_renderer',
          ],
          'msvs_settings': {
            'VCManifestTool': {
              'AdditionalManifestFiles': 'win32/mozc_renderer.exe.manifest',
              'EmbedManifest': 'true',
            },
          },
        },
        {
          'target_name': 'win32_renderer_client',
          'type': 'static_library',
          'sources': [
            'win32/win32_renderer_client.cc',
          ],
          'dependencies': [
            '../base/absl.gyp:absl_synchronization',
            '../base/base.gyp:base',
            '../protocol/protocol.gyp:renderer_proto',
            'renderer_client',
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
            'mac/mac_renderer_main.cc',
            'mac/mac_server.mm',
            'mac/mac_server_send_command.mm',
            'mac/CandidateController.mm',
            'mac/CandidateWindow.mm',
            'mac/CandidateView.mm',
            'mac/InfolistWindow.mm',
            'mac/InfolistView.mm',
            'mac/RendererBaseWindow.mm',
            'mac/mac_view_util.mm',
          ],
          'mac_bundle_resources': [
            '<(mozc_oss_src_dir)/data/images/mac/candidate_window_logo.tiff',
            '<(mozc_oss_src_dir)/data/images/mac/product_icon.icns',
          ],
          'dependencies': [
            '../base/absl.gyp:absl_base',
            '../base/absl.gyp:absl_strings',
            '../base/absl.gyp:absl_synchronization',
            '../base/base.gyp:base',
            '../base/base.gyp:crash_report_handler',
            '../client/client.gyp:client',
            '../config/config.gyp:stats_config_util',
            '../ipc/ipc.gyp:ipc',
            '../protocol/protocol.gyp:commands_proto',
            '../protocol/protocol.gyp:config_proto',
            '../protocol/protocol.gyp:renderer_proto',
            'gen_renderer_files#host',
            'renderer_server',
            'renderer_style_handler',
            'table_layout',
            'window_util',
            'init_mozc_renderer',
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
          'toolsets': ['host'],
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
                '<(python)', '../build_tools/tweak_info_plist.py',
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
