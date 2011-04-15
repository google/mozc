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
    'relative_dir': 'unix/ibus',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
    'pkg_config_libs': [
      'glib-2.0',
      'gobject-2.0',
      'ibus-1.0',
      'libcurl',
    ],
    'ibus_client_dependencies' : [
      '../../client/client.gyp:client',
      '../../session/session.gyp:ime_switch_util',
    ],
    'ibus_standalone_dependencies' : [
      '../../base/base.gyp:config_file_stream',
      '../../composer/composer.gyp:composer',
      '../../converter/converter.gyp:converter',
      '../../dictionary/dictionary.gyp:dictionary',
      '../../net/net.gyp:net',
      '../../prediction/prediction.gyp:prediction',
      '../../rewriter/rewriter.gyp:rewriter',
      '../../session/session.gyp:genproto_session',
      '../../session/session.gyp:session',
      '../../storage/storage.gyp:storage',
      '../../transliteration/transliteration.gyp:transliteration',
      '../../usage_stats/usage_stats.gyp:usage_stats',
    ],
  },
  'targets': [
    {
      'target_name': 'ibus_mozc_lib',
      'type': 'static_library',
      'sources': [
        'engine_registrar.cc',
        'key_translator.cc',
        'mozc_engine.cc',
        'mozc_engine_property.cc',
        'path_util.cc',
      ],
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
            ['chromeos==1 and branding=="GoogleJapaneseInput"', {
              'action': [
                'python', '../../build_tools/redirect.py',
                '<(gen_out_dir)/mozc.xml',
                './gen_mozc_xml.py',
                '--platform=ChromeOS',
                '--branding=GoogleJapaneseInput',
              ],
            }],
            ['chromeos==1 and branding!="GoogleJapaneseInput"', {
              'action': [
                'python', '../../build_tools/redirect.py',
                '<(gen_out_dir)/mozc.xml',
                './gen_mozc_xml.py',
                '--platform=ChromeOS',
                '--branding=Mozc',
              ],
            }],
            ['chromeos!=1', {
              'action': [
                'python', '../../build_tools/redirect.py',
                '<(gen_out_dir)/mozc.xml',
                './gen_mozc_xml.py',
                '--platform=Linux',
                '--branding=Mozc',
              ],
            }],
          ],
        },
      ],
      'conditions': [
        ['chromeos==1', {
         'sources': [
           'session.cc',
           'config_util.cc',
         ],
        }],
       ],
      'cflags': [
        '<!@(pkg-config --cflags <@(pkg_config_libs))',
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
            ['chromeos==1 and branding=="GoogleJapaneseInput"', {
              'action': [
                'python', '../../build_tools/redirect.py',
                '<(gen_out_dir)/main.h',
                './gen_mozc_xml.py',
                '--platform=ChromeOS',
                '--branding=GoogleJapaneseInput',
                '--output_cpp'
              ],
            }],
            ['chromeos==1 and branding!="GoogleJapaneseInput"', {
              'action': [
                'python', '../../build_tools/redirect.py',
                '<(gen_out_dir)/main.h',
                './gen_mozc_xml.py',
                '--platform=ChromeOS',
                '--branding=Mozc',
                '--output_cpp'
              ],
            }],
            ['chromeos!=1', {
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
        'ibus_mozc_lib',
      ],
      'conditions': [
        ['chromeos==1', {
         'dependencies+': [
           '<@(ibus_standalone_dependencies)',
         ],
        }, {
         'dependencies+': [
           '<@(ibus_client_dependencies)',
         ],
        }],
       ],
      'cflags': [
        '<!@(pkg-config --cflags <@(pkg_config_libs))',
      ],
      'libraries': [
        '<!@(pkg-config --libs-only-l <@(pkg_config_libs))',
      ],
      'ldflags': [
        '<!@(pkg-config --libs-only-L <@(pkg_config_libs))',
      ],
    },
    {
      'target_name': 'ibus_mozc_test',
      'type': 'executable',
      'sources': [
        'key_translator_test.cc',
        'mozc_engine_test.cc',
        'path_util_test.cc',
        'session_test.cc',
        'config_util_test.cc',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../testing/testing.gyp:gtest_main',
        'ibus_mozc_lib',
      ],
      'conditions': [
        ['chromeos==1', {
         'dependencies+': [
           '<@(ibus_standalone_dependencies)',
         ],
        }, {
         'dependencies+': [
           '<@(ibus_client_dependencies)',
         ],
        }],
       ],
      'cflags': [
        '<!@(pkg-config --cflags <@(pkg_config_libs))',
      ],
      'libraries': [
        '<!@(pkg-config --libs-only-l <@(pkg_config_libs))',
      ],
      'ldflags': [
        '<!@(pkg-config --libs-only-L <@(pkg_config_libs))',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
  ],
}
