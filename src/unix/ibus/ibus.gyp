# Copyright 2010-2012, Google Inc.
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
      '../../languages/japanese/japanese.gyp:language_dependent_spec_japanese',
      '../../prediction/prediction.gyp:prediction',
      '../../rewriter/rewriter.gyp:rewriter',
      '../../session/session.gyp:session',
      '../../session/session.gyp:session_server',
      '../../storage/storage.gyp:storage',
      '../../transliteration/transliteration.gyp:transliteration',
    ],
    'ibus_client_dependencies' : [
      '../../client/client.gyp:client',
      '../../session/session_base.gyp:genproto_session',
    ],
    'ibus_standalone_dependencies' : [
      '../../base/base.gyp:config_file_stream',
      '../../config/config.gyp:config_handler',
      '../../net/net.gyp:net',
      '../../session/session.gyp:session_handler',
      '../../usage_stats/usage_stats.gyp:usage_stats',
    ],
  },
  'targets': [
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
    },
    {
      'target_name': 'ibus_mozc_metadata',
      'type': 'static_library',
      'sources': [
        'mozc_engine_property.cc',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../session/session_base.gyp:genproto_session',
      ],
      'includes': [
        'ibus_libraries.gypi',
      ],
    },
    {
      'target_name': 'ibus_mozc_lib',
      'type': 'static_library',
      'sources': [
        'engine_registrar.cc',
        'key_translator.cc',
        'mozc_engine.cc',
        'path_util.cc',
      ],
      'includes': [
        'ibus_libraries.gypi',
      ],
      'dependencies': [
        '../../session/session_base.gyp:genproto_session',
        '../../session/session_base.gyp:ime_switch_util',
      ],
      'conditions': [
        ['chromeos==1', {
          'sources': [
            'client.cc',
            'config_util.cc',
          ],
          'dependencies+': [
            '<@(ibus_standalone_dependencies)',
          ],
        },{
          'dependencies+': [
            '<@(ibus_client_dependencies)',
          ]
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
      'includes': [
        'ibus_libraries.gypi',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../languages/japanese/japanese.gyp:language_dependent_spec_japanese',
        'gen_mozc_xml',
        'ibus_mozc_lib',
        'ibus_mozc_metadata',
      ],
      'conditions': [
        ['chromeos==1', {
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
        'key_translator_test.cc',
        'mozc_engine_test.cc',
        'path_util_test.cc',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../client/client.gyp:client_mock',
        '../../testing/testing.gyp:gtest_main',
        'ibus_mozc_lib',
        'ibus_mozc_metadata',
        '<@(ibus_client_dependencies)',
        '<@(ibus_standalone_dependencies)',
        '<@(ibus_japanese_standalone_dependencies)',
      ],
      'conditions': [
        ['chromeos!=1', {
         # config_util.cc is included in ibus_mozc_lib only for chromeos==1
         'sources': [
           'config_util.cc',
         ],
         'dependencies': [
           '../../client/client.gyp:client_mock',
         ],
        }],
      ],
      'includes': [
        'ibus_libraries.gypi',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
  ],
  'conditions': [
    ['chromeos==1', {
      'targets': [
        {
          'target_name': 'chromeos_client_test',
          'type': 'executable',
          'sources': [
            'client_test.cc',
          ],
          'dependencies': [
            '../../base/base.gyp:base',
            '../../testing/testing.gyp:testing',
            'ibus_mozc_lib',
            'ibus_mozc_metadata',
            '<@(ibus_standalone_dependencies)',
            '<@(ibus_japanese_standalone_dependencies)',
          ],
          'includes': [
            'ibus_libraries.gypi',
          ],
          'variables': {
            'test_size': 'small',
          },
        }],
    }],
  ],
}
