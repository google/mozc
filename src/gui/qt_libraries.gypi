# Copyright 2010-2013, Google Inc.
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

# You can include this file in a target block to add Qt-related
# libraries to the target.
# Currently debug libraries are not supported on Mac
# (e.g. libQtCore_debug.a) and GNU/Linux.
{
  'conditions': [['use_qt=="YES"', {

  'variables': {
    'includes': ['qt_vars.gypi'],
    'conditions': [
      ['qt_dir', {
        'qt_cflags': [],
        'qt_include_dirs': ['<(qt_dir)/include'],
      }, {
        'conditions': [
          ['pkg_config_command', {
            'qt_cflags': ['<!@(<(pkg_config_command) --cflags QtGui QtCore)'],
            'qt_include_dirs': [],
          }, {
            'qt_cflags': [],
            'qt_include_dirs': ['<(qt_dir_env)/include'],
          }],
        ],
      }],
    ],
  },
  # compilation settings
  'cflags': ['<@(qt_cflags)'],
  'include_dirs': ['<@(qt_include_dirs)'],
  # link settings
  # TODO(yukawa): Use 'link_settings' so that linker settings can be passed
  #     to executables and loadable modules.
  'conditions': [
    ['OS=="mac"', {
      'conditions': [
        ['qt_dir', {
          # Supposing Qt libraries in qt_dir will be built as static libraries.
          'link_settings': {
            'xcode_settings': {
              'LIBRARY_SEARCH_PATHS': [
                '<(qt_dir)/lib',
              ],
            },
            'mac_framework_dirs': [
              '<(qt_dir)/lib',
            ],
            'libraries': [
              '<(qt_dir)/lib/QtCore.framework',
              '<(qt_dir)/lib/QtGui.framework',
            ],
          },
        }],
      ],
      'libraries': [
        '$(SDKROOT)/System/Library/Frameworks/Carbon.framework',
      ]
    }],
    ['OS=="linux"', {
      'conditions': [
        ['qt_dir', {
          'libraries': [
            '-L<(qt_dir)/lib',
            '-lQtGui',
            '-lQtCore',
            # Supposing Qt libraries in qt_dir will be built as static libraries
            # without support of pkg-config, we need to list all the
            # dependencies of QtGui.
            # See http://doc.qt.nokia.com/4.7/requirements-x11.html
            # pthread library is removed because it must not be specific to Qt.
            '<!@(<(pkg_config_command) --libs-only-L --libs-only-l'
            ' xrender xrandr xcursor xfixes xinerama fontconfig freetype2'
            ' xi xt xext x11'
            ' sm ice'
            ' gobject-2.0)',
          ],
        }, {
          'libraries': [
            '<!@(<(pkg_config_command) --libs QtGui QtCore)',
          ],
        }],
      ],
    }],
    # Workarounds related with clang.
    ['clang==1', {
      'conditions': [
        ['OS=="linux"', {
          'cflags': [
            # Temporal workaround against following false warning in Clang.
            # http://lists.cs.uiuc.edu/pipermail/cfe-dev/2012-June/022477.html
            '-Wno-uninitialized',
            # Suppress uncontrolable warnings on Qt 4.7.1 source code.
            '-Wno-unused-private-field',
          ],
        }],
        ['OS=="mac"', {
          'xcode_settings': {
            'WARNING_CFLAGS+': [
              # Temporal workaround against following false warning in Clang.
              # http://lists.cs.uiuc.edu/pipermail/cfe-dev/2012-June/022477.html
              '-Wno-uninitialized',
              # Suppress uncontrolable warnings on Qt 4.7.1 source code.
              '-Wno-unused-private-field',
            ],
          },
        }],
      ],
    }],
  ],

  }]],  # End of use_qt=="YES"
}
