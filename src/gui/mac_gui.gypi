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

# This include file adds actions to create symbolic link applications for GUIs.
{
  'product_name': '<(product_name)',
  'conditions': [
    ['use_qt=="YES"', {
      'sources': [
        'tool/mozc_tool_main.cc',
      ],
      'mac_bundle_resources': ['<(mozc_oss_src_dir)/data/mac/qt.conf'],
      'dependencies': [
        'config_dialog_mac',
        'gen_mozc_tool_info_plist',
        'mozc_tool_lib',
      ],
      'postbuilds': [
        {
          'postbuild_name': 'Change the reference to frameworks',
          'action': [
            '<(python)', '<(mozc_src_dir)/build_tools/change_reference_mac.py',
            '--qtdir', '<(qt_dir)',
            '--qtver', '<(qt_ver)',
            '--target',
            '${BUILT_PRODUCTS_DIR}/<(product_name).app/Contents/MacOS/<(product_name)',
          ],
        },
      ],
    }, { # else
      'sources': [
        'tool/mozc_tool_main_noqt.cc',
      ],
      'dependencies': [
        'gen_mozc_tool_info_plist',
      ],
    }],
  ],
  'includes': ['../gyp/postbuilds_mac.gypi'],
}
