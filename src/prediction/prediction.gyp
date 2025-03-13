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
  'variables': {
    'relative_dir': 'prediction',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  'targets': [
    {
      'target_name': 'prediction',
      'type': 'static_library',
      'hard_dependency': 1,
      'sources': [
        'dictionary_predictor.cc',
        'dictionary_prediction_aggregator.cc',
        'number_decoder.cc',
        'predictor.cc',
        'result.cc',
        'single_kanji_prediction_aggregator.cc',
        'user_history_predictor.cc',
      ],
      'dependencies': [
        '<(mozc_oss_src_dir)/base/absl.gyp:absl_strings',
        '<(mozc_oss_src_dir)/base/base.gyp:base',
        '<(mozc_oss_src_dir)/base/base.gyp:config_file_stream',
        '<(mozc_oss_src_dir)/base/base.gyp:number_util',
        '<(mozc_oss_src_dir)/composer/composer.gyp:composer',
        '<(mozc_oss_src_dir)/config/config.gyp:config_handler',
        '<(mozc_oss_src_dir)/converter/converter_base.gyp:segmenter',
        '<(mozc_oss_src_dir)/converter/converter_base.gyp:segments',
        '<(mozc_oss_src_dir)/converter/immutable_converter.gyp:immutable_converter',
        '<(mozc_oss_src_dir)/dictionary/dictionary.gyp:dictionary',
        '<(mozc_oss_src_dir)/dictionary/dictionary.gyp:suffix_dictionary',
        '<(mozc_oss_src_dir)/engine/engine_base.gyp:modules',
        '<(mozc_oss_src_dir)/protocol/protocol.gyp:commands_proto',
        '<(mozc_oss_src_dir)/request/request.gyp:conversion_request',
        '<(mozc_oss_src_dir)/rewriter/rewriter.gyp:rewriter',
        '<(mozc_oss_src_dir)/storage/storage.gyp:storage',
        'prediction_base.gyp:suggestion_filter',
        'prediction_protocol',
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
        '<(mozc_oss_src_dir)/protobuf/protobuf.gyp:protobuf',
        'genproto_prediction#host',
      ],
      'export_dependent_settings': [
        'genproto_prediction#host',
      ],
    },
  ],
}
