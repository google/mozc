# Copyright 2010-2014, Google Inc.
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

# This include file adds various settings for Qt as target_defaults.
{
  'conditions': [['use_qt=="YES"', {

  'variables': {
    'includes': ['qt_vars.gypi'],
    'conditions': [
      ['use_dynamically_linked_qt==1', {
        'libfile_postfix_for_win': '4',
        'win32_dlls_for_qt': [],
      }, {  # else
        'libfile_postfix_for_win': '',
        'win32_dlls_for_qt': [
          'winmm.lib',
          'ws2_32.lib',
        ],
      }],
    ],
  },
  # MSVS specific settings
  'target_defaults': {
    'configurations': {
      'Common_Base': {
        'msvs_settings': {
          'VCLinkerTool': {
            'AdditionalDependencies': [
              '<@(win32_dlls_for_qt)',
            ],
            'conditions': [
              ['qt_dir', {
                'AdditionalLibraryDirectories': [
                  '<(qt_dir)/lib',
                ],
              }, {
                'AdditionalLibraryDirectories': [
                  '<(qt_dir_env)/lib',
                ],
              }],
            ],
          },
        },
      },
      'Debug_Base': {
        'msvs_settings': {
          'VCLinkerTool': {
            'AdditionalDependencies': [
              'QtCored<(libfile_postfix_for_win).lib',
              'QtGuid<(libfile_postfix_for_win).lib',
            ],
          },
        },
      },
      'Optimize_Base': {
        'msvs_settings': {
          'VCLinkerTool': {
            'AdditionalDependencies': [
              'QtCore<(libfile_postfix_for_win).lib',
              'QtGui<(libfile_postfix_for_win).lib',
            ],
          },
        },
      },
    },
  },

  }]],  # End of use_qt=="YES"
}
