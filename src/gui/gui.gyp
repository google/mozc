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
  'includes': [
    'qt_target_defaults.gypi',
  ],
  'targets': [
    {
      'target_name': 'gui_base',
      'type': 'static_library',
      'sources': [
        '<(gen_out_dir)/base/moc_window_title_modifier.cc',
        'base/debug_util.cc',
        'base/locale_util.cc',
        'base/setup_util.cc',
        'base/singleton_window_helper.cc',
        'base/win_util.cc',
        'base/window_title_modifier.cc',
      ],
      'dependencies': [
        '../dictionary/dictionary.gyp:user_dictionary',
        '../ipc/ipc.gyp:ipc',
        '../ipc/ipc.gyp:window_info_protocol',
        '../session/session_base.gyp:session_protocol',
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
        '../base/base.gyp:base',
        'gen_about_dialog_files',
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
        '../config/config.gyp:stats_config_util',
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
        'administration_dialog_lib',
        'gui_base',
      ],
      'includes': [
        'qt_libraries.gypi',
      ],
    },
    {
      'target_name': 'gen_character_pad_files',
      'type': 'none',
      'variables': {
        'subdir': 'character_pad',
        'qrc_base_name': 'character_pad',
      },
      'sources': [
        '<(subdir)/character_pad.qrc',
        '<(subdir)/character_palette.h',
        '<(subdir)/character_palette_table_widget.h',
        '<(subdir)/character_palette.ui',
        '<(subdir)/hand_writing.h',
        '<(subdir)/hand_writing.ui',
        '<(subdir)/hand_writing_canvas.h',
        '<(subdir)/hand_writing_thread.h',
        '<(subdir)/result_list.h',
      ],
      'includes': [
        'qt_moc.gypi',
        'qt_rcc.gypi',
        'qt_uic.gypi',
      ],
    },
    {
      'target_name': 'gen_character_pad_cp932_data',
      'type': 'none',
      'actions': [
        {
          'action_name': 'gen_cp932_map_data',
          'variables': {
            'input_files': [
              '../data/unicode/CP932.TXT',
            ],
          },
          'inputs': [
            'character_pad/data/gen_cp932_map.py',
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/character_pad/data/cp932_map.h',
          ],
          'action': [
            'python', '../build_tools/redirect.py',
            '<(gen_out_dir)/character_pad/data/cp932_map.h',
            'character_pad/data/gen_cp932_map.py',
            '<@(input_files)',
          ],
          'message': 'Generating <(gen_out_dir)/character_pad/data/cp932_map.h.',
        },
      ],
    },
    {
      'target_name': 'gen_character_pad_data',
      'type': 'none',
      'actions': [
        {
          'action_name': 'gen_unihan_data',
          'variables': {
            'input_files': [
              '../data/unicode/RadicalIndex.txt',
              '../data/unicode/Unihan.txt',
            ],
          },
          'inputs': [
            'character_pad/data/gen_unihan_data.py',
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/character_pad/data/unihan_data.h',
          ],
          'action': [
            'python', '../build_tools/redirect.py',
            '<(gen_out_dir)/character_pad/data/unihan_data.h',
            'character_pad/data/gen_unihan_data.py',
            '<@(input_files)',
          ],
          'message': 'Generating <(gen_out_dir)/character_pad/data/unihan_data.h.',
        },
        {
          'action_name': 'gen_unicode_blocks',
          'variables': {
            'input_files': [
              '../data/unicode/Blocks.txt',
            ],
          },
          'inputs': [
            'character_pad/data/gen_unicode_blocks.py',
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/character_pad/data/unicode_blocks.h',
          ],
          'action': [
            'python', '../build_tools/redirect.py',
            '<(gen_out_dir)/character_pad/data/unicode_blocks.h',
            'character_pad/data/gen_unicode_blocks.py',
            '<@(input_files)',
          ],
          'message': 'Generating <(gen_out_dir)/character_pad/data/unicode_blocks.h.',
        },
        {
          'action_name': 'gen_unicode_data',
          'variables': {
            'input_files': [
              '../data/unicode/UnicodeData.txt',
            ],
          },
          'inputs': [
            'character_pad/data/gen_unicode_data.py',
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/character_pad/data/unicode_data.h',
          ],
          'action': [
            'python', '../build_tools/redirect.py',
            '<(gen_out_dir)/character_pad/data/unicode_data.h',
            'character_pad/data/gen_unicode_data.py',
            '<@(input_files)',
          ],
          'message': 'Generating <(gen_out_dir)/character_pad/data/unicode_data.h.',
        },
        {
          'action_name': 'gen_local_character_map',
          'variables': {
            'input_files': [
              '../data/unicode/JIS0201.TXT',
              '../data/unicode/JIS0208.TXT',
              '../data/unicode/JIS0212.TXT',
              '../data/unicode/CP932.TXT',
            ],
          },
          'inputs': [
            'character_pad/data/gen_local_character_map.py',
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/character_pad/data/local_character_map.h',
          ],
          'action': [
            'python', '../build_tools/redirect.py',
            '<(gen_out_dir)/character_pad/data/local_character_map.h',
            'character_pad/data/gen_local_character_map.py',
            '<@(input_files)',
          ],
          'message': 'Generating <(gen_out_dir)/character_pad/data/local_character_map.h.',
        },
      ],
    },
    {
      'target_name': 'character_pad_lib',
      'type': 'static_library',
      'sources': [
        '<(gen_out_dir)/character_pad/moc_character_palette.cc',
        '<(gen_out_dir)/character_pad/moc_hand_writing.cc',
        '<(gen_out_dir)/character_pad/moc_hand_writing_canvas.cc',
        '<(gen_out_dir)/character_pad/moc_hand_writing_thread.cc',
        '<(gen_out_dir)/character_pad/moc_character_palette_table_widget.cc',
        '<(gen_out_dir)/character_pad/moc_result_list.cc',
        '<(gen_out_dir)/character_pad/qrc_character_pad.cc',
        '<(gen_out_dir)/character_pad/data/cp932_map.h',
        '<(gen_out_dir)/character_pad/data/unihan_data.h',
        '<(gen_out_dir)/character_pad/data/unicode_blocks.h',
        '<(gen_out_dir)/character_pad/data/unicode_data.h',
        '<(gen_out_dir)/character_pad/data/local_character_map.h',
        '<(gen_out_dir)/dictionary_tool/moc_zero_width_splitter.cc',
        'character_pad/character_pad_libmain.cc',
        'character_pad/character_palette_table_widget.cc',
        'character_pad/character_palette.cc',
        'character_pad/hand_writing_canvas.cc',
        'character_pad/hand_writing_thread.cc',
        'character_pad/hand_writing.cc',
        'character_pad/result_list.cc',
        'character_pad/selection_handler.cc',
        'character_pad/unicode_util.cc',
        'character_pad/windows_selection_handler.cc',
        'dictionary_tool/zero_width_splitter.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../client/client.gyp:client',
        '../config/config.gyp:stats_config_util',
        '../handwriting/handwriting.gyp:handwriting_manager',
        '../handwriting/handwriting.gyp:zinnia_handwriting',
        '../session/session_base.gyp:session_protocol',
        'gen_character_pad_files',
        'gen_character_pad_cp932_data',
        'gen_character_pad_data',
        'gen_dictionary_tool_files',
      ],
      'includes': [
        'qt_libraries.gypi',
      ],
      'conditions': [
        ['enable_cloud_handwriting==1', {
          'dependencies': [
            '../handwriting/handwriting.gyp:cloud_handwriting',
            '../session/session_base.gyp:session_protocol',
          ],
        }],
        ['use_libzinnia==1 and OS=="linux"', {
          'defines': [
            'USE_LIBZINNIA',
          ],
        }],
      ],
    },
    {
      'target_name': 'character_palette_main',
      'type': 'executable',
      'sources': [
        'character_pad/character_palette_main.cc',
      ],
      'dependencies': [
        'gui_base',
        'character_pad_lib',
      ],
      'includes': [
        'qt_libraries.gypi',
      ],
    },
    {
      'target_name': 'hand_writing_main',
      'type': 'executable',
      'sources': [
        'character_pad/hand_writing_main.cc',
      ],
      'dependencies': [
        'gui_base',
        'character_pad_lib',
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
      'conditions': [
        ['enable_webservice_infolist==1', {
          'sources': [
            '<(subdir)/webservice_infolist_editor.h',
          ],
        }],
        ['enable_cloud_sync==1', {
          'sources': [
            '<(subdir)/auth_code_detector.h',
            '<(subdir)/auth_dialog.h',
            '<(subdir)/auth_dialog.ui',
            '<(subdir)/sync_customize_dialog.h',
            '<(subdir)/sync_customize_dialog.ui',
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
        '../config/config.gyp:config_handler',
        '../config/config.gyp:config_protocol',
        '../config/config.gyp:stats_config_util',
        '../session/session_base.gyp:key_parser',
        '../session/session_base.gyp:keymap',
        '../session/session_base.gyp:session_protocol',
        '../sync/sync.gyp:oauth2_token_util',
        'gen_config_dialog_files',
      ],
      'conditions': [
        ['enable_webservice_infolist==1', {
          'sources': [
            '<(gen_out_dir)/config_dialog/moc_webservice_infolist_editor.cc',
            'config_dialog/webservice_infolist_editor.cc',
          ],
        }],
        ['enable_cloud_sync==1', {
          'sources': [
            '<(gen_out_dir)/config_dialog/moc_auth_code_detector.cc',
            '<(gen_out_dir)/config_dialog/moc_auth_dialog.cc',
            '<(gen_out_dir)/config_dialog/moc_sync_customize_dialog.cc',
            'config_dialog/auth_code_detector.cc',
            'config_dialog/auth_dialog.cc',
            'config_dialog/sync_customize_dialog.cc',
          ],
          'dependencies': [
            '../sync/sync.gyp:oauth2',
          ],
        }],
        ['enable_cloud_handwriting==1 or enable_webservice_infolist==1', {
          'dependencies': [
            '<(DEPTH)/third_party/jsoncpp/jsoncpp.gyp:jsoncpp',
          ],
        }],
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
        '../base/base.gyp:base',
        'gen_confirmation_dialog_files',
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
        'confirmation_dialog_lib',
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
        'qrc_base_name': 'dictionary_tool',
      },
      'sources': [
        '<(subdir)/dictionary_content_table_widget.h',
        '<(subdir)/dictionary_tool.h',
        '<(subdir)/dictionary_tool.qrc',
        '<(subdir)/dictionary_tool.ui',
        '<(subdir)/find_dialog.h',
        '<(subdir)/find_dialog.ui',
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
        '../config/config.gyp:config_protocol',
        '../data_manager/data_manager.gyp:user_dictionary_manager',
        '../dictionary/dictionary.gyp:dictionary_protocol',
        '../dictionary/dictionary.gyp:user_dictionary',
        '../session/session_base.gyp:session_protocol',
        'gen_config_dialog_files',
        'gen_dictionary_tool_files',
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
        'qrc_base_name': 'word_register_dialog',
      },
      'sources': [
        '<(subdir)/word_register_dialog.ui',
        '<(subdir)/word_register_dialog.h',
        '<(subdir)/word_register_dialog.qrc',
      ],
      'includes': [
        'qt_moc.gypi',
        'qt_rcc.gypi',
        'qt_uic.gypi',
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
        '../data_manager/data_manager.gyp:user_dictionary_manager',
        '../dictionary/dictionary.gyp:dictionary_protocol',
        '../dictionary/dictionary.gyp:user_dictionary',
        '../session/session_base.gyp:session_protocol',
        'gen_word_register_dialog_files',
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
        '../base/base.gyp:base',
        'gen_error_message_dialog_files',
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
        '../base/base.gyp:base',
        '../ipc/ipc.gyp:ipc',
        '../session/session_base.gyp:session_protocol',
        '../usage_stats/usage_stats.gyp:usage_stats',
        'gen_post_install_dialog_files',
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
        '../client/client.gyp:client',
        '../config/config.gyp:config_protocol',
        '../ipc/ipc.gyp:ipc',
        '../session/session_base.gyp:session_protocol',
        'gen_set_default_dialog_files',
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
      'target_name': 'gen_update_dialog_files',
      'type': 'none',
      'variables': {
        'subdir': 'update_dialog',
        'qrc_base_name': 'update_dialog',
      },
      'sources': [
        '<(subdir)/update_dialog.ui',
        '<(subdir)/update_dialog.h',
        '<(subdir)/update_dialog.qrc',
      ],
      'includes': [
        'qt_moc.gypi',
        'qt_rcc.gypi',
        'qt_uic.gypi',
      ],
    },
    {
      'target_name': 'update_dialog_lib',
      'type': 'static_library',
      'sources': [
        '<(gen_out_dir)/update_dialog/moc_update_dialog.cc',
        '<(gen_out_dir)/update_dialog/qrc_update_dialog.cc',
        'update_dialog/update_dialog.cc',
        'update_dialog/update_dialog_libmain.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'gen_update_dialog_files',
      ],
      'includes': [
        'qt_libraries.gypi',
      ],
    },
    {
      'target_name': 'update_dialog_main',
      'type': 'executable',
      'sources': [
        'update_dialog/update_dialog_main.cc',
      ],
      'dependencies': [
        'gui_base',
        'update_dialog_lib',
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
      'target_name': 'prelauncher_lib',
      'type': 'static_library',
      'sources': [
        'tool/prelauncher_libmain.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../client/client.gyp:client',
        '../renderer/renderer.gyp:renderer',
        '../session/session_base.gyp:session_protocol',
      ],
    },
    {
      'target_name': 'mozc_tool_lib',
      'sources': [
        '<(gen_out_dir)/tool/qrc_mozc_tool.cc',
        'tool/mozc_tool_libmain.cc',
      ],
      'dependencies': [
        'about_dialog_lib',
        'administration_dialog_lib',
        'config_dialog_lib',
        'confirmation_dialog_lib',
        'dictionary_tool_lib',
        'error_message_dialog_lib',
        'gen_mozc_tool_files',
        'gui_base',
        'post_install_dialog_lib',
        'set_default_dialog_lib',
        'update_dialog_lib',
        'word_register_dialog_lib',
        '../config/config.gyp:stats_config_util',
      ],
      'includes': [
        'qt_libraries.gypi',
      ],
      'conditions': [
        ['OS=="mac"', {
          'type': 'shared_library',
          'product_name': '<(branding)Tool_lib',
          'mac_bundle': 1,
          'xcode_settings': {
            'INSTALL_PATH': '@executable_path/../Frameworks',
          },
          'dependencies+': [
            'prelauncher_lib',
          ],
          'conditions': [
            ['use_qt=="YES" and branding=="GoogleJapaneseInput"', {
              'postbuilds': [
                {
                  'postbuild_name': 'make Resources directory',
                  'action': [
                    'mkdir', '-p',
                    '${BUILT_PRODUCTS_DIR}/<(branding)Tool_lib.framework/Contents/Resources',
                  ],
                },
                {
                  'postbuild_name': 'copy qt_menu.nib to Resources directory',
                  'action': [
                    'cp', '-rf',
                     '<(qt_dir)/src/gui/mac/qt_menu.nib',
                     '${BUILT_PRODUCTS_DIR}/<(branding)Tool_lib.framework/Contents/Resources/qt_menu.nib',
                  ],
                },
              ],
            }],
          ],
        }, {
          'type': 'static_library',
        }],
        ['use_zinnia=="YES"', {
          'dependencies+': [
            'character_pad_lib',
          ],
          'defines': [
            'USE_ZINNIA',
          ],
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
        ['OS=="mac"', {
          'product_name': '<(product_name)',
          'variables': {
            'product_name': '<(branding)Tool',
          },
          'dependencies': [
            'gen_mozc_tool_info_plist',
          ],
          'conditions': [
            ['use_qt=="YES"', {
              'variables': {
                'copying_frameworks': [
                  '<(PRODUCT_DIR)/<(branding)Tool_lib.framework',
                ],
              },
              # We include this gypi file here because the variables
              # in a condition cannot be refferred from the gypi file
              # included outside from the condition.
              'includes': [
                '../gyp/postbuilds_mac.gypi',
              ],
            }, {
              # So we include the same file explicitly here.
              'includes': [
                '../gyp/postbuilds_mac.gypi',
              ],
            }]
          ],
          'mac_bundle': 1,
          'xcode_settings': {
            'INFOPLIST_FILE': '<(gen_out_dir)/hidden_mozc_tool_info',
          },
        }],
        ['OS=="win"', {
          'product_name': '<(tool_product_name_win)',
          'sources': [
            'tool/mozc_tool.exe.manifest',
            '<(gen_out_dir)/tool/mozc_tool_autogen.rc',
          ],
          'dependencies': [
            '../base/base.gyp:base',
            '../win32/base/win32_base.gyp:ime_base',
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
          'target_name': 'hand_writing_mac',
          'type': 'executable',
          'mac_bundle': 1,
          'variables': {
            'product_name': 'HandWriting',
          },
          'xcode_settings': {
            'INFOPLIST_FILE': '<(gen_out_dir)/mozc_tool_info',
          },
          'mac_bundle_resources': [
            '../data/images/mac/product_icon.icns',
            '../data/mac/HandWriting/English.lproj/InfoPlist.strings',
            '../data/mac/HandWriting/Japanese.lproj/InfoPlist.strings',
          ],
          'includes': [
            'mac_gui.gypi',
          ],
        },
        {
          'target_name': 'character_palette_mac',
          'type': 'executable',
          'mac_bundle': 1,
          'variables': {
            'product_name': 'CharacterPalette',
          },
          'xcode_settings': {
            'INFOPLIST_FILE': '<(gen_out_dir)/mozc_tool_info',
          },
          'mac_bundle_resources': [
            '../data/images/mac/product_icon.icns',
            '../data/mac/CharacterPalette/English.lproj/InfoPlist.strings',
            '../data/mac/CharacterPalette/Japanese.lproj/InfoPlist.strings',
          ],
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
