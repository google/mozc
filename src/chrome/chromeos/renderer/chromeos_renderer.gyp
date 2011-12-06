# Copyright 2010-2011, Google Inc.
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

# The ChromeOS renderer uses candidates.proto and which should be "lite"
# version definition. However in Mozc, we can not use "lite" version because of
# API restriction. So following rule makes the candidates.proto "lite" before
# generate codes. And this file is used by Chrome.
{
  'variables': {
    'protoc_out_dir': '<(SHARED_INTERMEDIATE_DIR)/protoc_out',
    'third_party_dir': '../../../../../third_party',
  },
  'targets': [
    {
      'target_name': 'litify_mozc_proto_files',
      'type': 'none',
      'actions': [
        {
          'action_name': 'litify_candidats_proto',
          'inputs': [
            '<(third_party_dir)/mozc/session/candidates.proto',
          ],
          'outputs': [
            '<(third_party_dir)/mozc/session/candidates_lite.proto',
          ],
          'action': [
            'python',
            'litify_proto_file.py',
            '--in_file_path',
            '<(third_party_dir)/mozc/session/candidates.proto',
            '--out_file_path',
            '<(third_party_dir)/mozc/session/candidates_lite.proto',
          ],
        },
      ]
    },
    {
      # Protobuf compiler / generator for the mozc inputmethod commands
      # protocol buffer.
      'target_name': 'gen_candidates_proto',
      'type': 'none',
      'sources': [
        '<(third_party_dir)/mozc/session/candidates_lite.proto',
      ],
      'rules': [
        {
          'rule_name': 'genproto',
          'extension': 'proto',
          'inputs': [
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
          ],
          'outputs': [
            '<(protoc_out_dir)/third_party/mozc/session/<(RULE_INPUT_ROOT).pb.h',
            '<(protoc_out_dir)/third_party/mozc/session/<(RULE_INPUT_ROOT).pb.cc',
          ],
          'action': [
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
            '--proto_path=<(third_party_dir)/mozc',
            '<(third_party_dir)/mozc/session/<(RULE_INPUT_ROOT)<(RULE_INPUT_EXT)',
            '--cpp_out=<(protoc_out_dir)/third_party/mozc',
          ],
          'message': 'Generating C++ code from <(RULE_INPUT_PATH)',
          'process_outputs_as_sources': 1,
        },
      ],
      'dependencies': [
        'litify_mozc_proto_files',
        '<(third_party_dir)/protobuf/protobuf.gyp:protobuf_lite',
        '<(third_party_dir)/protobuf/protobuf.gyp:protoc#host',
      ],
      'include_dirs': [
        '<(protoc_out_dir)/third_party/mozc',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(protoc_out_dir)/third_party/mozc',
        ]
      },
      'export_dependent_settings': [
        '<(third_party_dir)/protobuf/protobuf.gyp:protobuf_lite',
      ],
    },
    {
      'target_name': 'mozc_candidates_proto',
      'type': 'static_library',
      'sources': [
        '<(protoc_out_dir)/third_party/mozc/session/candidates_lite.pb.cc',
      ],
      'dependencies': [
        'gen_candidates_proto',
      ],
    },
  ]
}
