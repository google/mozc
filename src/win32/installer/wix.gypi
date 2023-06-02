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
        ],
        'icon_path': '<(mozc_content_dir)/images/win/product_icon.ico',
        'document_dir': '<(mozc_content_dir)/installer',
      },
      'inputs': [
        '<(wxs_file)',
      ],
      'outputs': [
        '<(wixobj_file)',
      ],
      'action': [
        '<(wix_dir)/candle.exe',
        '-nologo',
        '-dMozcVersion=<(version)',
        '-dUpgradeCode=<(upgrade_code)',
        '-dOmahaGuid=<(omaha_guid)',
        '-dOmahaClientKey=<(omaha_client_key)',
        '-dOmahaClientStateKey=<(omaha_clientstate_key)',
        '-dOmahaChannelType=<(omaha_channel_type)',
        '-dVSConfigurationName=<(CONFIGURATION_NAME)',
        '-dReleaseRedistCrt32Dir=<(release_redist_32bit_crt_dir)',
        '-dReleaseRedistCrt64Dir=<(release_redist_64bit_crt_dir)',
        '-dAddRemoveProgramIconPath=<(icon_path)',
        '-dMozcTIP32Path=<(mozc_tip32_path)',
        '-dMozcTIP64Path=<(mozc_tip64_path)',
        '-dMozcBroker64Path=<(mozc_broker64_path)',
        '-dMozcServer64Path=<(mozc_server64_path)',
        '-dMozcCacheService64Path=<(mozc_cache_service64_path)',
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
        # ninja.exe will invoke this action if any file listed here is
        # newer than files in 'outputs'.
        '<(wixobj_file)',
        '<@(stamp_files)',
      ],
      'outputs': [
        '<(msi_file)',
      ],
      'action': [
        '<(wix_dir)/light.exe',
        '-nologo',
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
