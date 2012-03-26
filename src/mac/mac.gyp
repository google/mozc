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
    'relative_dir': 'mac',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
    'build_type': 'stable',
  },
  # Add a dummy target because at least one target is needed in a gyp file.
  'targets': [
    {
      'target_name': 'mac_test',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../client/client.gyp:client_mock',
        '../config/config.gyp:config_protocol',
        '../renderer/renderer.gyp:renderer',
        '../renderer/renderer.gyp:renderer_protocol',
        '../renderer/renderer.gyp:table_layout',
        '../renderer/renderer.gyp:window_util',
        '../session/session_base.gyp:ime_switch_util',
        '../session/session_base.gyp:session_protocol',
        '../testing/testing.gyp:gtest_main',
        'gen_key_mappings',
      ],
      'variables': {
        'test_size': 'small',
      },
      'conditions': [
        ['OS=="mac"', {
          'sources': [
            'GoogleJapaneseInputController.mm',
            'GoogleJapaneseInputController_test.mm',
            'GoogleJapaneseInputServer.mm',
            'GoogleJapaneseInputServer_test.mm',
            'KeyCodeMap.mm',
            'KeyCodeMap_test.mm',
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/InputMethodKit.framework',
            ],
          },
          'xcode_settings': {
            'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
          },
        }],
        ['OS=="win"', {
          'msvs_settings': {
            'VCLinkerTool': {
              # W/O this flag, an error occurs during building mac_test
              # on Windows.
              'GenerateManifest': 'false',
            },
          },
        }],
      ],
    },
    # Test cases meta target: this target is referred from gyp/tests.gyp
    {
      'target_name': 'mac_all_test',
      'type': 'none',
      'conditions': [
        ['OS=="mac"', {
          'dependencies': [
            'mac_test',
          ],
        },],
      ],
    },
    {
      'target_name': 'gen_key_mappings',
      'type': 'none',
      'actions': [
        {
          'action_name': 'mac-kana',
          'variables': {
            'input_file': '../data/preedit/mac-kana.tsv',
            'output_file': '<(gen_out_dir)/init_kanamap.h',
          },
          'inputs': [
            '<(input_file)',
          ],
          'outputs': [
            '<(output_file)',
          ],
          'action': [
            'python', '../build_tools/redirect.py',
            '<(output_file)',
            'generate_mapping.py',
            '--mapname=KanaMap',
            '--result_type=const char *',
            '--filename=<(input_file)',
          ],
        },
        {
          'action_name': 'mac-specialkeys',
          'variables': {
            'input_file': '../data/preedit/mac-specialkeys.tsv',
            'output_file': '<(gen_out_dir)/init_specialkeymap.h',
          },
          'inputs': [
            '<(input_file)',
          ],
          'outputs': [
            '<(output_file)',
          ],
          'action': [
            'python', '../build_tools/redirect.py',
            '<(output_file)',
            'generate_mapping.py',
            '--mapname=SpecialKeyMap',
            '--result_type=KeyEvent::SpecialKey',
            '--filename=<(input_file)',
          ],
        },
        {
          'action_name': 'mac-specialchars',
          'variables': {
            'input_file': '../data/preedit/mac-specialchars.tsv',
            'output_file': '<(gen_out_dir)/init_specialcharmap.h',
          },
          'inputs': [
            '<(input_file)',
          ],
          'outputs': [
            '<(output_file)',
          ],
          'action': [
            'python', '../build_tools/redirect.py',
            '<(output_file)',
            'generate_mapping.py',
            '--mapname=SpecialCharMap',
            '--key_type=unichar',
            '--result_type=KeyEvent::SpecialKey',
            '--filename=<(input_file)',
          ],
        },
      ],
    },
  ],
  'conditions': [
    ['OS=="mac"', {
      'conditions': [
        ['channel_dev==1', {
          'variables': {
            'build_type': 'dev',
          },
        },],
      ],
      'targets': [
        {
          'target_name': 'UserHistoryTransition',
          'type': 'executable',
          'mac_bundle': 1,
          'sources': [
            'UserHistoryTransition/DialogsController.mm',
            'UserHistoryTransition/deprecated_user_storage.cc',
            'UserHistoryTransition/user_history_transition.cc',
            'UserHistoryTransition/user_history_transition_gui_main.mm',
            '../sync/sync_util.cc',  # for GenRandomString
            '../sync/user_history_sync_util.cc',
          ],
          'dependencies': [
            '../base/base.gyp:base',
            '../prediction/prediction.gyp:prediction',
            '../prediction/prediction.gyp:prediction_protocol',
          ],
          'mac_bundle_resources': [
            '../data/images/mac/product_icon.icns',
            'UserHistoryTransition/English.lproj/Dialogs.xib',
            'UserHistoryTransition/English.lproj/InfoPlist.strings',
            'UserHistoryTransition/Japanese.lproj/Dialogs.xib',
            'UserHistoryTransition/Japanese.lproj/InfoPlist.strings',
          ],
          'xcode_settings': {
            'INFOPLIST_FILE': 'UserHistoryTransition/Info.plist',
          },
        },
        {
          'target_name': 'UserHistoryTransitionCUI',
          'type': 'executable',
          'sources': [
            'UserHistoryTransition/deprecated_user_storage.cc',
            'UserHistoryTransition/user_history_transition.cc',
            'UserHistoryTransition/user_history_transition_main.cc',
            '../sync/sync_util.cc',  # for GenRandomString
            '../sync/user_history_sync_util.cc',
          ],
          'dependencies': [
            '../base/base.gyp:base',
            '../prediction/prediction.gyp:prediction',
            '../prediction/prediction.gyp:prediction_protocol',
          ],
        },
        {
          'target_name': 'ActivatePane',
          'type': 'loadable_module',
          'mac_bundle': 1,
          'sources': [
            'ActivatePane/ActivatePane.m',
          ],
          'mac_bundle_resources': [
            'ActivatePane/ActivatePane.xib',
            'ActivatePane/English.lproj/Localizable.strings',
            'ActivatePane/Japanese.lproj/Localizable.strings',
          ],
          'xcode_settings': {
            'INFOPLIST_FILE': 'ActivatePane/Info.plist',
            'GCC_C_LANGUAGE_STANDARD': 'c99',
          },
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/Carbon.framework',
              '$(SDKROOT)/System/Library/Frameworks/InstallerPlugins.framework',
            ],
          },
        },
        {
          'target_name': 'DevConfirmPane',
          'type': 'loadable_module',
          'mac_bundle': 1,
          'sources': [
            'DevConfirmPane/DevConfirmPane.m',
          ],
          'mac_bundle_resources': [
            'DevConfirmPane/DevConfirmPane.xib',
            'DevConfirmPane/English.lproj/Localizable.strings',
            'DevConfirmPane/Japanese.lproj/Localizable.strings',
          ],
          'xcode_settings': {
            'INFOPLIST_FILE': 'DevConfirmPane/Info.plist',
            'GCC_C_LANGUAGE_STANDARD': 'c99',
          },
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/Carbon.framework',
              '$(SDKROOT)/System/Library/Frameworks/InstallerPlugins.framework',
            ],
          },
        },
        {
          'target_name': 'UninstallGoogleJapaneseInput',
          'type': 'executable',
          'mac_bundle': 1,
          'sources': [
            'Uninstaller/DialogsController.mm',
            'Uninstaller/Uninstaller.mm',
            'Uninstaller/Uninstaller_main.mm',
          ],
          'dependencies': [
            '../base/base.gyp:base',
          ],
          'mac_bundle_resources': [
            '../data/images/mac/product_icon.icns',
            'Uninstaller/English.lproj/Dialogs.xib',
            'Uninstaller/English.lproj/InfoPlist.strings',
            'Uninstaller/Japanese.lproj/Dialogs.xib',
            'Uninstaller/Japanese.lproj/InfoPlist.strings',
          ],
          'xcode_settings': {
            'INFOPLIST_FILE': 'Uninstaller/Info.plist',
          },
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/Carbon.framework',
              '$(SDKROOT)/System/Library/Frameworks/Security.framework',
            ],
          },
        },
        {
          'target_name': 'GoogleJapaneseInput',
          'type': 'executable',
          'mac_bundle': 1,
          'sources': [
            'GoogleJapaneseInputController.mm',
            'GoogleJapaneseInputServer.mm',
            'KeyCodeMap.mm',
            'main.mm',
          ],
          'product_name': '<(branding)',
          'dependencies': [
            '../client/client.gyp:client',
            '../config/config.gyp:stats_config_util',
            '../gui/gui.gyp:mozc_tool',
            '../gui/gui.gyp:about_dialog_mac',
            '../gui/gui.gyp:character_palette_mac',
            '../gui/gui.gyp:config_dialog_mac',
            '../gui/gui.gyp:dictionary_tool_mac',
            '../gui/gui.gyp:error_message_dialog_mac',
            '../gui/gui.gyp:hand_writing_mac',
            '../gui/gui.gyp:prelauncher_mac',
            '../gui/gui.gyp:word_register_dialog_mac',
            '../renderer/renderer.gyp:mozc_renderer',
            '../renderer/renderer.gyp:renderer',
            '../renderer/renderer.gyp:table_layout',
            '../renderer/renderer.gyp:window_util',
            '../server/server.gyp:mozc_server',
            '../session/session_base.gyp:ime_switch_util',
            'gen_client_info_plist',
            'gen_key_mappings',
          ],
          'mac_bundle_resources': [
            '../data/images/mac/direct.png',
            '../data/images/mac/full_ascii.png',
            '../data/images/mac/full_katakana.png',
            '../data/images/mac/half_ascii.png',
            '../data/images/mac/half_katakana.png',
            '../data/images/mac/hiragana.png',
            '../data/images/mac/product_icon.icns',
            '../data/installer/credits_en.html',
            '../data/installer/credits_ja.html',
            'English.lproj/Config.xib',
            '<(gen_out_dir)/English.lproj/InfoPlist.strings',
            'Japanese.lproj/Config.xib',
            '<(gen_out_dir)/Japanese.lproj/InfoPlist.strings',
          ],
          'xcode_settings': {
            'INFOPLIST_FILE': '<(gen_out_dir)/Info.plist',
            'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
          },
          'copies': [
            {
              'files': [
                '<(PRODUCT_DIR)/<(branding)Converter.app',
                '<(PRODUCT_DIR)/<(branding)Prelauncher.app',
                '<(PRODUCT_DIR)/<(branding)Renderer.app',
                '<(PRODUCT_DIR)/<(branding)Tool.app',
                '<(PRODUCT_DIR)/AboutDialog.app',
                '<(PRODUCT_DIR)/CharacterPalette.app',
                '<(PRODUCT_DIR)/ConfigDialog.app',
                '<(PRODUCT_DIR)/DictionaryTool.app',
                '<(PRODUCT_DIR)/ErrorMessageDialog.app',
                '<(PRODUCT_DIR)/HandWriting.app',
                '<(PRODUCT_DIR)/WordRegisterDialog.app',
              ],
              'destination': '<(PRODUCT_DIR)/<(branding).app/Contents/Resources',
            },
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/Carbon.framework',
              '$(SDKROOT)/System/Library/Frameworks/InputMethodKit.framework',
            ],
          },
          'variables': {
            # This product name is used in postbuilds_mac.gypi.
            'product_name': '<(branding)',
          },
          'includes': [
            '../gyp/postbuilds_mac.gypi',
          ],
        },
        {
          'target_name': 'gen_client_info_plist',
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
                'python', '../build_tools/tweak_info_plist.py',
                '--output', '<(gen_out_dir)/Info.plist',
                '--input', 'Info.plist',
                '--version_file', '../mozc_version.txt',
                '--branding', '<(branding)',
              ],
            },
            {
              'action_name': 'generate_english_strings',
              'inputs': [
                'English.lproj/InfoPlist.strings',
              ],
              'outputs': [
                '<(gen_out_dir)/English.lproj/InfoPlist.strings',
              ],
              'action': [
                'python', '../build_tools/tweak_info_plist_strings.py',
                '--output', '<(gen_out_dir)/English.lproj/InfoPlist.strings',
                '--input', 'English.lproj/InfoPlist.strings',
                '--branding', '<(branding)',
              ],
            },
            {
              'action_name': 'generate_japanese_strings',
              'inputs': [
                'Japanese.lproj/InfoPlist.strings',
              ],
              'outputs': [
                '<(gen_out_dir)/Japanese.lproj/InfoPlist.strings',
              ],
              'action': [
                'python', '../build_tools/tweak_info_plist_strings.py',
                '--output', '<(gen_out_dir)/Japanese.lproj/InfoPlist.strings',
                '--input', 'Japanese.lproj/InfoPlist.strings',
                '--branding', '<(branding)',
              ],
            },
          ],
        },
        {
          'target_name': 'gen_launchd_confs',
          'type': 'none',
          'actions': [
            {
              'action_name': 'tweak_converter_launchd_conf',
              'inputs': [ '../data/mac/com.google.inputmethod.Japanese.Converter.plist', ],
              'outputs': [ '<(gen_out_dir)/<(domain_prefix).inputmethod.Japanese.Converter.plist' ],
              'action': [
                'python', '../build_tools/tweak_info_plist.py',
                '--output', '<(gen_out_dir)/<(domain_prefix).inputmethod.Japanese.Converter.plist',
                '--input', '../data/mac/com.google.inputmethod.Japanese.Converter.plist',
                '--version_file', '../mozc_version.txt',
                '--branding', '<(branding)',
              ],
            },
            {
              'action_name': 'tweak_renderer_launchd_conf',
              'inputs': [ '../data/mac/com.google.inputmethod.Japanese.Renderer.plist', ],
              'outputs': [ '<(gen_out_dir)/<(domain_prefix).inputmethod.Japanese.Renderer.plist' ],
              'action': [
                'python', '../build_tools/tweak_info_plist.py',
                '--output', '<(gen_out_dir)/<(domain_prefix).inputmethod.Japanese.Renderer.plist',
                '--input', '../data/mac/com.google.inputmethod.Japanese.Renderer.plist',
                '--version_file', '../mozc_version.txt',
                '--branding', '<(branding)',
              ],
            },
          ],
          'conditions': [
            ['branding=="GoogleJapaneseInput"', {
              'variables': {
                'domain_prefix': 'com.google',
              },
            }, # else
            {
              'variables': {
                'domain_prefix': 'org.mozc',
              },
            }],
          ],
        },
      ],
    }],
  ],
}
