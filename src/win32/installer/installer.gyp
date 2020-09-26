# Copyright 2010-2020, Google Inc.
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
  'conditions': [
    ['OS!="win" or use_wix!="YES" or branding!="GoogleJapaneseInput"', {
      # Add a dummy target because at least one target is needed in a gyp file.
      'targets': [
        {
          'target_name': 'dummy_win32_installer',
          'type': 'none',
        },
      ],
    }, {  # else, that is: 'OS=="win" and use_wix=="YES" and branding=="GoogleJapaneseInput"'
      'variables': {
        'relative_dir': 'win32/installer',
        'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
        'outdir32': '<(build_base)/<(CONFIGURATION_NAME)',
        'outdir32_dynamic': '<(build_base)/<(CONFIGURATION_NAME)Dynamic',
        'outdir64': '<(build_base)/<(CONFIGURATION_NAME)_x64',
        'mozc_version_file': '<(gen_out_dir)/mozc_version.wxi',
        'mozc_ime32_path': '<(outdir32)/GIMEJa.ime',
        'mozc_ime64_path': '<(outdir64)/GIMEJa.ime',
        'mozc_tip32_path': '<(outdir32)/GoogleIMEJaTIP32.dll',
        'mozc_tip64_path': '<(outdir64)/GoogleIMEJaTIP64.dll',
        'mozc_server32_path': '<(outdir32)/GoogleIMEJaConverter.exe',
        'mozc_server64_path': '<(outdir64)/GoogleIMEJaConverter.exe',
        'mozc_cache_service32_path': '<(outdir32)/GoogleIMEJaCacheService.exe',
        'mozc_cache_service64_path': '<(outdir64)/GoogleIMEJaCacheService.exe',
        'mozc_renderer32_path': '<(outdir32)/GoogleIMEJaRenderer.exe',
        'mozc_renderer64_path': '<(outdir64)/GoogleIMEJaRenderer.exe',
        'variables': {
          'debug_crt_merge_module_id_prefix': 'DebugCRT140',
          'release_crt_merge_module_id_prefix': 'CRT140',
          'debug_crt_merge_module_path': 'C:/Program Files (x86)/Common Files/Merge Modules/Microsoft_VC140_DebugCRT_x86.msm',
          'release_crt_merge_module_path': 'C:/Program Files (x86)/Common Files/Merge Modules/Microsoft_VC140_CRT_x86.msm',
          'qt5core_dll_path': '',
          'qt5cored_dll_path': '',
          'qt5gui_dll_path': '',
          'qt5guid_dll_path': '',
          'qt5widgets_dll_path': '',
          'qt5widgetsd_dll_path': '',
          'qwindows_dll_path': '',
          'qwindowsd_dll_path': '',
          'mozc_tool_path': '<(outdir32)/GoogleIMEJaTool.exe',
          'conditions': [
            ['use_qt=="YES"', {
              'mozc_tool_path': '<(outdir32_dynamic)/GoogleIMEJaTool.exe',
              'qt5core_dll_path': '<(qt_dir)/bin/Qt5Core.dll',
              'qt5cored_dll_path': '<(qt_dir)/bin/Qt5Cored.dll',
              'qt5gui_dll_path': '<(qt_dir)/bin/Qt5Gui.dll',
              'qt5guid_dll_path': '<(qt_dir)/bin/Qt5Guid.dll',
              'qt5widgets_dll_path': '<(qt_dir)/bin/Qt5Widgets.dll',
              'qt5widgetsd_dll_path': '<(qt_dir)/bin/Qt5Widgetsd.dll',
              'qwindows_dll_path': '<(qt_dir)/plugins/platforms/qwindows.dll',
              'qwindowsd_dll_path': '<(qt_dir)/plugins/platforms/qwindowsd.dll',
            }],
          ],
        },
        'debug_crt_merge_module_id_prefix': '<(debug_crt_merge_module_id_prefix)',
        'release_crt_merge_module_id_prefix': '<(release_crt_merge_module_id_prefix)',
        'debug_crt_merge_module_path': '<(debug_crt_merge_module_path)',
        'release_crt_merge_module_path': '<(release_crt_merge_module_path)',
        'qt5core_dll_path': '<(qt5core_dll_path)',
        'qt5cored_dll_path': '<(qt5cored_dll_path)',
        'qt5gui_dll_path': '<(qt5gui_dll_path)',
        'qt5guid_dll_path': '<(qt5guid_dll_path)',
        'qt5widgets_dll_path': '<(qt5widgets_dll_path)',
        'qt5widgetsd_dll_path': '<(qt5widgetsd_dll_path)',
        'qwindows_dll_path': '<(qwindows_dll_path)',
        'qwindowsd_dll_path': '<(qwindowsd_dll_path)',
        'mozc_tool_path': '<(mozc_tool_path)',
        'mozc_broker32_path': '<(outdir32)/GoogleIMEJaBroker32.exe',
        'mozc_broker64_path': '<(outdir64)/GoogleIMEJaBroker64.exe',
        'mozc_ca32_path': '<(outdir32)/GoogleIMEJaInstallerHelper32.dll',
        'mozc_ca64_path': '<(outdir64)/GoogleIMEJaInstallerHelper64.dll',
        'mozc_content_dir': '<(DEPTH)/data',
        'mozc_32bit_wixobj': '<(outdir32)/installer_32bit.wixobj',
        'mozc_64bit_wixobj': '<(outdir32)/installer_64bit.wixobj',
        'mozc_32bit_msi': '<(outdir32)/GoogleJapaneseInput32.msi',
        'mozc_64bit_msi': '<(outdir32)/GoogleJapaneseInput64.msi',
        'mozc_32bit_binaries': [
          '<(mozc_ime32_path)',
          '<(mozc_tip32_path)',
          '<(mozc_server32_path)',
          '<(mozc_cache_service32_path)',
          '<(mozc_renderer32_path)',
          '<(mozc_tool_path)',
          '<(mozc_broker32_path)',
          '<(mozc_ca32_path)',
        ],
        'mozc_32bit_postbuild_targets': [
          'mozc_ime32_postbuild',
          'mozc_tip32_postbuild',
          'mozc_server32_postbuild',
          'mozc_cache_service32_postbuild',
          'mozc_renderer32_postbuild',
          'mozc_tool_postbuild',
          'mozc_broker32_postbuild',
          'mozc_ca32_postbuild',
        ],
        'mozc_64bit_binaries': [
          '<(mozc_ime64_path)',
          '<(mozc_tip64_path)',
          '<(mozc_broker64_path)',
          '<(mozc_ca64_path)',
        ],
        'mozc_64bit_postbuild_targets': [
          'mozc_ime64_postbuild',
          'mozc_tip64_postbuild',
          'mozc_broker64_postbuild',
          'mozc_ca64_postbuild',
        ],
      },
      'targets': [
        {
          'target_name': 'mozc_installer_version_file',
          'type': 'none',
          'actions': [
            {
              'action_name': 'gen_installer_version_file',
              'inputs': [
                '../../mozc_version.txt',
                '../../build_tools/replace_version.py',
                'mozc_version_template.wxi',
              ],
              'outputs': [
                '<(mozc_version_file)',
              ],
              'action': [
                '<(python)', '../../build_tools/replace_version.py',
                '--version_file', '../../mozc_version.txt',
                '--input', 'mozc_version_template.wxi',
                '--output', '<(mozc_version_file)',
                '--branding', '<(branding)',
              ],
              'message': '<(mozc_version_file)',
            },
          ],
        },
        {
          'target_name': 'mozc_ime32_postbuild',
          'variables': { 'target_file': '<(mozc_ime32_path)' },
          'includes': [ 'postbuilds_win.gypi' ],
        },
        {
          'target_name': 'mozc_tip32_postbuild',
          'variables': { 'target_file': '<(mozc_tip32_path)' },
          'includes': [ 'postbuilds_win.gypi' ],
        },
        {
          'target_name': 'mozc_server32_postbuild',
          'variables': { 'target_file': '<(mozc_server32_path)' },
          'includes': [ 'postbuilds_win.gypi' ],
        },
        {
          'target_name': 'mozc_server64_postbuild',
          'variables': { 'target_file': '<(mozc_server64_path)' },
          'includes': [ 'postbuilds_win.gypi' ],
        },
        {
          'target_name': 'mozc_cache_service32_postbuild',
          'variables': { 'target_file': '<(mozc_cache_service32_path)' },
          'includes': [ 'postbuilds_win.gypi' ],
        },
        {
          'target_name': 'mozc_cache_service64_postbuild',
          'variables': { 'target_file': '<(mozc_cache_service64_path)' },
          'includes': [ 'postbuilds_win.gypi' ],
        },
        {
          'target_name': 'mozc_renderer32_postbuild',
          'variables': { 'target_file': '<(mozc_renderer32_path)' },
          'includes': [ 'postbuilds_win.gypi' ],
        },
        {
          'target_name': 'mozc_renderer64_postbuild',
          'variables': { 'target_file': '<(mozc_renderer64_path)' },
          'includes': [ 'postbuilds_win.gypi' ],
        },
        {
          'target_name': 'mozc_tool_postbuild',
          'variables': { 'target_file': '<(mozc_tool_path)' },
          'includes': [ 'postbuilds_win.gypi' ],
        },
        {
          'target_name': 'mozc_broker32_postbuild',
          'variables': { 'target_file': '<(mozc_broker32_path)' },
          'includes': [ 'postbuilds_win.gypi' ],
        },
        {
          'target_name': 'mozc_ca32_postbuild',
          'variables': { 'target_file': '<(mozc_ca32_path)' },
          'includes': [ 'postbuilds_win.gypi' ],
        },
        {
          'target_name': 'mozc_ime64_postbuild',
          'variables': { 'target_file': '<(mozc_ime64_path)' },
          'includes': [ 'postbuilds_win.gypi' ],
        },
        {
          'target_name': 'mozc_tip64_postbuild',
          'variables': { 'target_file': '<(mozc_tip64_path)' },
          'includes': [ 'postbuilds_win.gypi' ],
        },
        {
          'target_name': 'mozc_broker64_postbuild',
          'variables': { 'target_file': '<(mozc_broker64_path)' },
          'includes': [ 'postbuilds_win.gypi' ],
        },
        {
          'target_name': 'mozc_ca64_postbuild',
          'variables': { 'target_file': '<(mozc_ca64_path)' },
          'includes': [ 'postbuilds_win.gypi' ],
        },
        {
          'target_name': 'mozc_32bit_installer',
          'variables': {
            'wxs_file': '<(DEPTH)/win32/installer/installer_32bit.wxs',
            'wixobj_file': '<(mozc_32bit_wixobj)',
            'msi_file': '<(mozc_32bit_msi)',
          },
          'dependencies': [
            '<@(mozc_32bit_postbuild_targets)',
          ],
          'includes': [
            'wix.gypi',
          ],
        },
        {
          'target_name': 'mozc_64bit_installer',
          'variables': {
            'wxs_file': '<(DEPTH)/win32/installer/installer_64bit.wxs',
            'wixobj_file': '<(mozc_64bit_wixobj)',
            'msi_file': '<(mozc_64bit_msi)',
          },
          'dependencies': [
            '<@(mozc_32bit_postbuild_targets)',
            '<@(mozc_64bit_postbuild_targets)',
          ],
          'includes': [
            'wix.gypi',
          ],
        },
        {
          'target_name': 'mozc_installer32_postbuild',
          'variables': { 'target_file': '<(mozc_32bit_msi)' },
          'includes': [ 'postbuilds_win.gypi' ],
          'dependencies': [
            'mozc_32bit_installer',
          ],
        },
        {
          'target_name': 'mozc_installer64_postbuild',
          'variables': { 'target_file': '<(mozc_64bit_msi)' },
          'includes': [ 'postbuilds_win.gypi' ],
          'dependencies': [
            'mozc_64bit_installer',
          ],
        },
        {
          'target_name': 'mozc_installers_win',
          'type': 'none',
          'dependencies': [
            'mozc_installer32_postbuild',
            'mozc_installer64_postbuild',
          ],
        },
        {
          'target_name': 'mozc_installers_win_versioning',
          'type': 'none',
          'actions': [
            {
              'action_name': 'mozc_installers_win_versioning',
              'inputs': [
                '../../mozc_version.txt',
                '../../build_tools/versioning_files.py',
                '<(mozc_32bit_msi).postbuild',
                '<(mozc_64bit_msi).postbuild',
                '<(mozc_32bit_msi)',
                '<(mozc_64bit_msi)',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/mozc_installers_win_versioning_dummy',
              ],
              'action': [
                '<(python)',
                '../../build_tools/versioning_files.py',
                '--version_file', '../../mozc_version.txt',
                '--configuration', '<(CONFIGURATION_NAME)',
                '<(mozc_32bit_msi)',
                '<(mozc_64bit_msi)',
              ],
            },
          ],
          'dependencies': [
            'mozc_installers_win',
          ],
        },
        {
          'target_name': 'mozc_installers_win_size_check',
          'type': 'none',
          'actions': [
            {
              'action_name': 'mozc_installers_win_size_check',
              'inputs': [
                '<(mozc_32bit_msi)',
                '<(mozc_64bit_msi)',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/mozc_installers_win_size_check_dummy',
              ],
              'action': [
                '<(python)',
                '../../build_tools/binary_size_checker.py',
                '--target_filename',
                '<(mozc_32bit_msi),<(mozc_64bit_msi)',
              ],
            },
          ],
          'dependencies': [
            'mozc_installers_win',
          ],
        },
      ],
    }],
  ],
}
