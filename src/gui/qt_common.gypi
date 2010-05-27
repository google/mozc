# Copyright 2010, Google Inc.
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

# This include file add various settings for Qt as
# target_defaults.
{
  'variables': {
    'qt_basepath': '$(QTDIR)',
    'conditions': [
      ['OS=="linux"', {
       'qt_cflags': ' <!@(pkg-config --cflags QtCore QtGui)',
      },{  # OS!="linux"
       'qt_cflags': '<(qt_basepath)/include',
      }],
    ],
  },
  'target_defaults': {
    'configurations': {
      'Common_Base': {
        'include_dirs': [
          '<(qt_cflags)',
        ],
      },
    },
  },
  'conditions': [
    ['OS=="win"', {
      'target_defaults': {
        'configurations': {
          'Common_Base': {
            'msvs_settings': {
              'VCLinkerTool': {
                'AdditionalDependencies': [
                  'Ws2_32.lib',
                ],
                'AdditionalLibraryDirectories': [
                  '<(qt_basepath)/lib',
                ],
              },
            },
          },
          'Debug_Base': {
            'msvs_settings': {
              'VCLinkerTool': {
                'AdditionalDependencies': [
                  'QtCored.lib',
                  'QtGuid.lib',
                ],
              },
            },
          },
          'Optimize_Base': {
            'msvs_settings': {
              'VCLinkerTool': {
                'AdditionalDependencies': [
                  'QtCore.lib',
                  'QtGui.lib',
                ],
              },
            },
          },
        },
      },
    }],
    ['OS=="mac"', {
      'target_defaults': {
        'configurations': {
          'Common_Base': {
            'xcode_settings': {
              'LIBRARY_SEARCH_PATHS': [
                '<(qt_basepath)/lib',
              ],
            },
          },
        },
      },
    }],
    ['OS=="linux"', {
      'target_defaults': {
        'configurations': {
          'Common_Base': {
            'ldflags': [
            ' <!@(pkg-config --libs-only-L QtCore QtGui)',
            ],
          },
        },
      },
    }],
  ],
}
