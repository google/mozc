# Copyright 2010-2013, Google Inc.
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
        '../base/base.gyp:base',
        '../config/config.gyp:config_protocol',
        '../ipc/ipc.gyp:ipc',
        '../session/session_base.gyp:session_protocol',
        'renderer_protocol',
      ],
    },
    {
      'target_name': 'renderer_server',
      'type': 'static_library',
      'sources': [
        'renderer_server.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../client/client.gyp:client',
        '../config/config.gyp:config_handler',
        '../ipc/ipc.gyp:ipc',
        '../session/session_base.gyp:session_protocol',
        'renderer_protocol',
      ],
      'conditions': [
        ['enable_webservice_infolist==1', {
          'dependencies': [
            'webservice_infolist_handler',
          ],
        }],
      ],
    },
    {
      'target_name': 'renderer_client_test',
      'type': 'executable',
      'sources': [
        'renderer_client_test.cc',
      ],
      'dependencies': [
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
        '../ipc/ipc.gyp:ipc_test_util',
        '../testing/testing.gyp:gtest_main',
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
        'renderer_protocol',
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
        '../testing/testing.gyp:gtest_main',
        'renderer_protocol',
        'renderer_style_handler',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'genproto_renderer',
      'type': 'none',
      'toolsets': ['host'],
      'sources': [
        'renderer_command.proto',
        'renderer_style.proto',
      ],
      'includes': [
        '../protobuf/genproto.gypi',
      ],
    },
    {
      'target_name': 'renderer_protocol',
      'type': 'static_library',
      'hard_dependency': 1,
      'sources': [
        '<(proto_out_dir)/<(relative_dir)/renderer_command.pb.cc',
        '<(proto_out_dir)/<(relative_dir)/renderer_style.pb.cc',
      ],
      'dependencies': [
        '../config/config.gyp:config_protocol',
        '../protobuf/protobuf.gyp:protobuf',
        '../session/session_base.gyp:session_protocol',
        'genproto_renderer#host'
      ],
      'export_dependent_settings': [
        'genproto_renderer#host',
      ],
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
        ['enable_webservice_infolist==1', {
          'dependencies': [
            'webservice_infolist_handler_test',
          ],
        }],
        ['OS=="win"', {
          'dependencies': [
            'win32_font_util_test',
            'win32_renderer_core_test',
          ],
        }],
        ['enable_gtk_renderer==1', {
          'dependencies': [
            'gtk_renderer_test',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['enable_webservice_infolist==1', {
      'targets': [
        {
          'target_name': 'webservice_infolist_handler',
          'type': 'static_library',
          'sources': [
            'webservice_infolist_handler.cc',
          ],
          'dependencies': [
            '../base/base.gyp:base',
            '../config/config.gyp:config_handler',
            '../config/config.gyp:genproto_config#host',
            '../libxml/libxml.gyp:libxml',
            '../net/net.gyp:jsonpath',
            '../net/net.gyp:http_client',
            '../session/session_base.gyp:session_protocol',
            'renderer_protocol',
          ],
          'include_dirs' : [
            '../libxml/libxml2-2.7.7/include',
          ],
        },
        {
          'target_name': 'webservice_infolist_handler_test',
          'type': 'executable',
          'sources': [
            'webservice_infolist_handler_test.cc',
          ],
          'dependencies': [
            '../net/net.gyp:http_client_mock',
            '../testing/testing.gyp:gtest_main',
            'renderer_server',
            'webservice_infolist_handler',
          ],
          'variables': {
            'test_size': 'small',
          },
        },
      ],
    }],
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'gen_mozc_renderer_resource_header',
          'toolsets': ['host'],
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
          'target_name': 'win32_font_util',
          'type': 'static_library',
          'sources': [
            'win32/win32_font_util.cc',
          ],
          'dependencies': [
            '../base/base.gyp:base',
            '../config/config.gyp:config_protocol',
            '../session/session_base.gyp:session_protocol',
            'renderer_protocol',
          ],
        },
        {
          'target_name': 'win32_font_util_test',
          'type': 'executable',
          'sources': [
            'win32/win32_font_util_test.cc',
          ],
          'dependencies': [
            '../testing/testing.gyp:gtest_main',
            'win32_font_util',
          ],
          'variables': {
            'test_size': 'small',
          },
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
            '../config/config.gyp:config_protocol',
            '../session/session_base.gyp:session_protocol',
            'renderer_protocol',
            'win32_font_util',
          ],
        },
        {
          'target_name': 'install_renderer_core_test_data',
          'type': 'none',
          'variables': {
            'test_data': [
              '../<(test_data_subdir)/balloon_blur_alpha_-1.png',
              '../<(test_data_subdir)/balloon_blur_alpha_-1.png.json',
              '../<(test_data_subdir)/balloon_blur_alpha_0.png',
              '../<(test_data_subdir)/balloon_blur_alpha_0.png.json',
              '../<(test_data_subdir)/balloon_blur_alpha_10.png',
              '../<(test_data_subdir)/balloon_blur_alpha_10.png.json',
              '../<(test_data_subdir)/balloon_blur_color_32_64_128.png',
              '../<(test_data_subdir)/balloon_blur_color_32_64_128.png.json',
              '../<(test_data_subdir)/balloon_blur_offset_-20_-10.png',
              '../<(test_data_subdir)/balloon_blur_offset_-20_-10.png.json',
              '../<(test_data_subdir)/balloon_blur_offset_0_0.png',
              '../<(test_data_subdir)/balloon_blur_offset_0_0.png.json',
              '../<(test_data_subdir)/balloon_blur_offset_20_5.png',
              '../<(test_data_subdir)/balloon_blur_offset_20_5.png.json',
              '../<(test_data_subdir)/balloon_blur_sigma_0.0.png',
              '../<(test_data_subdir)/balloon_blur_sigma_0.0.png.json',
              '../<(test_data_subdir)/balloon_blur_sigma_0.5.png',
              '../<(test_data_subdir)/balloon_blur_sigma_0.5.png.json',
              '../<(test_data_subdir)/balloon_blur_sigma_1.0.png',
              '../<(test_data_subdir)/balloon_blur_sigma_1.0.png.json',
              '../<(test_data_subdir)/balloon_blur_sigma_2.0.png',
              '../<(test_data_subdir)/balloon_blur_sigma_2.0.png.json',
              '../<(test_data_subdir)/balloon_frame_thickness_-1.png',
              '../<(test_data_subdir)/balloon_frame_thickness_-1.png.json',
              '../<(test_data_subdir)/balloon_frame_thickness_0.png',
              '../<(test_data_subdir)/balloon_frame_thickness_0.png.json',
              '../<(test_data_subdir)/balloon_frame_thickness_1.5.png',
              '../<(test_data_subdir)/balloon_frame_thickness_1.5.png.json',
              '../<(test_data_subdir)/balloon_frame_thickness_3.png',
              '../<(test_data_subdir)/balloon_frame_thickness_3.png.json',
              '../<(test_data_subdir)/balloon_inside_color_32_64_128.png',
              '../<(test_data_subdir)/balloon_inside_color_32_64_128.png.json',
              '../<(test_data_subdir)/balloon_no_label.png',
              '../<(test_data_subdir)/balloon_no_label.png.json',
              '../<(test_data_subdir)/balloon_tail_bottom.png',
              '../<(test_data_subdir)/balloon_tail_bottom.png.json',
              '../<(test_data_subdir)/balloon_tail_left.png',
              '../<(test_data_subdir)/balloon_tail_left.png.json',
              '../<(test_data_subdir)/balloon_tail_right.png',
              '../<(test_data_subdir)/balloon_tail_right.png.json',
              '../<(test_data_subdir)/balloon_tail_top.png',
              '../<(test_data_subdir)/balloon_tail_top.png.json',
              '../<(test_data_subdir)/balloon_tail_width_height_-10_-10.png',
              '../<(test_data_subdir)/balloon_tail_width_height_-10_-10.png.json',
              '../<(test_data_subdir)/balloon_tail_width_height_0_0.png',
              '../<(test_data_subdir)/balloon_tail_width_height_0_0.png.json',
              '../<(test_data_subdir)/balloon_tail_width_height_10_20.png',
              '../<(test_data_subdir)/balloon_tail_width_height_10_20.png.json',
              '../<(test_data_subdir)/balloon_width_height_40_30.png',
              '../<(test_data_subdir)/balloon_width_height_40_30.png.json',
            ],
            'test_data_subdir': 'data/test/renderer/win32',
          },
          'includes': ['../gyp/install_testdata.gypi'],
        },
        {
          'target_name': 'win32_renderer_core_test',
          'type': 'executable',
          'sources': [
            'win32/win32_image_util_test.cc',
            'win32/win32_renderer_util_test.cc',
          ],
          'dependencies': [
            '../net/net.gyp:jsoncpp',
            '../testing/testing.gyp:gtest_main',
            'install_renderer_core_test_data',
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
          # Use IPAex font, which contains IVS characters for b/2876066
          'copies': [
            {
              'files': [
                '<(DEPTH)/third_party/ipa_font/ipaexg.ttf',
                '<(DEPTH)/third_party/ipa_font/ipaexm.ttf',
              ],
              'destination': '<(PRODUCT_DIR)/data',
            },
          ],
        },
        {
          'target_name': 'win32_text_renderer',
          'type': 'static_library',
          'sources': [
            'win32/text_renderer.cc',
          ],
          'dependencies': [
            '../base/base.gyp:base',
            'renderer_protocol',
            'renderer_style_handler',
          ],
        },
        {
          'target_name': 'mozc_renderer',
          'product_name': '<(renderer_product_name_win)',
          'type': 'executable',
          'sources': [
            'mozc_renderer_main.cc',
            'win32/win32_server.cc',
            'win32/window_manager.cc',
            'win32/candidate_window.cc',
            'win32/composition_window.cc',
            'win32/infolist_window.cc',
            '<(gen_out_dir)/mozc_renderer_autogen.rc',
          ],
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:crash_report_handler',
            '../client/client.gyp:client',
            '../config/config.gyp:config_protocol',
            '../config/config.gyp:stats_config_util',
            '../ipc/ipc.gyp:ipc',
            '../session/session_base.gyp:session_protocol',
            'gen_mozc_renderer_resource_header#host',
            'renderer_protocol',
            'renderer_server',
            'renderer_style_handler',
            'table_layout',
            'win32_renderer_core',
            'win32_text_renderer',
            'window_util',
          ],
          'msvs_settings': {
            'VCManifestTool': {
              'AdditionalManifestFiles': 'mozc_renderer.exe.manifest',
              'EmbedManifest': 'false',
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
            '../base/base.gyp:base',
            'renderer.gyp:renderer_client',
            'renderer.gyp:renderer_protocol',
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
          ],
          'mac_bundle_resources': [
            '../data/images/mac/candidate_window_logo.tiff',
            '../data/images/mac/product_icon.icns',
          ],
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:crash_report_handler',
            '../client/client.gyp:client',
            '../config/config.gyp:config_protocol',
            '../config/config.gyp:stats_config_util',
            '../ipc/ipc.gyp:ipc',
            '../session/session_base.gyp:session_protocol',
            'gen_renderer_files#host',
            'renderer_protocol',
            'renderer_server',
            'renderer_style_handler',
            'table_layout',
            'window_util',
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
    ['enable_gtk_renderer==1', {
      'targets': [
        {
          # Meta target to set up build environment for gtk+-2.0.
          # Required 'cflags' and 'link_settings' will be automatically
          # injected into any target which directly or indirectly depends
          # on this target.
          'target_name': 'gtk2_build_environment',
          'type': 'none',
          'variables': {
            'target_pkgs' : [
              'glib-2.0',
              'gobject-2.0',
              'gthread-2.0',
              'gtk+-2.0',
              'gdk-2.0',
            ],
          },
          'all_dependent_settings': {
            'cflags': [
              '<!@(<(pkg_config_command) --cflags <@(target_pkgs))',
            ],
            'link_settings': {
              'libraries': [
                '<!@(<(pkg_config_command) --libs-only-l <@(target_pkgs))',
              ],
              'ldflags': [
                '<!@(<(pkg_config_command) --libs-only-L <@(target_pkgs))',
              ],
            },
          },
        },
        {
          'target_name': 'mozc_renderer_lib',
          'type': 'static_library',
          'sources': [
            'unix/cairo_factory.cc',
            'unix/cairo_wrapper.cc',
            'unix/candidate_window.cc',
            'unix/draw_tool.cc',
            'unix/font_spec.cc',
            'unix/gtk_window_base.cc',
            'unix/gtk_wrapper.cc',
            'unix/infolist_window.cc',
            'unix/pango_wrapper.cc',
            'unix/text_renderer.cc',
            'unix/unix_renderer.cc',
            'unix/unix_server.cc',
            'unix/window_manager.cc',
          ],
          'dependencies': [
            '../base/base.gyp:base',
            '../client/client.gyp:client',
            '../config/config.gyp:genproto_config#host',
            '../config/config.gyp:stats_config_util',
            '../ipc/ipc.gyp:ipc',
            '../session/session_base.gyp:genproto_session#host',
            'renderer_protocol',
            'gtk2_build_environment',
            'renderer_server',
            'renderer_style_handler',
            'table_layout',
            'window_util',
          ],
        },
        {
          'target_name': 'mozc_renderer',
          'type': 'executable',
          'sources': [
            'mozc_renderer_main.cc',
          ],
          'dependencies': [
            '../base/base.gyp:crash_report_handler',
            'mozc_renderer_lib',
          ],
        },
        {
          'target_name': 'gtk_renderer_test',
          'type': 'executable',
          'sources': [
            'unix/candidate_window_test.cc',
            'unix/draw_tool_test.cc',
            'unix/font_spec_test.cc',
            'unix/gtk_window_base_test.cc',
            'unix/infolist_window_test.cc',
            'unix/text_renderer_test.cc',
            'unix/unix_renderer_test.cc',
            'unix/unix_server_test.cc',
            'unix/window_manager_test.cc',
          ],
          'dependencies': [
            '../testing/testing.gyp:gtest_main',
            'mozc_renderer_lib',
          ],
        },
      ],
    }],
  ],
}
