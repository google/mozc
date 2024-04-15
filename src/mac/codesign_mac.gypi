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

{
  'type': 'none',
  'actions': [
    {
      'action_name': 'code_signing',
      'inputs': [
        # Though Ninja requires the rule to generate files in inputs,
        # it does not catch files under directories to be copied.
        # (See: mac/mac.gyp > GoogleJapaneseInput > copies)
        # The dependencies can be managed by the 'dependencies'
        # variable in any case.
        # TODO(komatsu): Enable to handle dependencies with Ninja.
        # '<(app_path)/Contents/MacOS/<(package_name)',
      ],
      'outputs': [
        '<(app_path)/Contents/_CodeSignature/CodeResources',
      ],
      'action': [
        '<(python)', '<(mozc_oss_src_dir)/build_tools/codesign_mac.py',
        '--target', '<(app_path)',
        # This keychain is for testing. In the release build environment,
        # the keychain is replaced with the official keychain in the script.
        '--keychain', '<(DEPTH)/mac/MacSigning.keychain',
      ]
    },
  ],
}
