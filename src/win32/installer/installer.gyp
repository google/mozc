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
    ['OS!="win" or use_wix!="YES" or use_qt!="YES"', {
      'targets': [
        {
          'target_name': 'mozc_installers_win',
          'type': 'none',
        },
      ],
    }, {  # else, that is: 'OS=="win" and use_wix=="YES" and use_qt=="YES"'
      'variables': {
        'relative_dir': 'win32/installer',
        'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
        'outdir32': '<(build_base)/<(CONFIGURATION_NAME)',
        'outdir64_dynamic': '<(build_base)/<(CONFIGURATION_NAME)Dynamic_x64',
        'outdir64': '<(build_base)/<(CONFIGURATION_NAME)_x64',
        'conditions': [
          ['branding=="GoogleJapaneseInput"', {
            'upgrade_code': 'C1A818AF-6EC9-49EF-ADCF-35A40475D156',
            'omaha_guid': 'DDCCD2A9-025E-4142-BCEB-F467B88CF830',
            'omaha_client_key': r'Software\Google\Update\Clients\{<(omaha_guid)}',
            'omaha_clientstate_key': r'Software\Google\Update\ClientState\{<(omaha_guid)}',
            'wxs_64bit_file': '<(DEPTH)/win32/installer/installer_64bit.wxs',
            'mozc_broker64_path': '<(outdir64_dynamic)/GoogleIMEJaBroker.exe',
            'mozc_ca64_path': '<(outdir64)/GoogleIMEJaInstallerHelper.dll',
            'mozc_cache_service64_path': '<(outdir64_dynamic)/GoogleIMEJaCacheService.exe',
            'mozc_content_dir': '<(DEPTH)/data',
            'mozc_renderer64_path': '<(outdir64_dynamic)/GoogleIMEJaRenderer.exe',
            'mozc_server64_path': '<(outdir64_dynamic)/GoogleIMEJaConverter.exe',
            'mozc_tip32_path': '<(outdir32)/GoogleIMEJaTIP32.dll',
            'mozc_tip64_path': '<(outdir64)/GoogleIMEJaTIP64.dll',
            'mozc_tool_path': '<(outdir64_dynamic)/GoogleIMEJaTool.exe',
          }, {  # branding!="GoogleJapaneseInput"
            'upgrade_code': 'DD94B570-B5E2-4100-9D42-61930C611D8A',
            'omaha_guid': '',
            'omaha_client_key': '',
            'omaha_clientstate_key': '',
            'wxs_64bit_file': '<(DEPTH)/win32/installer/installer_oss_64bit.wxs',
            'mozc_broker64_path': '<(outdir64_dynamic)/mozc_broker.exe',
            'mozc_ca64_path': '<(outdir64)/mozc_installer_helper.dll',
            'mozc_cache_service64_path': '<(outdir64_dynamic)/mozc_cache_service.exe',
            'mozc_content_dir': '<(DEPTH)/data',
            'mozc_renderer64_path': '<(outdir64_dynamic)/mozc_renderer.exe',
            'mozc_server64_path': '<(outdir64_dynamic)/mozc_server.exe',
            'mozc_tip32_path': '<(outdir32)/mozc_tip32.dll',
            'mozc_tip64_path': '<(outdir64)/mozc_tip64.dll',
            'mozc_tool_path': '<(outdir64_dynamic)/mozc_tool.exe',
          }],
        ],
        'upgrade_code': '<(upgrade_code)',
        'omaha_guid': '<(omaha_guid)',
        'omaha_client_key': '<(omaha_client_key)',
        'omaha_clientstate_key': '<(omaha_clientstate_key)',
        'release_redist_32bit_crt_dir': '<!(echo %VCToolsRedistDir%)/x86/Microsoft.VC<(vcruntime_ver).CRT',
        'release_redist_64bit_crt_dir': '<!(echo %VCToolsRedistDir%)/x64/Microsoft.VC<(vcruntime_ver).CRT',
        'mozc_cache_service64_path': '<(mozc_cache_service64_path)',
        'mozc_renderer64_path': '<(mozc_renderer64_path)',
        'mozc_server64_path': '<(mozc_server64_path)',
        'mozc_tip32_path': '<(mozc_tip32_path)',
        'mozc_tip64_path': '<(mozc_tip64_path)',
        'mozc_tool_path': '<(mozc_tool_path)',
        'mozc_broker64_path': '<(mozc_broker64_path)',
        'mozc_ca64_path': '<(mozc_ca64_path)',
        'mozc_content_dir': '<(mozc_content_dir)',
        'mozc_64bit_wixobj': '<(outdir32)/installer_64bit.wixobj',
        'mozc_64bit_msi': '<(outdir32)/<(branding)64.msi',
        'mozc_64bit_installer_inputs': [
          '<(mozc_broker64_path)',
          '<(mozc_ca64_path)',
          '<(mozc_cache_service64_path)',
          '<(mozc_renderer64_path)',
          '<(mozc_server64_path)',
          '<(mozc_tip32_path)',
          '<(mozc_tip64_path)',
          '<(mozc_tool_path)',
        ],
      },
      'targets': [
        {
          'target_name': 'mozc_64bit_installer',
          'type': 'none',
          'variables': {
            'wxs_file': '<(wxs_64bit_file)',
            'msi_file': '<(mozc_64bit_msi)',
          },
          'actions': [
            {
              'action_name': 'generate_msi',
              'conditions': [
                ['channel_dev==1', {
                  'variables': {
                    'omaha_channel_type': 'dev',
                  },
                }, { # else
                  'variables': {
                    'omaha_channel_type': 'stable',
                  },
                }],
              ],
              'variables': {
                'icon_path': '<(mozc_content_dir)/images/win/product_icon.ico',
                'document_dir': '<(mozc_content_dir)/installer',
              },
              'inputs': [
                # ninja.exe will invoke this action if any file listed here is
                # newer than files in 'outputs'.
                '<(wxs_file)',
                '<@(mozc_64bit_installer_inputs)',
              ],
              'outputs': [
                '<(msi_file)',
              ],
              'action': [
                'dotnet', 'tool', 'run', 'wix',
                'build',
                '-nologo',
                '-arch', 'x64',
                '-define', 'MozcVersion=<(version)',
                '-define', 'UpgradeCode=<(upgrade_code)',
                '-define', 'OmahaGuid=<(omaha_guid)',
                '-define', 'OmahaClientKey=<(omaha_client_key)',
                '-define', 'OmahaClientStateKey=<(omaha_clientstate_key)',
                '-define', 'OmahaChannelType=<(omaha_channel_type)',
                '-define', 'VSConfigurationName=<(CONFIGURATION_NAME)',
                '-define', 'ReleaseRedistCrt32Dir=<(release_redist_32bit_crt_dir)',
                '-define', 'ReleaseRedistCrt64Dir=<(release_redist_64bit_crt_dir)',
                '-define', 'AddRemoveProgramIconPath=<(icon_path)',
                '-define', 'MozcTIP32Path=<(mozc_tip32_path)',
                '-define', 'MozcTIP64Path=<(mozc_tip64_path)',
                '-define', 'MozcBroker64Path=<(mozc_broker64_path)',
                '-define', 'MozcServer64Path=<(mozc_server64_path)',
                '-define', 'MozcCacheService64Path=<(mozc_cache_service64_path)',
                '-define', 'MozcRenderer64Path=<(mozc_renderer64_path)',
                '-define', 'MozcToolPath=<(mozc_tool_path)',
                '-define', 'CustomActions64Path=<(mozc_ca64_path)',
                '-define', 'DocumentsDir=<(document_dir)',
                '-define', 'QtDir=<(qt_dir)',
                '-define', 'QtVer=<(qt_ver)',
                '-out', '<@(_outputs)',
                # We do not use '<@(_inputs)' here because it contains some
                # input files just for peoper rebuild condition.
                '-src', '<(wxs_file)',
              ],
              'message': 'WiX is generating <@(_outputs)',
            },
          ],
        },
        {
          'target_name': 'mozc_installers_win',
          'type': 'none',
          'dependencies': [
            'mozc_64bit_installer',
          ],
        },
        {
          'target_name': 'mozc_installers_win_size_check',
          'type': 'none',
          'actions': [
            {
              'action_name': 'mozc_installers_win_size_check',
              'inputs': [
                '<(mozc_64bit_msi)',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/mozc_installers_win_size_check_dummy',
              ],
              'action': [
                '<(python)',
                '<(mozc_oss_src_dir)/build_tools/binary_size_checker.py',
                '--target_filename',
                '<(mozc_64bit_msi)',
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
