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
  'variables': {
    'relative_dir': 'chrome/nacl',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
    'configuration_nacl_i686': '<(CONFIGURATION_NAME)_NaCl_i686',
    'configuration_nacl_x86_64': '<(CONFIGURATION_NAME)_NaCl_x86-64',
    'dummy_input_file': 'nacl_extension.gyp',
    # GYP's 'copies' rule cannot change the destination file name, so we use
    # our own script to copy files.
    'copy_file': ['python', '../../build_tools/copy_file.py'],
    'product_nacl_i686_dir': '<(DEPTH)/$(builddir_name)/<(configuration_nacl_i686)',
    'product_nacl_x86_64_dir': '<(DEPTH)/$(builddir_name)/<(configuration_nacl_x86_64)',
  },
  'targets': [
    {
      'target_name': 'copy_nacl_session_nexe',
      'type': 'none',
      'actions': [
        {
          'action_name': 'copy_nacl_session_x86_32',
          'inputs': ['<(gen_out_dir)/build_nacl_session_nexe_done'],
          'outputs': ['<(gen_out_dir)/nacl/nacl_session_x86_32.nexe'],
          'action': [
            '<@(copy_file)',
            '--reference=<(gen_out_dir)/build_nacl_session_nexe_done',
            '<(product_nacl_i686_dir)/nacl_session.nexe',
            '<(gen_out_dir)/nacl/nacl_session_x86_32.nexe',
          ],
        },
        {
          'action_name': 'copy_nacl_session_x86_64',
          'inputs': ['<(gen_out_dir)/build_nacl_session_nexe_done'],
          'outputs': ['<(gen_out_dir)/nacl/nacl_session_x86_64.nexe'],
          'action': [
            '<@(copy_file)',
            '--reference=<(gen_out_dir)/build_nacl_session_nexe_done',
            '<(product_nacl_x86_64_dir)/nacl_session.nexe',
            '<(gen_out_dir)/nacl/nacl_session_x86_64.nexe',
          ],
        },
      ],
      'dependencies': [
        'build_nacl_session_nexe',
      ],
    },
    {
      'target_name': 'build_nacl_session_nexe',
      'type': 'none',
      'actions': [
        {
          'action_name': 'build_nacl_session_nexe',
          'inputs': ['<(dummy_input_file)'],
          'outputs': ['dummy_build_nacl_session_nexe',
                      '<(gen_out_dir)/build_nacl_session_nexe_done'],
          'action': [
            'python', '../build_for_nacl.py',
            '--build_base=<(build_base)',
            '--depth=<(DEPTH)',
            '--nacl_sdk_root=<(nacl_sdk_root)',
            '--target_settings='
            '[{"configuration": "<(configuration_nacl_i686)",'
            '  "toolchain_dir": "linux_x86_newlib",'
            '  "toolchain_prefix": "i686-nacl-"},'
            ' {"configuration": "<(configuration_nacl_x86_64)",'
            '  "toolchain_dir": "linux_x86_newlib",'
            '  "toolchain_prefix": "x86_64-nacl-"}]',
            '--touch_when_done=<(gen_out_dir)/build_nacl_session_nexe_done',
            'chrome/nacl/nacl_session.gyp:nacl_session.nexe',
          ],
        },
      ],
    },
  ],
}
