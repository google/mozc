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

# You can include this file in a target block to add Qt-related
# libraries to the target.
# Currently debug libraries are not supported on Mac
# (e.g. libQtCore_debug.a) and GNU/Linux.
{
  'conditions': [['use_qt=="YES"', {

  # link settings
  # TODO(yukawa): Use 'link_settings' so that linker settings can be passed
  #     to executables and loadable modules.
  'conditions': [
    ['qt_dir and target_platform=="Windows"', {
      'include_dirs': [
        '<(qt_dir)/include',
        '<(qt_dir)/include/QtCore',
        '<(qt_dir)/include/QtGui',
        '<(qt_dir)/include/QtWidgets',
      ],
      'configurations': {
        'Debug_Base': {
          'msvs_settings': {
            'VCLinkerTool': {
              'AdditionalLibraryDirectories': [
                '<(qt_dir)/lib',
              ],
              'AdditionalDependencies': [
                'Qt5Cored.lib',
                'Qt5Guid.lib',
                'Qt5Widgetsd.lib',
              ],
            },
          },
        },
        'Release_Base': {
          'msvs_settings': {
            'VCLinkerTool': {
              'AdditionalLibraryDirectories': [
                '<(qt_dir)/lib',
              ],
              'AdditionalDependencies': [
                'Qt5Core.lib',
                'Qt5Gui.lib',
                'Qt5Widgets.lib',
              ],
            },
          },
        },
      },
    }],
    ['target_platform=="Mac"', {
      'conditions': [
        ['qt_dir', {
          'include_dirs': [
            '<(qt_dir)/include',
            '<(qt_dir)/include/QtCore',
            '<(qt_dir)/include/QtGui',
            '<(qt_dir)/include/QtWidgets',
          ],
          'xcode_settings': {
            'WARNING_CFLAGS': ['-Wno-inconsistent-missing-override'],
          },
          # Supposing Qt libraries in qt_dir will be built as static libraries.
          'link_settings': {
            'xcode_settings': {
              'LIBRARY_SEARCH_PATHS': ['<(qt_dir)/lib'],
            },
            'mac_framework_dirs': [
              '<(qt_dir)/lib',
            ],
            'libraries': [
              '<(qt_dir)/lib/QtCore.framework',
              '<(qt_dir)/lib/QtGui.framework',
              '<(qt_dir)/lib/QtWidgets.framework',
            ]
          },
        }],
      ],
      'libraries': [
        '$(SDKROOT)/System/Library/Frameworks/Carbon.framework',
      ]
    }],
    ['target_platform=="Linux"', {
      'cflags': ['<!@(pkg-config --cflags Qt5Widgets Qt5Gui Qt5Core)'],
      'libraries': ['<!@(pkg-config --libs Qt5Widgets Qt5Gui Qt5Core)'],
    }],
    # Workarounds related with clang.
    ['(_toolset=="target" and compiler_target=="clang") or '
     '(_toolset=="host" and compiler_host=="clang")', {
      'cflags': [
        # Temporal workaround against following false warning in Clang.
        # http://lists.cs.uiuc.edu/pipermail/cfe-dev/2012-June/022477.html
        '-Wno-uninitialized',
        # Suppress uncontrolable warnings on Qt 4.7.1 source code.
        '-Wno-unused-private-field',
      ],
    }],
  ],

  }]],  # End of use_qt=="YES"
}
