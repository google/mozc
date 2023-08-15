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

# This file provides a common rule for Qt uic command.
{
  'conditions': [['use_qt=="YES"', {

  'includes': [
    'qt_tool_dir.gypi',
  ],
  'variables': {
    'uic_path': '<(qt_tool_dir)/uic<(EXECUTABLE_SUFFIX)',
  },
  'rules': [
    {
      'rule_name': 'qtui',
      'extension': 'ui',
      'outputs': [
        '<(gen_out_dir)/<(subdir)/ui_<(RULE_INPUT_ROOT).h'
      ],
      'action': [
        '<(uic_path)',
        '-o', '<(gen_out_dir)/<(subdir)/ui_<(RULE_INPUT_ROOT).h',
        '<(RULE_INPUT_PATH)'
      ],
      'message': 'Generating UI header files from <(RULE_INPUT_PATH)',
    },
  ],

  }]],  # End of use_qt=="YES"
}
