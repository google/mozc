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
        'outdir32': '<(build_base)/$(ConfigurationName)',
        'outdir64': '<(build_base)/$(ConfigurationName)64',
        'mozc_version_file': '<(gen_out_dir)/mozc_version.wxi',
        'mozc_ime32_path': '<(outdir32)/GIMEJa.ime',
        'mozc_ime64_path': '<(outdir64)/GIMEJa.ime',
        'mozc_server_path': '<(outdir32)/GoogleIMEJaConverter.exe',
        'mozc_cache_service_path': '<(outdir32)/GoogleIMEJaCacheService.exe',
        'mozc_renderer_path': '<(outdir32)/GoogleIMEJaRenderer.exe',
        'mozc_tool_path': '<(outdir32)/GoogleIMEJaTool.exe',
        'conditions': [
          ['use_qt=="YES"', {
            'mozc_zinnia_model_data_path%': '<(DEPTH)/third_party/zinnia/tomoe/handwriting-light-ja.model',
          }, {  # else, that is 'use_qt!="YES"'
            'mozc_zinnia_model_data_path': '',
          }],
        ],
        'mozc_broker32_path': '<(outdir32)/GoogleIMEJaBroker32.exe',
        'mozc_broker64_path': '<(outdir64)/GoogleIMEJaBroker64.exe',
        'mozc_ca32_path': '<(outdir32)/GoogleIMEJaInstallerHelper32.dll',
        'mozc_ca64_path': '<(outdir64)/GoogleIMEJaInstallerHelper64.dll',
        'mozc_data_dir': '<(DEPTH)/data',
        'mozc_imm_32bit_wixobj': '<(outdir32)/installer_imm_32bit.wixobj',
        'mozc_imm_32bit_msi': '<(outdir32)/GoogleJapaneseInput32.msi',
        'mozc_imm_64bit_wixobj': '<(outdir32)/installer_imm_64bit.wixobj',
        'mozc_imm_64bit_msi': '<(outdir32)/GoogleJapaneseInput64.msi',
        'mozc_32bit_binaries': [
          '<(mozc_ime32_path)',
          '<(mozc_server_path)',
          '<(mozc_cache_service_path)',
          '<(mozc_renderer_path)',
          '<(mozc_tool_path)',
          '<(mozc_broker32_path)',
          '<(mozc_ca32_path)',
        ],
        'mozc_64bit_binaries': [
          '<(mozc_ime64_path)',
          '<(mozc_broker64_path)',
          '<(mozc_ca64_path)',
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
                'python', '../../build_tools/replace_version.py',
                '--version_file', '../../mozc_version.txt',
                '--input', 'mozc_version_template.wxi',
                '--output', '<(mozc_version_file)',
              ],
              'message': '<(mozc_version_file)',
            },
          ],
        },
        {
          'target_name': 'mozc_imm_32bit_installer',
          'variables': {
            'wxs_file': '<(DEPTH)/win32/installer/installer_imm_32bit.wxs',
            'wixobj_file': '<(mozc_imm_32bit_wixobj)',
            'msi_file': '<(mozc_imm_32bit_msi)',
            'dependent_binaries': [
              '<@(mozc_32bit_binaries)',
            ],
          },
          'includes': [
            'wix.gypi',
          ],
        },
        {
          'target_name': 'mozc_imm_64bit_installer',
          'variables': {
            'wxs_file': '<(DEPTH)/win32/installer/installer_imm_64bit.wxs',
            'wixobj_file': '<(mozc_imm_64bit_wixobj)',
            'msi_file': '<(mozc_imm_64bit_msi)',
            'dependent_binaries': [
              '<@(mozc_32bit_binaries)',
              '<@(mozc_64bit_binaries)',
            ],
          },
          'includes': [
            'wix.gypi',
          ],
        },
      ],
    }],
  ],
}
