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
    'relative_dir': 'unix/ibus',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
    'ibus_japanese_standalone_dependencies': [
      '../../composer/composer.gyp:composer',
      '../../converter/converter.gyp:converter',
      '../../dictionary/dictionary.gyp:dictionary',
      '../../prediction/prediction.gyp:prediction',
      '../../rewriter/rewriter.gyp:rewriter',
      '../../session/session.gyp:session',
      '../../session/session.gyp:session_server',
      '../../storage/storage.gyp:storage',
      '../../transliteration/transliteration.gyp:transliteration',
    ],
    'ibus_client_dependencies' : [
      '../../client/client.gyp:client',
      '../../session/session_base.gyp:session_protocol',
    ],
    'ibus_standalone_dependencies' : [
      '../../base/base.gyp:config_file_stream',
      '../../config/config.gyp:config_handler',
      '../../session/session.gyp:session_handler',
      '../../usage_stats/usage_stats_base.gyp:usage_stats',
    ],
    'conditions': [
      # enable_x11_selection_monitor represents if ibus_mozc uses X11 selection
      # monitor or not.
      ['target_platform=="Linux" and language=="japanese"', {
        'enable_x11_selection_monitor%': 1,
      }, { # else
        # Currently X11 selection monitor is not enabled on ChromeOS build and
        # non-Japanese build.
        # TODO(nona): Enable it for ChromeOS or other languages if applicable.
        'enable_x11_selection_monitor': 0,
      }],
    ],
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
          'conditions': [
            ['target_platform=="ChromeOS" and branding=="GoogleJapaneseInput"', {
              'action': [
                'python', '../../build_tools/redirect.py',
                '<(gen_out_dir)/mozc.xml',
                './gen_mozc_xml.py',
                '--platform=ChromeOS',
                '--branding=GoogleJapaneseInput',
                '--pkg_config_command=<(pkg_config_command)',
              ],
            }],
            ['target_platform=="ChromeOS" and branding!="GoogleJapaneseInput"', {
              'action': [
                'python', '../../build_tools/redirect.py',
                '<(gen_out_dir)/mozc.xml',
                './gen_mozc_xml.py',
                '--platform=ChromeOS',
                '--branding=Mozc',
                '--pkg_config_command=<(pkg_config_command)',
              ],
            }],
            ['target_platform!="ChromeOS"', {
              'action': [
                'python', '../../build_tools/redirect.py',
                '<(gen_out_dir)/mozc.xml',
                './gen_mozc_xml.py',
                '--platform=Linux',
                '--branding=Mozc',
                '--server_dir=<(server_dir)',
                '--pkg_config_command=<(pkg_config_command)',
              ],
            }],
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
        'config_util.cc',
        'engine_registrar.cc',
        'ibus_candidate_window_handler.cc',
        'key_event_handler.cc',
        'key_translator.cc',
        'mozc_engine.cc',
        'preedit_handler.cc',
        'surrounding_text_util.cc',
      ],
      'dependencies': [
        '../../session/session_base.gyp:ime_switch_util',
        '../../session/session_base.gyp:session_protocol',
        'ibus_property_handler',
        'message_translator',
        'path_util',
      ],
      'conditions': [
        ['target_platform=="ChromeOS"', {
          'sources': [
            'client.cc',
          ],
          'dependencies+': [
            '<@(ibus_standalone_dependencies)',
          ],
        },{ # else (i.e. target_platform!="ChromeOS")
          'dependencies+': [
            '<@(ibus_client_dependencies)',
          ]
        }],
        ['enable_gtk_renderer==1', {
          'dependencies+': [
            'gtk_candidate_window_handler',
          ],
        }],
        ['enable_x11_selection_monitor==1', {
          'dependencies+': [
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
          'conditions': [
            ['target_platform=="ChromeOS" and branding=="GoogleJapaneseInput"', {
              'action': [
                'python', '../../build_tools/redirect.py',
                '<(gen_out_dir)/main.h',
                './gen_mozc_xml.py',
                '--platform=ChromeOS',
                '--branding=GoogleJapaneseInput',
                '--output_cpp'
              ],
            }],
            ['target_platform=="ChromeOS" and branding!="GoogleJapaneseInput"', {
              'action': [
                'python', '../../build_tools/redirect.py',
                '<(gen_out_dir)/main.h',
                './gen_mozc_xml.py',
                '--platform=ChromeOS',
                '--branding=Mozc',
                '--output_cpp'
              ],
            }],
            ['target_platform!="ChromeOS"', {
              'action': [
                'python', '../../build_tools/redirect.py',
                '<(gen_out_dir)/main.h',
                './gen_mozc_xml.py',
                '--platform=Linux',
                '--branding=Mozc',
                '--output_cpp'
              ],
            }],
          ],
        },
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../engine/engine.gyp:engine_factory',
        'gen_mozc_xml',
        'ibus_mozc_lib',
        'ibus_mozc_metadata',
      ],
      'conditions': [
        ['target_platform=="ChromeOS"', {
         'dependencies+': [
           '<@(ibus_standalone_dependencies)',
           '<@(ibus_japanese_standalone_dependencies)',
         ],
        }, {
         'dependencies+': [
           '<@(ibus_client_dependencies)',
         ],
        }],
      ],
    },
    {
      'target_name': 'ibus_mozc_test',
      'type': 'executable',
      'sources': [
        'config_util_test.cc',
        'key_event_handler_test.cc',
        'key_translator_test.cc',
        'message_translator_test.cc',
        'mozc_engine_test.cc',
        'path_util_test.cc',
        'surrounding_text_util_test.cc',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../client/client.gyp:client_mock',
        '../../session/session_base.gyp:key_event_util',
        '../../testing/testing.gyp:gtest_main',
        'ibus_mozc_lib',
        'ibus_mozc_metadata',
        '<@(ibus_client_dependencies)',
        '<@(ibus_standalone_dependencies)',
        '<@(ibus_japanese_standalone_dependencies)',
      ],
      'conditions': [
        ['target_platform!="ChromeOS"', {
            # config_util.cc is included in ibus_mozc_lib only for ChromeOS
         'sources': [
           'config_util.cc',
         ],
         'dependencies': [
           '../../client/client.gyp:client_mock',
         ],
        }],
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
        ['target_platform=="ChromeOS"', {
          'dependencies': [
            'chromeos_client_test',
          ],
        }],
        ['enable_gtk_renderer==1', {
          'dependencies': [
            'gtk_candidate_window_handler_test',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['target_platform=="ChromeOS"', {
      'targets': [
        {
          'target_name': 'chromeos_client_test',
          'type': 'executable',
          'sources': [
            'client_test.cc',
          ],
          'dependencies': [
            '../../base/base.gyp:base',
            '../../engine/engine.gyp:mock_converter_engine',
            '../../session/session_base.gyp:session_protocol',
            '../../testing/testing.gyp:googletest_lib',
            '../../testing/testing.gyp:testing',
            'ibus_mozc_lib',
            'ibus_mozc_metadata',
            '<@(ibus_standalone_dependencies)',
            '<@(ibus_japanese_standalone_dependencies)',
          ],
          'variables': {
            'test_size': 'small',
          },
        }],
    }],
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
