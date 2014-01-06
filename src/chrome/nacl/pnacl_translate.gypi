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

{
  'type': 'none',
  'rules': [
    {
      'rule_name': 'translate',
      'extension': '',
      'inputs': [
        'pnacl_translate.py',
      ],
      'outputs': [
        '<(PRODUCT_DIR)/<(RULE_INPUT_ROOT)_arm.nexe',
        '<(PRODUCT_DIR)/<(RULE_INPUT_ROOT)_x86_32.nexe',
        '<(PRODUCT_DIR)/<(RULE_INPUT_ROOT)_x86_64.nexe',
      ],
      'action': [
        'python', 'pnacl_translate.py',
        '--toolchain_root=<(nacl_sdk_root)/toolchain/linux_x86_pnacl',
        '--input=<(RULE_INPUT_PATH)',
        '--output_base=<(PRODUCT_DIR)/<(RULE_INPUT_ROOT)',
        # Strips the binaries in Release build.
        '$(if $(filter Release,<(CONFIGURATION_NAME)),--strip,)',
      ],
    },
  ],
}