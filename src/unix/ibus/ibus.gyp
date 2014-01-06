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

{
  'variables': {
    'relative_dir': 'unix/ibus',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
    'ibus_mozc_icon_path%': '/usr/share/ibus-mozc/product_icon.png',
    'ibus_mozc_path%': '/usr/lib/ibus-mozc/ibus-engine-mozc',
    # enable_x11_selection_monitor represents if ibus_mozc uses X11 selection
    # monitor or not.
    'enable_x11_selection_monitor%': 1,
  },
  'targets': [
    {
      # Meta target to set up build environment for ibus. Required 'cflags'
      # and 'link_settings' will be automatically injected into any target
      # which directly or indirectly depends on this target.
      'target_name': 'ibus_build_environment',
      'type': 'none',
      'variables': {
        'target_libs': [
          'glib-2.0',
          'gobject-2.0',
          'ibus-1.0',
        ],
      },
      'all_dependent_settings': {
        'cflags': [
          '<!@(<(pkg_config_command) --cflags <@(target_libs))',
        ],
        'link_settings': {
          'libraries': [
            '<!@(<(pkg_config_command) --libs-only-l <@(target_libs))',
          ],
          'ldflags': [
            '<!@(<(pkg_config_command) --libs-only-L <@(target_libs))',
          ],
        },
      },
    },
    {
      'target_name': 'gen_mozc_xml',
      'type': 'none',
      'actions': [
        {
          'action_name': 'gen_mozc_xml',
          'inputs': [
            './gen_mozc_xml.py',
          ],
          'outputs': [
            '<(gen_out_dir)/mozc.xml',
          ],
          'action': [
            'python', '../../build_tools/redirect.py',
            '<(gen_out_dir)/mozc.xml',
            './gen_mozc_xml.py',
            '--branding=Mozc',
            '--server_dir=<(server_dir)',
            '--pkg_config_command=<(pkg_config_command)',
            '--ibus_mozc_path=<(ibus_mozc_path)',
            '--ibus_mozc_icon_path=<(ibus_mozc_icon_path)',
          ],
        },
      ],
    },
    {
      'target_name': 'ibus_mozc_metadata',
      'type': 'static_library',
      'sources': [
        'mozc_engine_property.cc',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../session/session_base.gyp:session_protocol',
        'ibus_build_environment',
      ],
    },
    {
      'target_name': 'ibus_property_handler',
      'type': 'static_library',
      'sources': [
        'property_handler.cc',
      ],
      'dependencies': [
        '../../session/session_base.gyp:session_protocol',
        'ibus_build_environment',
        'message_translator',
        'path_util',
      ],
    },
    {
      'target_name': 'path_util',
      'type': 'static_library',
      'sources': [
        'path_util.cc',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
      ],
    },
    {
      'target_name': 'message_translator',
      'type': 'static_library',
      'sources': [
        'message_translator.cc',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
      ],
    },
    {
      'target_name': 'ibus_mozc_lib',
      'type': 'static_library',
      'sources': [
        'engine_registrar.cc',
        'ibus_candidate_window_handler.cc',
        'key_event_handler.cc',
        'key_translator.cc',
        'mozc_engine.cc',
        'preedit_handler.cc',
        'surrounding_text_util.cc',
      ],
      'dependencies': [
        '../../client/client.gyp:client',
        '../../session/session_base.gyp:ime_switch_util',
        '../../session/session_base.gyp:session_protocol',
        'ibus_property_handler',
        'message_translator',
        'path_util',
      ],
      'conditions': [
        ['enable_gtk_renderer==1', {
          'dependencies': [
            'gtk_candidate_window_handler',
          ],
        }],
        ['enable_x11_selection_monitor==1', {
          'dependencies': [
            'selection_monitor',
          ],
        }],
      ],
    },
    {
      'target_name': 'ibus_mozc',
      'type': 'executable',
      'sources': [
        'main.cc',
        '<(gen_out_dir)/main.h',
      ],
      'actions': [
        {
          'action_name': 'gen_main_h',
          'inputs': [
            './gen_mozc_xml.py',
          ],
          'outputs': [
            '<(gen_out_dir)/main.h',
          ],
          'action': [
            'python', '../../build_tools/redirect.py',
            '<(gen_out_dir)/main.h',
            './gen_mozc_xml.py',
            '--branding=Mozc',
            '--output_cpp',
            '--ibus_mozc_path=<(ibus_mozc_path)',
            '--ibus_mozc_icon_path=<(ibus_mozc_icon_path)',
          ],
        },
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        'gen_mozc_xml',
        'ibus_mozc_lib',
        'ibus_mozc_metadata',
      ],
    },
    {
      'target_name': 'ibus_mozc_test',
      'type': 'executable',
      'sources': [
        'key_event_handler_test.cc',
        'key_translator_test.cc',
        'message_translator_test.cc',
        'mozc_engine_test.cc',
        'path_util_test.cc',
        'surrounding_text_util_test.cc',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../client/client.gyp:client',
        '../../client/client.gyp:client_mock',
        '../../session/session_base.gyp:key_event_util',
        '../../session/session_base.gyp:session_protocol',
        '../../testing/testing.gyp:gtest_main',
        'ibus_mozc_lib',
        'ibus_mozc_metadata',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    # Test cases meta target: this target is referred from gyp/tests.gyp
    {
      'target_name': 'ibus_all_test',
      'type': 'none',
      'conditions': [
        ['enable_gtk_renderer==1', {
          'dependencies': [
            'gtk_candidate_window_handler_test',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['enable_gtk_renderer==1', {
      'targets': [
        {
          'target_name': 'gtk_candidate_window_handler',
          'type': 'static_library',
          'sources': [
            'gtk_candidate_window_handler.cc',
          ],
          'dependencies': [
            '../../renderer/renderer.gyp:renderer_client',
            '../../renderer/renderer.gyp:renderer_protocol',
            'ibus_build_environment',
          ],
        },
        {
          'target_name': 'gtk_candidate_window_handler_test',
          'type': 'executable',
          'sources': [
            'gtk_candidate_window_handler_test.cc',
          ],
          'dependencies': [
            'gtk_candidate_window_handler',
            '../../testing/testing.gyp:gtest_main',
          ],
        },
      ],
    }],
    ['enable_x11_selection_monitor==1', {
      'targets': [
        {
          # Meta target to set up build environment for ibus. Required 'cflags'
          # and 'link_settings' will be automatically injected into any target
          # which directly or indirectly depends on this target.
          'target_name': 'xcb_build_environment',
          'type': 'none',
          'variables': {
            'target_libs': [
              'xcb',
              'xcb-xfixes',
            ],
          },
          'all_dependent_settings': {
            'cflags': [
              '<!@(<(pkg_config_command) --cflags <@(target_libs))',
            ],
            'link_settings': {
              'libraries': [
                '<!@(<(pkg_config_command) --libs-only-l <@(target_libs))',
              ],
              'ldflags': [
                '<!@(<(pkg_config_command) --libs-only-L <@(target_libs))',
              ],
            },
            'defines': [
              'MOZC_ENABLE_X11_SELECTION_MONITOR=1'
            ],
          },
        },
        {
          'target_name': 'selection_monitor',
          'type': 'static_library',
          'sources': [
            'selection_monitor.cc',
          ],
          'dependencies': [
            '../../base/base.gyp:base',
            'xcb_build_environment',
          ],
        },
      ],
    }],
  ],
}
