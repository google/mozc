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
    'relative_dir': 'gui',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
    'conditions': [
      ['branding=="GoogleJapaneseInput"', {
        'tool_product_name_win': 'GoogleIMEJaTool',
      }, {  # else
        'tool_product_name_win': 'mozc_tool',
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'gui_base',
      'type': 'static_library',
      'sources': [
        '<(gen_out_dir)/base/moc_window_title_modifier.cc',
        'base/debug_util.cc',
        'base/msime_user_dictionary_importer.cc',
        'base/setup_util.cc',
        'base/singleton_window_helper.cc',
        'base/table_util.cc',
        'base/util.cc',
        'base/win_util.cc',
        'base/window_title_modifier.cc',
      ],
      'dependencies': [
        '../base/absl.gyp:absl_strings',
        '../dictionary/dictionary_base.gyp:user_dictionary',
        '../ipc/ipc.gyp:ipc',
        '../ipc/ipc.gyp:window_info_protocol',
        '../protocol/protocol.gyp:commands_proto',
        '../protocol/protocol.gyp:user_dictionary_storage_proto',
        'encoding_util',
        'gen_base_files',
        'qrc_tr',
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
      'target_name': 'qrc_tr',
      'type': 'none',
      'variables': {
        'subdir': 'base',
        'qrc_base_name': 'tr',
        'qrc_inputs': [
          '<(subdir)/<(qrc_base_name).qrc',
          '<(subdir)/<(qrc_base_name)_en.qm',
          '<(subdir)/<(qrc_base_name)_ja.qm',
        ],
      },
      'includes': [
        'qt_rcc.gypi',
      ],
    },
    {
      'target_name': 'gen_about_dialog_files',
      'type': 'none',
      'variables': {
        'subdir': 'about_dialog',
      },
      'sources': [
        '<(subdir)/about_dialog.ui',
        '<(subdir)/about_dialog.h',
      ],
      'includes': [
        'qt_moc.gypi',
        'qt_uic.gypi',
      ],
    },
    {
      'target_name': 'qrc_about_dialog',
      'type': 'none',
      'variables': {
        'subdir': 'about_dialog',
        'qrc_base_name': 'about_dialog',
        'qrc_inputs': [
          '<(subdir)/<(qrc_base_name).qrc',
          '<(subdir)/<(qrc_base_name)_en.qm',
          '<(subdir)/<(qrc_base_name)_ja.qm',
          '../data/images/product_icon_32bpp-128.png',
        ],
      },
      'includes': [
        'qt_rcc.gypi',
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
        '../base/base.gyp:base',
        'gen_about_dialog_files',
        'qrc_about_dialog',
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
        'about_dialog_lib',
        'gui_base',
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
      },
      'sources': [
        '<(subdir)/administration_dialog.h',
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
        'qt_uic.gypi',
      ],
    },
    {
      'target_name': 'qrc_administration_dialog',
      'type': 'none',
      'variables': {
        'subdir': 'administration_dialog',
        'qrc_base_name': 'administration_dialog',
        'qrc_inputs': [
          '<(subdir)/<(qrc_base_name).qrc',
          '<(subdir)/<(qrc_base_name)_en.qm',
          '<(subdir)/<(qrc_base_name)_ja.qm',
        ],
      },
      'includes': [
        'qt_rcc.gypi',
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
        '../config/config.gyp:stats_config_util',
        'gen_administration_dialog_files',
        'qrc_administration_dialog',
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
        'administration_dialog_lib',
        'gui_base',
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
      },
      'sources': [
        '<(subdir)/character_form_editor.h',
        '<(subdir)/combobox_delegate.h',
        '<(subdir)/config_dialog.h',
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
        'qt_uic.gypi',
      ],
    },
    {
      'target_name': 'qrc_config_dialog',
      'type': 'none',
      'variables': {
        'subdir': 'config_dialog',
        'qrc_base_name': 'config_dialog',
        'qrc_inputs': [
          '<(subdir)/<(qrc_base_name).qrc',
          '<(subdir)/<(qrc_base_name)_en.qm',
          '<(subdir)/<(qrc_base_name)_ja.qm',
          '<(subdir)/keymap_en.qm',
          '<(subdir)/keymap_ja.qm',
        ],
      },
      'includes': [
        'qt_rcc.gypi',
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
        '../base/base.gyp:base',
        '../base/base.gyp:config_file_stream',
        '../client/client.gyp:client',
        '../composer/composer.gyp:key_parser',
        '../config/config.gyp:config_handler',
        '../config/config.gyp:stats_config_util',
        '../protocol/protocol.gyp:commands_proto',
        '../protocol/protocol.gyp:config_proto',
        '../session/session_base.gyp:keymap',
        'gen_config_dialog_files',
        'qrc_config_dialog',
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
        'config_dialog_lib',
        'gui_base',
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
      },
      'sources': [
        '<(subdir)/dictionary_content_table_widget.h',
        '<(subdir)/dictionary_tool.h',
        '<(subdir)/dictionary_tool.ui',
        '<(subdir)/find_dialog.h',
        '<(subdir)/find_dialog.ui',
        '<(subdir)/import_dialog.h',
        '<(subdir)/import_dialog.ui',
        '<(subdir)/zero_width_splitter.h',
      ],
      'includes': [
        'qt_moc.gypi',
        'qt_uic.gypi',
      ],
    },
    {
      'target_name': 'qrc_dictionary_tool',
      'type': 'none',
      'variables': {
        'subdir': 'dictionary_tool',
        'qrc_base_name': 'dictionary_tool',
        'qrc_inputs': [
          '<(subdir)/<(qrc_base_name).qrc',
          '<(subdir)/<(qrc_base_name)_en.qm',
          '<(subdir)/<(qrc_base_name)_ja.qm',
        ],
      },
      'includes': [
        'qt_rcc.gypi',
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
        '<(gen_out_dir)/dictionary_tool/moc_find_dialog.cc',
        '<(gen_out_dir)/dictionary_tool/moc_import_dialog.cc',
        '<(gen_out_dir)/dictionary_tool/moc_zero_width_splitter.cc',
        'config_dialog/combobox_delegate.cc',
        'dictionary_tool/dictionary_tool.cc',
        'dictionary_tool/dictionary_content_table_widget.cc',
        'dictionary_tool/dictionary_tool_libmain.cc',
        'dictionary_tool/find_dialog.cc',
        'dictionary_tool/import_dialog.cc',
        'dictionary_tool/zero_width_splitter.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../client/client.gyp:client',
        '../config/config.gyp:config_handler',
        '../data_manager/data_manager.gyp:pos_list_provider',
        '../dictionary/dictionary_base.gyp:user_dictionary',
        '../protocol/protocol.gyp:commands_proto',
        '../protocol/protocol.gyp:config_proto',
        '../protocol/protocol.gyp:user_dictionary_storage_proto',
        'encoding_util',
        'gen_config_dialog_files',
        'gen_dictionary_tool_files',
        'qrc_dictionary_tool',
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
        'dictionary_tool_lib',
        'gui_base',
      ],
      'includes': [
        'qt_libraries.gypi',
      ],
    },
    {
      'target_name': 'gen_word_register_dialog_files',
      'type': 'none',
      'variables': {
        'subdir': 'word_register_dialog',
      },
      'sources': [
        '<(subdir)/word_register_dialog.ui',
        '<(subdir)/word_register_dialog.h',
      ],
      'includes': [
        'qt_moc.gypi',
        'qt_uic.gypi',
      ],
    },
    {
      'target_name': 'qrc_word_register_dialog',
      'type': 'none',
      'variables': {
        'subdir': 'word_register_dialog',
        'qrc_base_name': 'word_register_dialog',
        'qrc_inputs': [
          '<(subdir)/<(qrc_base_name).qrc',
          '<(subdir)/<(qrc_base_name)_en.qm',
          '<(subdir)/<(qrc_base_name)_ja.qm',
        ],
      },
      'includes': [
        'qt_rcc.gypi',
      ],
    },
    {
      'target_name': 'word_register_dialog_lib',
      'type': 'static_library',
      'sources': [
        '<(gen_out_dir)/word_register_dialog/moc_word_register_dialog.cc',
        '<(gen_out_dir)/word_register_dialog/qrc_word_register_dialog.cc',
        'word_register_dialog/word_register_dialog.cc',
        'word_register_dialog/word_register_dialog_libmain.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../client/client.gyp:client',
        '../data_manager/data_manager.gyp:pos_list_provider',
        '../dictionary/dictionary_base.gyp:pos_matcher',
        '../dictionary/dictionary_base.gyp:user_dictionary',
        '../protocol/protocol.gyp:commands_proto',
        '../protocol/protocol.gyp:user_dictionary_storage_proto',
        'gen_word_register_dialog_files',
        'qrc_word_register_dialog',
      ],
      'includes': [
        'qt_libraries.gypi',
      ],
    },
    {
      'target_name': 'word_register_dialog_main',
      'type': 'executable',
      'sources': [
        'word_register_dialog/word_register_dialog_main.cc',
      ],
      'dependencies': [
        'gui_base',
        'word_register_dialog_lib',
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
      },
      'sources': [
        '<(subdir)/error_message_dialog.h',
      ],
      'includes': [
        'qt_moc.gypi',
      ],
    },
    {
      'target_name': 'qrc_error_message_dialog',
      'type': 'none',
      'variables': {
        'subdir': 'error_message_dialog',
        'qrc_base_name': 'error_message_dialog',
        'qrc_inputs': [
          '<(subdir)/<(qrc_base_name).qrc',
          '<(subdir)/<(qrc_base_name)_en.qm',
          '<(subdir)/<(qrc_base_name)_ja.qm',
        ],
      },
      'includes': [
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
        '../base/base.gyp:base',
        'gen_error_message_dialog_files',
        'qrc_error_message_dialog',
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
        'error_message_dialog_lib',
        'gui_base',
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
      },
      'sources': [
        '<(subdir)/post_install_dialog.h',
        '<(subdir)/post_install_dialog.ui',
      ],
      'includes': [
        'qt_moc.gypi',
        'qt_uic.gypi',
      ],
    },
    {
      'target_name': 'qrc_post_install_dialog',
      'type': 'none',
      'variables': {
        'subdir': 'post_install_dialog',
        'qrc_base_name': 'post_install_dialog',
        'qrc_inputs': [
          '<(subdir)/<(qrc_base_name).qrc',
          '<(subdir)/<(qrc_base_name)_en.qm',
          '<(subdir)/<(qrc_base_name)_ja.qm',
        ],
      },
      'includes': [
        'qt_rcc.gypi',
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
        '../base/base.gyp:base',
        '../ipc/ipc.gyp:ipc',
        '../protocol/protocol.gyp:commands_proto',
        '../usage_stats/usage_stats_base.gyp:usage_stats',
        'gen_post_install_dialog_files',
        'qrc_post_install_dialog',
      ],
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            '../win32/base/win32_base.gyp:ime_base',
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
      },
      'sources': [
        '<(subdir)/set_default_dialog.h',
        '<(subdir)/set_default_dialog.ui',
      ],
      'includes': [
        'qt_moc.gypi',
        'qt_uic.gypi',
      ],
    },
    {
      'target_name': 'qrc_set_default_dialog',
      'type': 'none',
      'variables': {
        'subdir': 'set_default_dialog',
        'qrc_base_name': 'set_default_dialog',
        'qrc_inputs': [
          '<(subdir)/<(qrc_base_name).qrc',
          '<(subdir)/<(qrc_base_name)_en.qm',
          '<(subdir)/<(qrc_base_name)_ja.qm',
        ],
      },
      'includes': [
        'qt_rcc.gypi',
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
        '../client/client.gyp:client',
        '../ipc/ipc.gyp:ipc',
        '../protocol/protocol.gyp:commands_proto',
        '../protocol/protocol.gyp:config_proto',
        'gen_set_default_dialog_files',
        'qrc_set_default_dialog',
      ],
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            '../win32/base/win32_base.gyp:ime_base',
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
      'target_name': 'prelauncher_lib',
      'type': 'static_library',
      'sources': [
        'tool/prelauncher_libmain.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../client/client.gyp:client',
        '../protocol/protocol.gyp:commands_proto',
        '../renderer/renderer.gyp:renderer_client',
      ],
    },
    {
      'target_name': 'mozc_tool_lib',
      'sources': [
        'tool/mozc_tool_libmain.cc',
      ],
      'dependencies': [
        '../base/base.gyp:crash_report_handler',
        '../config/config.gyp:stats_config_util',
        'about_dialog_lib',
        'administration_dialog_lib',
        'config_dialog_lib',
        'dictionary_tool_lib',
        'error_message_dialog_lib',
        'gui_base',
        'post_install_dialog_lib',
        'set_default_dialog_lib',
        'word_register_dialog_lib',
      ],
      'includes': [
        'qt_libraries.gypi',
      ],
      'conditions': [
        ['OS=="mac"', {
          'type': 'shared_library',
          'product_name': 'GuiTool_lib',
          'mac_bundle': 1,
          'xcode_settings': {
            # Ninja uses DYLIB_INSTALL_NAME_BASE to specify
            # where this framework is located.
            'DYLIB_INSTALL_NAME_BASE': '@executable_path/../Frameworks',
            'INSTALL_PATH': '@executable_path/../Frameworks',
            'INFOPLIST_FILE': '<(gen_out_dir)/mozc_tool_lib_info',
          },
          'dependencies': [
            'gen_mozc_tool_lib_info_plist',
            'prelauncher_lib',
            '../base/base.gyp:breakpad',
          ],
          'link_settings': {
            'libraries': [
              '<(mac_breakpad_framework)',
            ],
          },
          'conditions': [
            ['use_qt=="YES"', {
              'postbuilds': [
                {
                  'postbuild_name': 'Change the reference to frameworks.',
                  'action': [
                    '<(python)', '../build_tools/change_reference_mac.py',
                    '--qtdir', '<(qt_dir)',
                    '--target',
                    '${BUILT_PRODUCTS_DIR}/GuiTool_lib.framework/Versions/A/GuiTool_lib',
                  ],
                },
              ],
            }],
          ],
        }, {
          'type': 'static_library',
        }],
      ],
    },
    {
      'target_name': 'mozc_tool',
      'type': 'executable',
      'conditions': [
        ['use_qt=="YES"', {
          'sources': [
            'tool/mozc_tool_main.cc',
          ],
          'dependencies': [
            'mozc_tool_lib',
          ],
          'includes': [
            'qt_libraries.gypi',
          ],
        }, {
          # if you don't use Qt, you will use a mock main file for tool
          # and do not have dependencies to _lib.
          'sources': [
            'tool/mozc_tool_main_noqt.cc',
          ],
        }],
        # For Mac, ConfigDialog is the host app for necessary frameworks.
        ['OS=="win"', {
          'product_name': '<(tool_product_name_win)',
          'sources': [
            '<(gen_out_dir)/tool/mozc_tool_autogen.rc',
          ],
          'dependencies': [
            '../base/base.gyp:base',
            '../win32/base/win32_base.gyp:ime_base',
            'gen_mozc_tool_header',
          ],
          'msvs_settings': {
            'VCManifestTool': {
              'AdditionalManifestFiles': 'tool/mozc_tool.exe.manifest',
              'EmbedManifest': 'true',
            },
          },
        }],
      ],
    },
    {
      'target_name': 'encoding_util',
      'type': 'static_library',
      'sources': [
        'base/encoding_util.cc',
      ],
      'dependencies': [
        '../base/absl.gyp:absl_strings',
        '../base/base.gyp:base_core',
      ],
    },
    {
      'target_name': 'encoding_util_test',
      'type': 'executable',
      'sources': [
        'base/encoding_util_test.cc',
      ],
      'dependencies': [
        '../base/absl.gyp:absl_strings',
        '../testing/testing.gyp:gtest_main',
        'encoding_util',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'gui_all_test',
      'type': 'none',
      'dependencies': [
        'encoding_util_test',
      ],
    },
  ],
  'conditions': [
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
          'includes': [
            '../gyp/gen_win32_resource_header.gypi',
          ],
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
                '<(python)', '../build_tools/tweak_info_plist.py',
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
                '<(python)', '../build_tools/tweak_info_plist.py',
                '--output', '<(gen_out_dir)/hidden_mozc_tool_info',
                '--input', '../data/mac/hidden_mozc_tool_info',
                '--version_file', '../mozc_version.txt',
                '--branding', '<(branding)',
              ],
            },
          ],
        },
        {
          'target_name': 'gen_mozc_tool_info_strings',
          'type': 'none',
          'actions': [
            {
              'action_name': 'generate_config_dialog_english_strings',
              'inputs': [
                '../data/mac/ConfigDialog/English.lproj/InfoPlist.strings',
              ],
              'outputs': [
                '<(gen_out_dir)/ConfigDialog/English.lproj/InfoPlist.strings',
              ],
              'action': [
                '<(python)', '../build_tools/tweak_info_plist_strings.py',
                '--output',
                '<(gen_out_dir)/ConfigDialog/English.lproj/InfoPlist.strings',
                '--input',
                '../data/mac/ConfigDialog/English.lproj/InfoPlist.strings',
                '--branding', '<(branding)',
              ],
            },
            {
              'action_name': 'generate_config_dialog_japanese_strings',
              'inputs': [
                '../data/mac/ConfigDialog/Japanese.lproj/InfoPlist.strings',
              ],
              'outputs': [
                '<(gen_out_dir)/ConfigDialog/Japanese.lproj/InfoPlist.strings',
              ],
              'action': [
                '<(python)', '../build_tools/tweak_info_plist_strings.py',
                '--output',
                '<(gen_out_dir)/ConfigDialog/Japanese.lproj/InfoPlist.strings',
                '--input',
                '../data/mac/ConfigDialog/Japanese.lproj/InfoPlist.strings',
                '--branding', '<(branding)',
              ],
            },
          ],
        },
        {
          'target_name': 'gen_mozc_tool_lib_info_plist',
          'type': 'none',
          'actions': [
            {
              'action_name': 'mozc_tool_lib info.plist',
              'inputs': [
                '../data/mac/mozc_tool_lib_info',
              ],
              'outputs': [
                '<(gen_out_dir)/mozc_tool_lib_info',
              ],
              'action': [
                '<(python)', '../build_tools/tweak_info_plist.py',
                '--output', '<(gen_out_dir)/mozc_tool_lib_info',
                '--input', '../data/mac/mozc_tool_lib_info',
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
          # ConfigDialog.app is the host app of Frameworks (e.g. GuiTool_lib,
          # QtCore, Breakpad, etc.). These Frameworks are refferred by other
          # apps like AboutDialog.app.
          'target_name': 'config_dialog_mac',
          'product_name': 'ConfigDialog',
          'type': 'executable',
          'mac_bundle': 1,
          'variables': {
            'product_name': 'ConfigDialog',
            'copying_frameworks': [
              '<(PRODUCT_DIR)/GuiTool_lib.framework',
            ],
          },
          'dependencies': [
            'gen_mozc_tool_info_plist',
            'gen_mozc_tool_info_strings',
          ],
          'xcode_settings': {
            'INFOPLIST_FILE': '<(gen_out_dir)/mozc_tool_info',
          },
          'mac_bundle_resources': [
            '../data/images/mac/product_icon.icns',
            '<(gen_out_dir)/ConfigDialog/English.lproj/InfoPlist.strings',
            '<(gen_out_dir)/ConfigDialog/Japanese.lproj/InfoPlist.strings',
          ],
          'conditions': [
            ['use_qt=="YES"', {
              'mac_bundle_resources': ['../data/mac/qt.conf'],
              'sources': [
                'tool/mozc_tool_main.cc',
              ],
              'dependencies': [
                'mozc_tool_lib',
              ],
              'includes': [
                '../gyp/postbuilds_mac.gypi',
              ],
              'postbuilds': [
                {
                  'postbuild_name': 'Change the reference to frameworks',
                  'action': [
                    '<(python)', '../build_tools/change_reference_mac.py',
                    '--qtdir', '<(qt_dir)',
                    '--target',
                    '${BUILT_PRODUCTS_DIR}/<(product_name).app/Contents/MacOS/<(product_name)',
                  ],
                },
                {
                  'postbuild_name': 'Copy Qt frameworks to the frameworks directory.',
                  'action': [
                    '<(python)', '../build_tools/copy_qt_frameworks_mac.py',
                    '--qtdir', '<(qt_dir)',
                    '--target', '${BUILT_PRODUCTS_DIR}/<(product_name).app/Contents/Frameworks/',
                  ],
                },
              ],
            }, {  # else
              'sources': [
                'tool/mozc_tool_main_noqt.cc',
              ],
            }],
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
        {
          'target_name': 'word_register_dialog_mac',
          'type': 'executable',
          'mac_bundle': 1,
          'variables': {
            'product_name': 'WordRegisterDialog',
          },
          'xcode_settings': {
            'INFOPLIST_FILE': '<(gen_out_dir)/hidden_mozc_tool_info',
          },
          'includes': [
            'mac_gui.gypi',
          ],
        },
        {
          'target_name': 'prelauncher_mac',
          'type': 'executable',
          'mac_bundle': 1,
          'variables': {
            'product_name': '<(branding)Prelauncher',
          },
          'xcode_settings': {
            'INFOPLIST_FILE': '<(gen_out_dir)/mozc_tool_info',
          },
          'mac_bundle_resources': [
            '../data/images/mac/product_icon.icns',
          ],
          'includes': [
            'mac_gui.gypi',
          ],
        },
      ],
    }],
  ],
}
