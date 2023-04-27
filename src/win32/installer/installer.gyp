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
  'conditions': [
    ['OS!="win" or use_wix!="YES"', {
      'targets': [
        {
          'target_name': 'mozc_installers_win',
          'type': 'none',
        },
      ],
    }, {  # else, that is: 'OS=="win" and use_wix=="YES"'
      'variables': {
        'relative_dir': 'win32/installer',
        'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
        'outdir32': '<(build_base)/<(CONFIGURATION_NAME)',
        'outdir32_dynamic': '<(build_base)/<(CONFIGURATION_NAME)Dynamic',
        'outdir64': '<(build_base)/<(CONFIGURATION_NAME)_x64',
        'conditions': [
          ['branding=="GoogleJapaneseInput"', {
            'upgrade_code': 'C1A818AF-6EC9-49EF-ADCF-35A40475D156',
            'omaha_guid': 'DDCCD2A9-025E-4142-BCEB-F467B88CF830',
            'omaha_client_key': r'Software\Google\Update\Clients\{<(omaha_guid)}',
            'omaha_clientstate_key': r'Software\Google\Update\ClientState\{<(omaha_guid)}',
            'wxs_32bit_file': '<(DEPTH)/win32/installer/installer_32bit.wxs',
            'wxs_64bit_file': '<(DEPTH)/win32/installer/installer_64bit.wxs',
            'mozc_broker32_path': '<(outdir32)/GoogleIMEJaBroker.exe',
            'mozc_broker64_path': '<(outdir64)/GoogleIMEJaBroker.exe',
            'mozc_ca32_path': '<(outdir32)/GoogleIMEJaInstallerHelper32.dll',
            'mozc_ca64_path': '<(outdir64)/GoogleIMEJaInstallerHelper64.dll',
            'mozc_cache_service32_path': '<(outdir32)/GoogleIMEJaCacheService.exe',
            'mozc_cache_service64_path': '<(outdir64)/GoogleIMEJaCacheService.exe',
            'mozc_content_dir': '<(DEPTH)/data',
            'mozc_renderer32_path': '<(outdir32)/GoogleIMEJaRenderer.exe',
            'mozc_renderer64_path': '<(outdir64)/GoogleIMEJaRenderer.exe',
            'mozc_server32_path': '<(outdir32)/GoogleIMEJaConverter.exe',
            'mozc_server64_path': '<(outdir64)/GoogleIMEJaConverter.exe',
            'mozc_tip32_path': '<(outdir32)/GoogleIMEJaTIP32.dll',
            'mozc_tip64_path': '<(outdir64)/GoogleIMEJaTIP64.dll',
            'mozc_tool_name': 'GoogleIMEJaTool.exe',
          }, {  # branding!="GoogleJapaneseInput"
            'upgrade_code': 'DD94B570-B5E2-4100-9D42-61930C611D8A',
            'omaha_guid': '',
            'omaha_client_key': '',
            'omaha_clientstate_key': '',
            'wxs_32bit_file': '<(DEPTH)/win32/installer/installer_oss_32bit.wxs',
            'wxs_64bit_file': '<(DEPTH)/win32/installer/installer_oss_64bit.wxs',
            'mozc_broker32_path': '<(outdir32)/mozc_broker.exe',
            'mozc_broker64_path': '<(outdir64)/mozc_broker.exe',
            'mozc_ca32_path': '<(outdir32)/mozc_installer_helper32.dll',
            'mozc_ca64_path': '<(outdir64)/mozc_installer_helper64.dll',
            'mozc_cache_service32_path': '<(outdir32)/mozc_cache_service.exe',
            'mozc_cache_service64_path': '<(outdir64)/mozc_cache_service.exe',
            'mozc_content_dir': '<(DEPTH)/data',
            'mozc_renderer32_path': '<(outdir32)/mozc_renderer.exe',
            'mozc_renderer64_path': '<(outdir64)/mozc_renderer.exe',
            'mozc_server32_path': '<(outdir32)/mozc_server.exe',
            'mozc_server64_path': '<(outdir64)/mozc_server.exe',
            'mozc_tip32_path': '<(outdir32)/mozc_ja_tip32.dll',
            'mozc_tip64_path': '<(outdir64)/mozc_ja_tip64.dll',
            'mozc_tool_name': 'mozc_tool.exe',
          }],
        ],
        'variables': {
          'qt5core_dll_path': '',
          'qt5cored_dll_path': '',
          'qt5gui_dll_path': '',
          'qt5guid_dll_path': '',
          'qt5widgets_dll_path': '',
          'qt5widgetsd_dll_path': '',
          'qwindows_dll_path': '',
          'qwindowsd_dll_path': '',
          'mozc_tool_name' : '<(mozc_tool_name)',
          'mozc_tool_path': '<(outdir32)/<(mozc_tool_name)',
          'conditions': [
            ['use_qt=="YES"', {
              'mozc_tool_path': '<(outdir32_dynamic)/<(mozc_tool_name)',
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
        'upgrade_code': '<(upgrade_code)',
        'omaha_guid': '<(omaha_guid)',
        'omaha_client_key': '<(omaha_client_key)',
        'omaha_clientstate_key': '<(omaha_clientstate_key)',
        'release_redist_32bit_crt_dir': '<!(echo %VCToolsRedistDir%)/x86/Microsoft.VC<(vcruntime_ver).CRT',
        'release_redist_64bit_crt_dir': '<!(echo %VCToolsRedistDir%)/x64/Microsoft.VC<(vcruntime_ver).CRT',
        'qt5core_dll_path': '<(qt5core_dll_path)',
        'qt5cored_dll_path': '<(qt5cored_dll_path)',
        'qt5gui_dll_path': '<(qt5gui_dll_path)',
        'qt5guid_dll_path': '<(qt5guid_dll_path)',
        'qt5widgets_dll_path': '<(qt5widgets_dll_path)',
        'qt5widgetsd_dll_path': '<(qt5widgetsd_dll_path)',
        'qwindows_dll_path': '<(qwindows_dll_path)',
        'qwindowsd_dll_path': '<(qwindowsd_dll_path)',
        'mozc_cache_service32_path': '<(mozc_cache_service32_path)',
        'mozc_cache_service64_path': '<(mozc_cache_service64_path)',
        'mozc_renderer32_path': '<(mozc_renderer32_path)',
        'mozc_renderer64_path': '<(mozc_renderer64_path)',
        'mozc_server32_path': '<(mozc_server32_path)',
        'mozc_server64_path': '<(mozc_server64_path)',
        'mozc_tip32_path': '<(mozc_tip32_path)',
        'mozc_tip64_path': '<(mozc_tip64_path)',
        'mozc_tool_path': '<(mozc_tool_path)',
        'mozc_broker32_path': '<(mozc_broker32_path)',
        'mozc_broker64_path': '<(mozc_broker64_path)',
        'mozc_ca32_path': '<(mozc_ca32_path)',
        'mozc_ca64_path': '<(mozc_ca64_path)',
        'mozc_content_dir': '<(mozc_content_dir)',
        'mozc_32bit_wixobj': '<(outdir32)/installer_32bit.wixobj',
        'mozc_64bit_wixobj': '<(outdir32)/installer_64bit.wixobj',
        'mozc_32bit_msi': '<(outdir32)/<(branding)32.msi',
        'mozc_64bit_msi': '<(outdir32)/<(branding)64.msi',
        'mozc_32bit_postbuild_stamps': [
          '<(mozc_broker32_path).postbuild',
          '<(mozc_ca32_path).postbuild',
          '<(mozc_cache_service32_path).postbuild',
          '<(mozc_renderer32_path).postbuild',
          '<(mozc_server32_path).postbuild',
          '<(mozc_tip32_path).postbuild',
          '<(mozc_tool_path).postbuild',
        ],
        'mozc_64bit_postbuild_stamps': [
          '<(mozc_broker64_path).postbuild',
          '<(mozc_ca32_path).postbuild',
          '<(mozc_ca64_path).postbuild',
          '<(mozc_cache_service64_path).postbuild',
          '<(mozc_renderer64_path).postbuild',
          '<(mozc_server64_path).postbuild',
          '<(mozc_tip32_path).postbuild',
          '<(mozc_tip64_path).postbuild',
          '<(mozc_tool_path).postbuild',
        ],
      },
      'targets': [
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
            'wxs_file': '<(wxs_32bit_file)',
            'wixobj_file': '<(mozc_32bit_wixobj)',
            'stamp_files': '<(mozc_32bit_postbuild_stamps)',
            'msi_file': '<(mozc_32bit_msi)',
          },
          'includes': [
            'wix.gypi',
          ],
        },
        {
          'target_name': 'mozc_64bit_installer',
          'variables': {
            'wxs_file': '<(wxs_64bit_file)',
            'wixobj_file': '<(mozc_64bit_wixobj)',
            'stamp_files': '<(mozc_64bit_postbuild_stamps)',
            'msi_file': '<(mozc_64bit_msi)',
          },
          'includes': [
            'wix.gypi',
          ],
        },
        {
          'target_name': 'mozc_installer32_postbuild',
          'variables': { 'target_file': '<(mozc_32bit_msi)' },
          'includes': [ 'postbuilds_win.gypi' ],
        },
        {
          'target_name': 'mozc_installer64_postbuild',
          'variables': { 'target_file': '<(mozc_64bit_msi)' },
          'includes': [ 'postbuilds_win.gypi' ],
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
