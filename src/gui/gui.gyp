# Copyright 2010, Google Inc.
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
    'relative_dir': 'gui',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  'includes': [
    'qt_common.gypi',
  ],
  'targets': [
    {
      'target_name': 'gui_base',
      'type': 'static_library',
      'sources': [
        '<(proto_out_dir)/ipc/window_info.pb.cc',
        '<(gen_out_dir)/base/moc_window_title_modifier.cc',
        'base/debug_util.cc',
        'base/locale_util.cc',
        'base/singleton_window_helper.cc',
        'base/win_util.cc',
        'base/window_title_modifier.cc',
      ],
      'dependencies': [
        '../ipc/ipc.gyp:genproto_ipc',
        '../ipc/ipc.gyp:ipc',
        '../session/session.gyp:genproto_session',
        'gen_base_files',
      ],
      'includes': [
        'qt_libraries.gypi',
      ],
    },
    {
      'target_name': 'gen_base_files',
      'type': 'none',
      'variables': {
        'subdir': 'base',
      },
      'sources': [
        '<(subdir)/window_title_modifier.h',
      ],
      'includes': [
        'qt_moc.gypi',
      ],
    },
    {
      'target_name': 'gen_about_dialog_files',
      'type': 'none',
      'variables': {
        'subdir': 'about_dialog',
        'qrc_base_name': 'about_dialog',
      },
      'sources': [
        '<(subdir)/about_dialog.qrc',
        '<(subdir)/about_dialog.ui',
        '<(subdir)/about_dialog.h',
      ],
      'includes': [
        'qt_moc.gypi',
        'qt_rcc.gypi',
        'qt_uic.gypi',
      ],
    },
    {
      'target_name': 'about_dialog_lib',
      'type': 'static_library',
      'sources': [
        '<(gen_out_dir)/about_dialog/moc_about_dialog.cc',
        '<(gen_out_dir)/about_dialog/qrc_about_dialog.cc',
        'about_dialog/about_dialog.cc',
        'about_dialog/about_dialog_libmain.cc',
      ],
      'dependencies': [
        'gen_about_dialog_files',
        '../base/base.gyp:base',
      ],
      'includes': [
        'qt_libraries.gypi',
      ],
    },
    {
      'target_name': 'about_dialog_main',
      'type': 'executable',
      'sources': [
        'about_dialog/about_dialog_main.cc',
      ],
      'dependencies': [
        'gui_base',
        'about_dialog_lib',
      ],
      'includes': [
        'qt_libraries.gypi',
      ],
    },
    {
      'target_name': 'gen_administration_dialog_files',
      'type': 'none',
      'variables': {
        'subdir': 'administration_dialog',
        'qrc_base_name': 'administration_dialog',
      },
      'sources': [
        '<(subdir)/administration_dialog.h',
        '<(subdir)/administration_dialog.qrc',
        '<(subdir)/administration_dialog.ui',
      ],
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            '../server/server.gyp:cache_service_manager',
          ],
        }],
      ],
      'includes': [
        'qt_moc.gypi',
        'qt_rcc.gypi',
        'qt_uic.gypi',
      ],
    },
    {
      'target_name': 'administration_dialog_lib',
      'type': 'static_library',
      'sources': [
        '<(gen_out_dir)/administration_dialog/moc_administration_dialog.cc',
        '<(gen_out_dir)/administration_dialog/qrc_administration_dialog.cc',
        'administration_dialog/administration_dialog.cc',
        'administration_dialog/administration_dialog_libmain.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'gen_administration_dialog_files',
      ],
      'includes': [
        'qt_libraries.gypi',
      ],
    },
    {
      'target_name': 'administration_dialog_main',
      'type': 'executable',
      'sources': [
        'administration_dialog/administration_dialog_main.cc',
      ],
      'dependencies': [
        'gui_base',
        'administration_dialog_lib',
      ],
      'includes': [
        'qt_libraries.gypi',
      ],
    },
    {
      'target_name': 'gen_config_dialog_files',
      'type': 'none',
      'variables': {
        'subdir': 'config_dialog',
        'qrc_base_name': 'config_dialog',
      },
      'sources': [
        '<(subdir)/character_form_editor.h',
        '<(subdir)/combobox_delegate.h',
        '<(subdir)/config_dialog.h',
        '<(subdir)/config_dialog.qrc',
        '<(subdir)/config_dialog.ui',
        '<(subdir)/generic_table_editor.h',
        '<(subdir)/generic_table_editor.ui',
        '<(subdir)/keybinding_editor.h',
        '<(subdir)/keybinding_editor.ui',
        '<(subdir)/keybinding_editor_delegate.h',
        '<(subdir)/keymap_editor.h',
        '<(subdir)/roman_table_editor.h',
      ],
      'includes': [
        'qt_moc.gypi',
        'qt_rcc.gypi',
        'qt_uic.gypi',
      ],
    },
    {
      'target_name': 'config_dialog_lib',
      'type': 'static_library',
      'sources': [
        '<(gen_out_dir)/config_dialog/moc_character_form_editor.cc',
        '<(gen_out_dir)/config_dialog/moc_combobox_delegate.cc',
        '<(gen_out_dir)/config_dialog/moc_config_dialog.cc',
        '<(gen_out_dir)/config_dialog/moc_generic_table_editor.cc',
        '<(gen_out_dir)/config_dialog/moc_keybinding_editor.cc',
        '<(gen_out_dir)/config_dialog/moc_keybinding_editor_delegate.cc',
        '<(gen_out_dir)/config_dialog/moc_keymap_editor.cc',
        '<(gen_out_dir)/config_dialog/moc_roman_table_editor.cc',
        '<(gen_out_dir)/config_dialog/qrc_config_dialog.cc',
        'config_dialog/character_form_editor.cc',
        'config_dialog/combobox_delegate.cc',
        'config_dialog/config_dialog.cc',
        'config_dialog/config_dialog_libmain.cc',
        'config_dialog/generic_table_editor.cc',
        'config_dialog/keybinding_editor.cc',
        'config_dialog/keybinding_editor_delegate.cc',
        'config_dialog/keymap_editor.cc',
        'config_dialog/roman_table_editor.cc',
      ],
      'dependencies': [
        'gen_config_dialog_files',
        '../base/base.gyp:base',
        '../client/client.gyp:client',
        '../session/session.gyp:config_handler',
        '../session/session.gyp:genproto_session',
        '../session/session.gyp:keymap',
        '../session/session.gyp:key_parser',
      ],
      'includes': [
        'qt_libraries.gypi',
      ],
    },
    {
      'target_name': 'config_dialog_main',
      'type': 'executable',
      'sources': [
        'config_dialog/config_dialog_main.cc',
      ],
      'dependencies': [
        'gui_base',
        'config_dialog_lib',
      ],
      'includes': [
        'qt_libraries.gypi',
      ],
    },
    {
      'target_name': 'gen_confirmation_dialog_files',
      'type': 'none',
      'variables': {
        'subdir': 'confirmation_dialog',
        'qrc_base_name': 'confirmation_dialog',
      },
      'sources': [
        '<(subdir)/confirmation_dialog.qrc',
      ],
      'includes': [
        'qt_rcc.gypi',
      ],
    },
    {
      'target_name': 'confirmation_dialog_lib',
      'type': 'static_library',
      'sources': [
        '<(gen_out_dir)/confirmation_dialog/qrc_confirmation_dialog.cc',
        'confirmation_dialog/confirmation_dialog.cc',
        'confirmation_dialog/confirmation_dialog_libmain.cc',
      ],
      'dependencies': [
        'gen_confirmation_dialog_files',
        '../base/base.gyp:base',
      ],
      'includes': [
        'qt_libraries.gypi',
      ],
    },
    {
      'target_name': 'confirmation_dialog_main',
      'type': 'executable',
      'sources': [
        'confirmation_dialog/confirmation_dialog_main.cc',
      ],
      'dependencies': [
        'gui_base',
        'confirmation_dialog_lib',
      ],
      'includes': [
        'qt_libraries.gypi',
      ],
    },
    {
      'target_name': 'gen_dictionary_tool_files',
      'type': 'none',
      'variables': {
        'subdir': 'dictionary_tool',
        'qrc_base_name': 'dictionary_tool',
      },
      'sources': [
        '<(subdir)/dictionary_content_table_widget.h',
        '<(subdir)/dictionary_tool.h',
        '<(subdir)/dictionary_tool.qrc',
        '<(subdir)/dictionary_tool.ui',
        '<(subdir)/import_dialog.h',
        '<(subdir)/import_dialog.ui',
        '<(subdir)/zero_width_splitter.h',
      ],
      'includes': [
        'qt_moc.gypi',
        'qt_rcc.gypi',
        'qt_uic.gypi',
      ],
    },
    {
      'target_name': 'dictionary_tool_lib',
      'type': 'static_library',
      'sources': [
        '<(gen_out_dir)/config_dialog/moc_combobox_delegate.cc',
        '<(gen_out_dir)/dictionary_tool/moc_dictionary_content_table_widget.cc',
        '<(gen_out_dir)/dictionary_tool/moc_dictionary_tool.cc',
        '<(gen_out_dir)/dictionary_tool/qrc_dictionary_tool.cc',
        '<(gen_out_dir)/dictionary_tool/moc_import_dialog.cc',
        '<(gen_out_dir)/dictionary_tool/moc_zero_width_splitter.cc',
        'config_dialog/combobox_delegate.cc',
        'dictionary_tool/dictionary_tool.cc',
        'dictionary_tool/dictionary_content_table_widget.cc',
        'dictionary_tool/dictionary_tool_libmain.cc',
        'dictionary_tool/import_dialog.cc',
        'dictionary_tool/zero_width_splitter.cc',
      ],
      'dependencies': [
        'gen_config_dialog_files',
        'gen_dictionary_tool_files',
        '../base/base.gyp:base',
        '../client/client.gyp:client',
        '../dictionary/dictionary.gyp:user_dictionary',
        '../dictionary/dictionary.gyp:genproto_dictionary',
      ],
      'includes': [
        'qt_libraries.gypi',
      ],
    },
    {
      'target_name': 'dictionary_tool_main',
      'type': 'executable',
      'sources': [
        'dictionary_tool/dictionary_tool_main.cc',
      ],
      'dependencies': [
        'gui_base',
        'dictionary_tool_lib',
      ],
      'includes': [
        'qt_libraries.gypi',
      ],
    },
    {
      'target_name': 'gen_error_message_dialog_files',
      'type': 'none',
      'variables': {
        'subdir': 'error_message_dialog',
        'qrc_base_name': 'error_message_dialog',
      },
      'sources': [
        '<(subdir)/error_message_dialog.h',
        '<(subdir)/error_message_dialog.qrc',
      ],
      'includes': [
        'qt_moc.gypi',
        'qt_rcc.gypi',
      ],
    },
    {
      'target_name': 'error_message_dialog_lib',
      'type': 'static_library',
      'sources': [
        '<(gen_out_dir)/error_message_dialog/moc_error_message_dialog.cc',
        '<(gen_out_dir)/error_message_dialog/qrc_error_message_dialog.cc',
        'error_message_dialog/error_message_dialog.cc',
        'error_message_dialog/error_message_dialog_libmain.cc',
      ],
      'dependencies': [
        'gen_error_message_dialog_files',
        '../base/base.gyp:base',
      ],
      'includes': [
        'qt_libraries.gypi',
      ],
    },
    {
      'target_name': 'error_message_dialog_main',
      'type': 'executable',
      'sources': [
        'error_message_dialog/error_message_dialog_main.cc',
      ],
      'dependencies': [
        'gui_base',
        'error_message_dialog_lib',
      ],
      'includes': [
        'qt_libraries.gypi',
      ],
    },
    {
      'target_name': 'gen_post_install_dialog_files',
      'type': 'none',
      'variables': {
        'subdir': 'post_install_dialog',
        'qrc_base_name': 'post_install_dialog',
      },
      'sources': [
        '<(subdir)/post_install_dialog.h',
        '<(subdir)/post_install_dialog.qrc',
        '<(subdir)/post_install_dialog.ui',
      ],
      'includes': [
        'qt_moc.gypi',
        'qt_rcc.gypi',
        'qt_uic.gypi',
      ],
    },
    {
      'target_name': 'post_install_dialog_lib',
      'type': 'static_library',
      'sources': [
        '<(gen_out_dir)/post_install_dialog/moc_post_install_dialog.cc',
        '<(gen_out_dir)/post_install_dialog/qrc_post_install_dialog.cc',
        'post_install_dialog/post_install_dialog.cc',
        'post_install_dialog/post_install_dialog_libmain.cc',
      ],
      'dependencies': [
        'gen_post_install_dialog_files',
        '../dictionary/dictionary.gyp:user_dictionary',
        '../dictionary/dictionary.gyp:genproto_dictionary',
        '../ipc/ipc.gyp:ipc',
        '../session/session.gyp:genproto_session',
        '../usage_stats/usage_stats.gyp:genproto_usage_stats',
        '../usage_stats/usage_stats.gyp:usage_stats',
        '../base/base.gyp:base',
      ],
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            '../win32/win32.gyp:ime_base',
          ],
        }],
      ],
      'includes': [
        'qt_libraries.gypi',
      ],
    },
    {
      'target_name': 'post_install_dialog_main',
      'type': 'executable',
      'sources': [
        'post_install_dialog/post_install_dialog_main.cc',
      ],
      'dependencies': [
        'gui_base',
        'post_install_dialog_lib',
      ],
      'includes': [
        'qt_libraries.gypi',
      ],
    },
    {
      'target_name': 'gen_set_default_dialog_files',
      'type': 'none',
      'variables': {
        'subdir': 'set_default_dialog',
        'qrc_base_name': 'set_default_dialog',
      },
      'sources': [
        '<(subdir)/set_default_dialog.h',
        '<(subdir)/set_default_dialog.qrc',
        '<(subdir)/set_default_dialog.ui',
      ],
      'includes': [
        'qt_moc.gypi',
        'qt_rcc.gypi',
        'qt_uic.gypi',
      ],
    },
    {
      'target_name': 'set_default_dialog_lib',
      'type': 'static_library',
      'sources': [
        '<(gen_out_dir)/set_default_dialog/moc_set_default_dialog.cc',
        '<(gen_out_dir)/set_default_dialog/qrc_set_default_dialog.cc',
        'set_default_dialog/set_default_dialog.cc',
        'set_default_dialog/set_default_dialog_libmain.cc',
      ],
      'dependencies': [
        'gen_set_default_dialog_files',
        '../client/client.gyp:client',
        '../ipc/ipc.gyp:ipc',
        '../session/session.gyp:genproto_session',
      ],
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            '../win32/win32.gyp:ime_base',
          ],
        }],
      ],
      'includes': [
        'qt_libraries.gypi',
      ],
    },
    {
      'target_name': 'set_default_dialog_main',
      'type': 'executable',
      'sources': [
        'set_default_dialog/set_default_dialog_main.cc',
      ],
      'dependencies': [
        'gui_base',
        'set_default_dialog_lib',
      ],
      'includes': [
        'qt_libraries.gypi',
      ],
    },
    {
      'target_name': 'gen_mozc_tool_files',
      'type': 'none',
      'variables': {
        'subdir': 'tool',
        'qrc_base_name': 'mozc_tool',
      },
      'sources': [
        '<(subdir)/mozc_tool.qrc',
      ],
      'includes': [
        'qt_rcc.gypi',
      ],
    },
    {
      'target_name': 'mozc_tool',
      'type': 'executable',
      'conditions': [
        ['use_qt=="YES"', {
          'sources': [
            '<(gen_out_dir)/tool/qrc_mozc_tool.cc',
           'tool/mozc_tool_main.cc',
          ],
          'dependencies': [
            'about_dialog_lib',
            'administration_dialog_lib',
            'confirmation_dialog_lib',
            'config_dialog_lib',
            'dictionary_tool_lib',
            'error_message_dialog_lib',
            'gen_mozc_tool_files',
            'gui_base',
            'post_install_dialog_lib',
            'set_default_dialog_lib',
          ],
          'includes': [
            'qt_libraries.gypi',
          ],
        }, { # else
          # if you don't use Qt, you will use a mock main file for tool
          # and do not have dependencies to _lib.
          'sources': [
            'tool/mozc_tool_main_noqt.cc',
          ],
        },],
        ['OS=="mac"', {
          'product_name': '<(product_name)',
          'dependencies': [
            'gen_mozc_tool_info_plist',
          ],
          'mac_bundle': 1,
          'xcode_settings': {
            'INFOPLIST_FILE': '<(gen_out_dir)/hidden_mozc_tool_info',
          },
          'variables': {
            'product_name': '<(branding)Tool',
          },
          'includes': [
            '../gyp/postbuilds_mac.gypi',
          ],
        }],
        ['OS=="win"', {
          'product_name': 'GoogleIMEJaTool',
          'sources': [
            'tool/mozc_tool.exe.manifest',
            '<(gen_out_dir)/tool/mozc_tool_autogen.rc',
          ],
          'dependencies': [
            '../base/base.gyp:base',
            '../win32/win32.gyp:ime_base',
            'gen_mozc_tool_header',
          ],
          'includes': [
            '../gyp/postbuilds_win.gypi',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['use_qt=="YES"', {
      'includes': [
        'qt_target_default.gypi',
      ],
    },],
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'gen_mozc_tool_header',
          'variables': {
            'gen_resource_proj_name': 'mozc_tool',
            'gen_main_resource_path': 'gui/tool/mozc_tool.rc',
            'gen_output_resource_path':
                '<(gen_out_dir)/tool/mozc_tool_autogen.rc',
          },
        },
      ],
    }],
    ['OS=="mac"', {
      'targets': [
        {
          'target_name': 'gen_mozc_tool_info_plist',
          'type': 'none',
          'actions': [
            {
              'action_name': 'generate normal info plist',
              'inputs': [
                '../data/mac/mozc_tool_info',
              ],
              'outputs': [
                '<(gen_out_dir)/mozc_tool_info',
              ],
              'action': [
                'python', '../build_tools/tweak_info_plist.py',
                '--output', '<(gen_out_dir)/mozc_tool_info',
                '--input', '../data/mac/mozc_tool_info',
                '--version_file', '../mozc_version.txt',
                '--branding', '<(branding)',
              ],
            },
            {
              'action_name': 'generate hidden info plist',
              'inputs': [
                '../data/mac/hidden_mozc_tool_info',
              ],
              'outputs': [
                '<(gen_out_dir)/hidden_mozc_tool_info',
              ],
              'action': [
                'python', '../build_tools/tweak_info_plist.py',
                '--output', '<(gen_out_dir)/hidden_mozc_tool_info',
                '--input', '../data/mac/hidden_mozc_tool_info',
                '--version_file', '../mozc_version.txt',
                '--branding', '<(branding)',
              ],
            },
          ],
        },
        {
          'target_name': 'about_dialog_mac',
          'type': 'executable',
          'mac_bundle': 1,
          'variables': {
            'product_name': 'AboutDialog',
          },
          'xcode_settings': {
            'INFOPLIST_FILE': '<(gen_out_dir)/hidden_mozc_tool_info',
          },
          'includes': [
            'mac_gui.gypi',
          ],
        },
        {
          'target_name': 'config_dialog_mac',
          'type': 'executable',
          'mac_bundle': 1,
          'variables': {
            'product_name': 'ConfigDialog',
          },
          'xcode_settings': {
            'INFOPLIST_FILE': '<(gen_out_dir)/mozc_tool_info',
          },
          'mac_bundle_resources': [
            '../data/images/mac/product_icon.icns',
            '../data/mac/ConfigDialog/English.lproj/InfoPlist.strings',
            '../data/mac/ConfigDialog/Japanese.lproj/InfoPlist.strings',
          ],
          'includes': [
            'mac_gui.gypi',
          ],
        },
        {
          'target_name': 'dictionary_tool_mac',
          'type': 'executable',
          'mac_bundle': 1,
          'variables': {
            'product_name': 'DictionaryTool',
          },
          'xcode_settings': {
            'INFOPLIST_FILE': '<(gen_out_dir)/mozc_tool_info',
          },
          'mac_bundle_resources': [
            '../data/images/mac/product_icon.icns',
            '../data/mac/DictionaryTool/English.lproj/InfoPlist.strings',
            '../data/mac/DictionaryTool/Japanese.lproj/InfoPlist.strings',
          ],
          'includes': [
            'mac_gui.gypi',
          ],
        },
        {
          'target_name': 'error_message_dialog_mac',
          'type': 'executable',
          'mac_bundle': 1,
          'variables': {
            'product_name': 'ErrorMessageDialog',
          },
          'xcode_settings': {
            'INFOPLIST_FILE': '<(gen_out_dir)/hidden_mozc_tool_info',
          },
          'includes': [
            'mac_gui.gypi',
          ],
        },
      ],
    }],
  ],
}
