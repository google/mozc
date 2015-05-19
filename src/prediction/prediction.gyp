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
  'variables': {
    'relative_dir': 'prediction',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  'targets': [
    {
      'target_name': 'prediction',
      'type': 'static_library',
      'sources': [
        'dictionary_predictor.cc',
        'predictor.cc',
        'user_history_predictor.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:config_file_stream',
        '../config/config.gyp:config_handler',
        '../composer/composer.gyp:composer',
        '../converter/converter_base.gyp:conversion_request',
        '../converter/converter_base.gyp:immutable_converter',
        '../converter/converter_base.gyp:segments',
        '../dictionary/dictionary.gyp:dictionary',
        '../dictionary/dictionary.gyp:suffix_dictionary',
        '../dictionary/dictionary_base.gyp:suppression_dictionary',
        '../rewriter/rewriter.gyp:rewriter',
        '../session/session_base.gyp:request_test_util',
        '../session/session_base.gyp:session_protocol',
        '../storage/storage.gyp:encrypted_string_storage',
        '../storage/storage.gyp:storage',
        '../usage_stats/usage_stats_base.gyp:usage_stats',
        'gen_zero_query_number_data#host',
        'prediction_base.gyp:suggestion_filter',
        'prediction_protocol',
      ],
    },
    {
      'target_name': 'gen_zero_query_number_data',
      'type': 'none',
      'toolsets': ['host'],
      'actions': [
        {
          'action_name': 'gen_zero_query_number_data',
          'variables': {
            'input_files': [
              '../data/zero_query/zero_query_number.def',
            ],
          },
          'inputs': [
            'gen_zero_query_number_data.py',
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/zero_query_number_data.h',
          ],
          'action': [
            'python', '../build_tools/redirect.py',
            '<(gen_out_dir)/zero_query_number_data.h',
            'gen_zero_query_number_data.py',
            '<@(input_files)',
          ],
          'message': 'Generating <(gen_out_dir)/zero_query_number_data.h',
        },
      ],
    },
    {
      'target_name': 'genproto_prediction',
      'type': 'none',
      'toolsets': ['host'],
      'sources': [
        'user_history_predictor.proto',
      ],
      'includes': [
        '../protobuf/genproto.gypi',
      ],
    },
    {
      'target_name': 'prediction_protocol',
      'type': 'static_library',
      'hard_dependency': 1,
      'sources': [
        '<(proto_out_dir)/<(relative_dir)/user_history_predictor.pb.cc',
      ],
      'dependencies': [
        '../protobuf/protobuf.gyp:protobuf',
        'genproto_prediction#host',
      ],
      'export_dependent_settings': [
        'genproto_prediction#host',
      ],
    },
  ],
}
