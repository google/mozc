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

# Generates *.java files from *.proto files.
#
# Q : Why genproto.gypi cannot be used ?
# A : --java_out option of protoc command needs .proto file to have all type
# names and *file name* uniquely or add special description inside .proto file.
# For exmaple, 'Command' type inside 'Command.proto' is forbidden.
# Thus if genproto.gypi is modified to have --java_out option,
# *all* .proto files have to be changed its name
# or be added special description.
#
# Q : Why rule based description like genproto.gypi cannot be used ?
# A : protoc command outputs generated .java files into an appropriate path
# based on its package name.
# The destination path cannot be detected at 'rules' section
# because the package name is described only in the .proto file
# so that 'outputs' section cannot be filled.
#
# We can retrieve Java package names from .proto files and automate the code
# generation.  However, we need only a few proto files so far and it wouldn't
# be worth doing for now.
{
  'variables': {
    'relative_dir': 'android',
    # Android Development Tools
    'adt_gen_dir': 'gen_for_adt',
    'adt_protobuf_gen_dir': 'protobuf/gen_for_adt',
    # Android SDK
    'sdk_gen_dir': 'gen',
    'sdk_protobuf_gen_dir': 'protobuf/gen',
  },
  'conditions': [
    ['OS!="linux"', {
      'variables': {
        'protoc_command%': '<(relative_dir)/<(mozc_build_tools_dir)/protoc<(EXECUTABLE_SUFFIX)',
      },
    }, {
      'conditions': [
        ['use_libprotobuf==0', {
          'variables': {
            'protoc_command%': '<(relative_dir)/<(mozc_build_tools_dir)/protoc<(EXECUTABLE_SUFFIX)',
          },
        }, {
          'variables': {
            'protoc_command%': 'protoc<(EXECUTABLE_SUFFIX)',
          },
        }],
      ],
    }],
  ],
  'targets': [
    {
      'target_name': 'sdk_genproto_java',
      'type': 'none',
      'dependencies': [
        'sdk_genproto_java_config',
        'sdk_genproto_java_descriptor',
        'sdk_genproto_java_user_dictionary_storage',
        'sdk_genproto_java_session',
      ],
    },
    {
      'target_name': 'adt_genproto_java',
      'type': 'none',
      'dependencies': [
        'adt_genproto_java_config',
        'adt_genproto_java_descriptor',
        'adt_genproto_java_user_dictionary_storage',
        'adt_genproto_java_session',
      ],
    },
    {
      'target_name': 'sdk_genproto_java_descriptor',
      'type': 'none',
      'copies': [{
        'destination': '<(sdk_protobuf_gen_dir)/com/google/protobuf/',
        'files': [
          '<(adt_protobuf_gen_dir)/com/google/protobuf/DescriptorProtos.java',
        ],
      }],
    },
    {
      'target_name': 'adt_genproto_java_descriptor',
      'type': 'none',
      'actions': [
        {
          'action_name': 'genproto_descriptor',
          'inputs': [
            '../protobuf/files/src/google/protobuf/descriptor.proto',
          ],
          'outputs': [
            '<(adt_protobuf_gen_dir)/com/google/protobuf/DescriptorProtos.java',
          ],
          'action': [
            'python', '../build_tools/run_after_chdir.py',
            '<(DEPTH)',
            '<(protoc_command)',
            '--java_out=<(relative_dir)/<(adt_protobuf_gen_dir)',
            # Make a list of inputs replacing /^\.\.\// with ''.
            '<!@(<(python_executable)'
            ' -c "import re, sys;'
            ' print \\" \\".join('
              '[re.sub(r\\"^\\\\.\\\\./\\", \\"\\", i)'
              ' for i in sys.argv[1:]])"'
            ' <(_inputs))',
          ],
        },
      ],
    },
    {
      'target_name': 'sdk_genproto_java_config',
      'type': 'none',
      'copies': [{
        'destination': '<(sdk_gen_dir)/org/mozc/android/inputmethod/japanese/protobuf/',
        'files': [
          '<(adt_gen_dir)/org/mozc/android/inputmethod/japanese/protobuf/ProtoConfig.java',
        ],
      }],
    },
    {
      'target_name': 'adt_genproto_java_config',
      'type': 'none',
      'actions': [
        {
          'action_name': 'genproto_config',
          'inputs': [
            '../config/config.proto',
          ],
          'outputs': [
            '<(adt_gen_dir)/org/mozc/android/inputmethod/japanese/protobuf/ProtoConfig.java',
          ],
          'action': [
            'python', '../build_tools/run_after_chdir.py',
            '<(DEPTH)',
            '<(protoc_command)',
            '--java_out=<(relative_dir)/<(adt_gen_dir)',
            # Make a list of inputs replacing /^\.\.\// with ''.
            '<!@(<(python_executable)'
            ' -c "import re, sys;'
            ' print \\" \\".join('
              '[re.sub(r\\"^\\\\.\\\\./\\", \\"\\", i)'
              ' for i in sys.argv[1:]])"'
            ' <(_inputs))',
          ],
        },
      ],
    },
    {
      'target_name': 'sdk_genproto_java_user_dictionary_storage',
      'type': 'none',
      'copies': [{
        'destination': '<(sdk_gen_dir)/org/mozc/android/inputmethod/japanese/protobuf/',
        'files': [
          '<(adt_gen_dir)/org/mozc/android/inputmethod/japanese/protobuf/ProtoUserDictionaryStorage.java',
        ],
      }],
    },
    {
      'target_name': 'adt_genproto_java_user_dictionary_storage',
      'type': 'none',
      'actions': [
        {
          'action_name': 'genproto_user_dictionary_storage',
          'inputs': [
            '../dictionary/user_dictionary_storage.proto',
          ],
          'outputs': [
            '<(adt_gen_dir)/org/mozc/android/inputmethod/japanese/protobuf/ProtoUserDictionaryStorage.java',
          ],
          'action': [
            'python', '../build_tools/run_after_chdir.py',
            '<(DEPTH)',
            '<(protoc_command)',
            '--java_out=<(relative_dir)/<(adt_gen_dir)',
            # Make a list of inputs replacing /^\.\.\// with ''.
            '<!@(<(python_executable)'
            ' -c "import re, sys;'
            ' print \\" \\".join('
              '[re.sub(r\\"^\\\\.\\\\./\\", \\"\\", i)'
              ' for i in sys.argv[1:]])"'
            ' <(_inputs))',
          ],
        },
      ],
    },
    {
      'target_name': 'sdk_genproto_java_session',
      'type': 'none',
      'copies': [{
        'destination': '<(sdk_gen_dir)/org/mozc/android/inputmethod/japanese/protobuf/',
        'files': [
          '<(adt_gen_dir)/org/mozc/android/inputmethod/japanese/protobuf/ProtoCandidates.java',
          '<(adt_gen_dir)/org/mozc/android/inputmethod/japanese/protobuf/ProtoCommands.java',
        ],
      }],
    },
    {
      'target_name': 'adt_genproto_java_session',
      'type': 'none',
      'actions': [
        {
          'action_name': 'genproto_session',
          'inputs': [
            '../session/candidates.proto',
            '../session/commands.proto',
          ],
          'outputs': [
            '<(adt_gen_dir)/org/mozc/android/inputmethod/japanese/protobuf/ProtoCandidates.java',
            '<(adt_gen_dir)/org/mozc/android/inputmethod/japanese/protobuf/ProtoCommands.java',
          ],
          'action': [
            'python', '../build_tools/run_after_chdir.py',
            '<(DEPTH)',
            '<(protoc_command)',
            '--java_out=<(relative_dir)/<(adt_gen_dir)',
            # Make a list of inputs replacing /^\.\.\// with ''.
            '<!@(<(python_executable)'
            ' -c "import re, sys;'
            ' print \\" \\".join('
              '[re.sub(r\\"^\\\\.\\\\./\\", \\"\\", i)'
              ' for i in sys.argv[1:]])"'
            ' <(_inputs))',
          ],
        },
      ],
    },
  ],
}
