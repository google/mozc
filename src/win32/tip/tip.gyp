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
    'relative_dir': 'win32/tip',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
    'conditions': [
      ['branding=="GoogleJapaneseInput"', {
        'tipfile_product_name_win': 'GoogleIMEJaTIP',
      }, {  # else
        'tipfile_product_name_win': 'mozc_tip',
      }],
    ],
  },
  'targets': [
    # Test cases meta target: this target is referred from gyp/tests.gyp
    {
      'target_name': 'tip_all_test',
      'type': 'none',
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            'tip_core_test',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'tip_core',
          'type': 'static_library',
          'sources': [
            'mozc_tip_main.cc',
            'tip_candidate_list.cc',
            'tip_candidate_string.cc',
            'tip_class_factory.cc',
            'tip_compartment_util.cc',
            'tip_composition_util.cc',
            'tip_display_attributes.cc',
            'tip_dll_module.cc',
            'tip_edit_session.cc',
            'tip_edit_session_impl.cc',
            'tip_enum_candidates.cc',
            'tip_enum_display_attributes.cc',
            'tip_input_mode_manager.cc',
            'tip_keyevent_handler.cc',
            'tip_lang_bar.cc',
            'tip_lang_bar_menu.cc',
            'tip_linguistic_alternates.cc',
            'tip_preferred_touch_keyboard.cc',
            'tip_private_context.cc',
            'tip_query_provider.cc',
            'tip_search_candidate_provider.cc',
            'tip_range_util.cc',
            'tip_reconvert_function.cc',
            'tip_status.cc',
            'tip_surrounding_text.cc',
            'tip_text_service.cc',
            'tip_thread_context.cc',
            'tip_transitory_extension.cc',
            'tip_ui_element_conventional.cc',
            'tip_ui_element_delegate.cc',
            'tip_ui_element_manager.cc',
            'tip_ui_handler.cc',
            'tip_ui_handler_conventional.cc',
          ],
          'dependencies': [
            '<(mozc_oss_src_dir)/base/absl.gyp:absl_base',
            '<(mozc_oss_src_dir)/base/base.gyp:base',
            '<(mozc_oss_src_dir)/base/base.gyp:update_util',
            '<(mozc_oss_src_dir)/base/win32/base_win32.gyp:com_implements',
            '<(mozc_oss_src_dir)/client/client.gyp:client',
            '<(mozc_oss_src_dir)/config/config.gyp:config_handler',
            '<(mozc_oss_src_dir)/protobuf/protobuf.gyp:protobuf',
            '<(mozc_oss_src_dir)/protocol/protocol.gyp:commands_proto',
            '<(mozc_oss_src_dir)/protocol/protocol.gyp:renderer_proto',
            '<(mozc_oss_src_dir)/renderer/renderer.gyp:win32_renderer_client',
            '../base/win32_base.gyp:ime_base',
            '../base/win32_base.gyp:ime_impl_base',
            '../base/win32_base.gyp:imframework_util',
            '../base/win32_base.gyp:msctf_dll_import_lib',
            '../base/win32_base.gyp:text_icon',
          ],
        },
        {
          'target_name': 'tip_core_test',
          'type': 'executable',
          'sources': [
            'tip_candidate_list_test.cc',
            'tip_candidate_string_test.cc',
            'tip_enum_candidates_test.cc',
            'tip_display_attributes_test.cc',
            'tip_enum_display_attributes_test.cc',
            'tip_input_mode_manager_test.cc',
            'tip_surrounding_text_test.cc',
          ],
          'dependencies': [
            '<(mozc_oss_src_dir)/testing/testing.gyp:gtest_main',
            'tip_core',
          ],
        },
        {
          'target_name': 'gen_mozc_tip_resource_header',
          'variables': {
            'gen_resource_proj_name': 'mozc_tip',
            'gen_main_resource_path': 'win32/tip/tip_resource.rc',
            'gen_output_resource_path':
                '<(gen_out_dir)/tip_resource_autogen.rc',
          },
          'includes': [
            '../../gyp/gen_win32_resource_header.gypi',
          ],
        },
        {
          'target_name': 'mozc_tip32',
          'product_name': '<(tipfile_product_name_win)32',
          'product_extension': 'dll',
          'type': 'shared_library',
          'sources': [
            '<(gen_out_dir)/tip_resource_autogen.rc',
            'mozc_tip.def',
          ],
          'dependencies': [
            'gen_mozc_tip_resource_header',
            'tip_core',
          ],
        },
        {
          'target_name': 'mozc_tip64',
          'product_name': '<(tipfile_product_name_win)64',
          'product_extension': 'dll',
          'type': 'shared_library',
          'sources': [
            '<(gen_out_dir)/tip_resource_autogen.rc',
            'mozc_tip.def',
          ],
          'dependencies': [
            'gen_mozc_tip_resource_header',
            'tip_core',
          ],
          'msvs_settings': {
            'VCManifestTool': {
              'EmbedManifest': 'true',
            },
          },
        },
      ],
    }],
  ],
}
