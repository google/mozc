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

# This file provides a common rule for genproto command.
{
  'variables': {
    'wrapper_path': '<(DEPTH)/build_tools/protoc_wrapper.py',
    'protoc_command': 'protoc<(EXECUTABLE_SUFFIX)',
    'conditions': [
      ['use_libprotobuf==1', {
        'protoc_wrapper_additional_options': [],
        'additional_inputs': [],
      }, {  # else
        'protoc_wrapper_additional_options': ['--protoc_dir=<(PRODUCT_DIR)'],
        'additional_inputs': ['<(PRODUCT_DIR)/protoc<(EXECUTABLE_SUFFIX)'],
      }],
    ],
  },
  'conditions': [
    ['use_libprotobuf==0', {
      'dependencies': [
        '<(DEPTH)/protobuf/protobuf.gyp:protoc#host',
      ],
    }],
  ],
  'rules': [
    {
      'rule_name': 'genproto',
      'extension': 'proto',
      'inputs': [
        '<(wrapper_path)',
        '<@(additional_inputs)',
      ],
      'outputs': [
        '<(proto_out_dir)/<(relative_dir)/<(RULE_INPUT_ROOT).pb.h',
        '<(proto_out_dir)/<(relative_dir)/<(RULE_INPUT_ROOT).pb.cc',
      ],
      'action': [
        '<(python)', '<(wrapper_path)',
        '--project_root=<(DEPTH)',
        '--protoc_command=<(protoc_command)',
        '--proto=<(RULE_INPUT_PATH)',
        '--cpp_out=<(proto_out_dir)',
        '<@(protoc_wrapper_additional_options)',
      ],
      'message': 'Generating C++ code from <(RULE_INPUT_PATH)',
    },
  ],
}
