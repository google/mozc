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
  'type': 'none',
  'dependencies': [
    'mozc_installer_version_file',
  ],
  'actions': [
    {
      'action_name': 'candle',
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
        'additional_args%': [],
        'conditions': [
          ['debug_crt_merge_module_id_prefix!=""', {
            'additional_args': [
              '-dDebugCrtMergeModuleIdPrefix=<(debug_crt_merge_module_id_prefix)',
            ],
          }],
          ['release_crt_merge_module_id_prefix!=""', {
            'additional_args': [
              '-dReleaseCrtMergeModuleIdPrefix=<(release_crt_merge_module_id_prefix)',
            ],
          }],
          ['debug_crt_merge_module_path!=""', {
            'additional_args': [
              '-dDebugCrtMergeModulePath=<(debug_crt_merge_module_path)',
            ],
          }],
          ['release_crt_merge_module_path!=""', {
            'additional_args': [
              '-dReleaseCrtMergeModulePath=<(release_crt_merge_module_path)',
            ],
          }],
          ['qt5core_dll_path!=""', {
            'additional_args': [
              '-dQt5CoreDllPath=<(qt5core_dll_path)',
            ],
          }],
          ['qt5cored_dll_path!=""', {
            'additional_args': [
              '-dQt5CoredDllPath=<(qt5cored_dll_path)',
            ],
          }],
          ['qt5gui_dll_path!=""', {
            'additional_args': [
              '-dQt5GuiDllPath=<(qt5gui_dll_path)',
            ],
          }],
          ['qt5guid_dll_path!=""', {
            'additional_args': [
              '-dQt5GuidDllPath=<(qt5guid_dll_path)',
            ],
          }],
          ['qt5widgets_dll_path!=""', {
            'additional_args': [
              '-dQt5WidgetsDllPath=<(qt5widgets_dll_path)',
            ],
          }],
          ['qt5widgetsd_dll_path!=""', {
            'additional_args': [
              '-dQt5WidgetsdDllPath=<(qt5widgetsd_dll_path)',
            ],
          }],
          ['qwindows_dll_path!=""', {
            'additional_args': [
              '-dQWindowsDllPath=<(qwindows_dll_path)',
            ],
          }],
          ['qwindowsd_dll_path!=""', {
            'additional_args': [
              '-dQWindowsdDllPath=<(qwindowsd_dll_path)',
            ],
          }],
          ['MSVS_VERSION=="2015" and use_qt=="YES"', {
            'additional_args': [
              r'-dUCRTDir=C:\Program Files (x86)\Windows Kits\10\Redist\ucrt\DLLs\x86',
            ],
          }],
          ['MSVS_VERSION=="2017" and use_qt=="YES"', {
            'additional_args': [
              r'-dUCRTDir=C:\Program Files (x86)\Windows Kits\10\Redist\ucrt\DLLs\x86',
            ],
          }],
        ],
        'omaha_guid': 'DDCCD2A9-025E-4142-BCEB-F467B88CF830',
        'omaha_client_key': r'Software\Google\Update\Clients\{<(omaha_guid)}',
        'omaha_clientstate_key': r'Software\Google\Update\ClientState\{<(omaha_guid)}',
        # Letters contained in the UpgradeCode property must be uppercase.
        # http://msdn.microsoft.com/en-us/library/465253cd.aspx
        'upgrade_code': 'C1A818AF-6EC9-49EF-ADCF-35A40475D156',
        'tool_elevation_policy_key': r'Software\Microsoft\Internet Explorer\Low Rights\ElevationPolicy\{94F14A1B-94E6-4303-A0DD-C1CED3D89DD4}',
        'broker32_elevation_policy_key': r'Software\Microsoft\Internet Explorer\Low Rights\ElevationPolicy\{F568BB28-0957-4A34-BEA7-D5F566B52410}',
        'broker64_elevation_policy_key': r'Software\Microsoft\Internet Explorer\Low Rights\ElevationPolicy\{04F68DA0-E43F-4CDC-8B79-C034A192787E}',
        'icon_path': '<(mozc_content_dir)/images/win/product_icon.ico',
        'document_dir': '<(mozc_content_dir)/installer',
      },
      'inputs': [
        '<(wxs_file)',
        '<(mozc_version_file)',
      ],
      'outputs': [
        '<(wixobj_file)',
      ],
      'action': [
        '<(wix_dir)/candle.exe',
        '-nologo',
        '-dMozcVersionFile=<(mozc_version_file)',
        '-dUpgradeCode=<(upgrade_code)',
        '-dOmahaGuid=<(omaha_guid)',
        '-dOmahaClientKey=<(omaha_client_key)',
        '-dOmahaClientStateKey=<(omaha_clientstate_key)',
        '-dOmahaChannelType=<(omaha_channel_type)',
        '-dVSConfigurationName=<(CONFIGURATION_NAME)',
        '-dMozcToolElevationPolicyRegKey=<(tool_elevation_policy_key)',
        '-dMozcBroker32ElevationPolicyRegKey=<(broker32_elevation_policy_key)',
        '-dMozcBroker64ElevationPolicyRegKey=<(broker64_elevation_policy_key)',
        '-dAddRemoveProgramIconPath=<(icon_path)',
        '-dMozcIME32Path=<(mozc_ime32_path)',
        '-dMozcIME64Path=<(mozc_ime64_path)',
        '-dMozcTIP32Path=<(mozc_tip32_path)',
        '-dMozcTIP64Path=<(mozc_tip64_path)',
        '-dMozcBroker32Path=<(mozc_broker32_path)',
        '-dMozcBroker64Path=<(mozc_broker64_path)',
        '-dMozcServer32Path=<(mozc_server32_path)',
        '-dMozcServer64Path=<(mozc_server64_path)',
        '-dMozcCacheService32Path=<(mozc_cache_service32_path)',
        '-dMozcCacheService64Path=<(mozc_cache_service64_path)',
        '-dMozcRenderer32Path=<(mozc_renderer32_path)',
        '-dMozcRenderer64Path=<(mozc_renderer64_path)',
        '-dMozcToolPath=<(mozc_tool_path)',
        '-dCustomActions32Path=<(mozc_ca32_path)',
        '-dCustomActions64Path=<(mozc_ca64_path)',
        '-dDocumentsDir=<(document_dir)',
        '<@(additional_args)',
        '-o', '<@(_outputs)',
        # We do not use '<@(_inputs)' here because it contains some
        # input files just for peoper rebiuld condition.
        '<(wxs_file)',
      ],
      'message': 'candle is generating <@(_outputs)',
    },
    {
      'action_name': 'generate_msi',
      'inputs': [
        # vcbuild will invoke this action if any file listed here is
        # newer than files in 'outputs'.
        '<(wixobj_file)',
      ],
      'outputs': [
        '<(msi_file)',
      ],
      'action': [
        '<(wix_dir)/light.exe',
        '-nologo',
        # Suppress harmless warnings caused by including VC runtime
        # merge modules.  See the following document for more details.
        # http://blogs.msdn.com/astebner/archive/2007/02/13/building-an-msi-using-wix-v3-0-that-includes-the-vc-8-0-runtime-merge-modules.aspx
        '-sw1055',
        '-sice:ICE03',
        '-sice:ICE30',
        '-sice:ICE60',
        '-sice:ICE82',
        '-sice:ICE83',
        # We intentionally remove *.ime from system folders as a part
        # of uninstallation.
        '-sice:ICE09',
        # Suppress the validation to address the LGHT0217 error.
        '-sval',
        '-o', '<@(_outputs)',
        # We do not use '<@(_inputs)' here because it contains some
        # input files just for peoper rebiuld condition.
        '<(wixobj_file)',
      ],
      'message': 'light is generating <@(_outputs)',
    },
  ],
}
