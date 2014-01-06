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

# This file provides a common rule for Qt moc command.
{
  'conditions': [['use_qt=="YES"', {

  'variables': {
    'includes': ['qt_vars.gypi'],
    'conditions': [
      ['qt_dir', {
        'moc_path': '<(qt_dir)/bin/moc<(EXECUTABLE_SUFFIX)',
      }, {
        'conditions': [
          ['pkg_config_command', {
            'moc_path': '<!(<(pkg_config_command) --variable=moc_location QtGui)',
          }, {
            'moc_path': '<(qt_dir_env)/bin/moc<(EXECUTABLE_SUFFIX)',
          }],
        ],
      }],
    ],
  },
  'rules': [
    {
      'rule_name': 'qtmoc',
      'extension': 'h',
      'outputs': [
        '<(gen_out_dir)/<(subdir)/moc_<(RULE_INPUT_ROOT).cc',
      ],
      'action': [
        '<(moc_path)',
        # Specify include path as a workaround of b/4905149
        '-p', '<(relative_dir)/<(subdir)',
        '-o', '<(gen_out_dir)/<(subdir)/moc_<(RULE_INPUT_ROOT).cc',
        '<(RULE_INPUT_PATH)',
      ],
      'message': 'Generatinc moc files from <(RULE_INPUT_PATH)',
    },
  ],

  }]],  # End of use_qt=="YES"
}
