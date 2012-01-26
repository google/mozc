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

# This file provides a common action for Qt rcc command.
{
  'conditions': [['use_qt=="YES"', {

  'variables': {
    'includes': ['qt_vars.gypi'],
    'conditions': [
      ['qt_dir', {
        'rcc_path': '<(qt_dir)/bin/rcc<(EXECUTABLE_SUFFIX)',
      }, {
        'conditions': [
          ['pkg_config_command', {
            # seems that --variable=rcc_location is not supported
            'rcc_path':
              '<!(<(pkg_config_command) --variable=exec_prefix QtGui)/bin/rcc',
          }, {
            'rcc_path': '<(qt_dir_env)/bin/rcc<(EXECUTABLE_SUFFIX)',
          }],
        ],
      }],
    ],
  },
  'actions': [
    {
      # Need to use actions for qrc files to workaround a gyp issue.
      'action_name': 'qrc',
      'inputs': [
        '<(subdir)/<(qrc_base_name).qrc',
      ],
      'outputs': [
        '<(gen_out_dir)/<(subdir)/qrc_<(qrc_base_name).cc'
      ],
      'action': [
        '<(rcc_path)',
        '-o', '<(gen_out_dir)/<(subdir)/qrc_<(qrc_base_name).cc',
        '-name', 'qrc_<(qrc_base_name)',
        '<(subdir)/<(qrc_base_name).qrc',
      ],
      'message': 'Generating Resource file from <(qrc_base_name).qrc',
    },
  ],

  }]],  # End of use_qt=="YES"
}
