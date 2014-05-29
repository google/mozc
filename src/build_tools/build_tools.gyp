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

# The gyp file is used for pre-building code generation tools.
#
# In particualr, generating embedded_dictionary_data.h (140+ MB) with
# converter_compiler_main is slow (can take several minutes).
#
# If we simply add a dependency from embedded_dictionary_data.h to
# converter_compiler_main binary, we end up re-generating the header file
# every time converter_compiler_main binary is rebuilt. This can happen by
# a change as simple as fixing a comment in base/util.h, as
# converter_compiler_main depends on base/util.h.
#
# To solve the problem, we pre-build converter_compiler_main and use the
# pre-built version in mozc_tools directory for code generation.
#
# In order to build products for Chrome OS, we also pre-build all the other code
# generation tools. They need to be built for the host machine rather than the
# target machine.

{
  'variables': {
    'relative_dir': 'build_tools',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  'targets': [
    {
      'target_name': 'build_tools',
      'type': 'none',
      'toolsets': ['host'],
      'conditions': [
        ['enable_two_pass_build==1', {
          'dependencies': [
            '../protobuf/protobuf.gyp:install_protoc',
            '../dictionary/dictionary.gyp:install_gen_system_dictionary_data_main',
          ],
        }, {  # else
          'actions': [
            {
              'action_name': 'show_message',
              'inputs': [
                'build_tools.gyp',
              ],
              'outputs': [
                '<(gen_out_dir)/dummy_output_file',
              ],
              'action': [
                'python',
                '-c',
                '"build_tools is deprecated!"',
              ],
              'message': 'build_tools is deprecated!',
            },
          ],
        }],
      ],
    },
  ],
}
