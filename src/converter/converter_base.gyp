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

# converter_base.gyp defines targets for lower layers to link to the converter
# modules, so modules in lower layers do not depend on ones in higher layers,
# avoiding circular dependencies.
{
  'variables': {
    'relative_dir': 'converter',
    'relative_mozc_dir': '',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
    'gen_out_mozc_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_mozc_dir)',
  },
  'targets': [
    {
      'target_name': 'segmenter',
      'type': 'static_library',
      'sources': [
        'segmenter.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
    },
    {
      'target_name': 'connector',
      'type': 'static_library',
      'sources': [
        'connector.cc',
      ],
      'dependencies': [
        '../base/absl.gyp:absl_status',
        '../base/base.gyp:base',
        '../storage/louds/louds.gyp:simple_succinct_bit_vector_index',
      ],
    },
    {
      'target_name': 'lattice',
      'type': 'static_library',
      'sources': [
        'lattice.cc',
        'node_allocator.h',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
    },
    {
      'target_name': 'segments',
      'type': 'static_library',
      'sources': [
        '<(gen_out_mozc_dir)/dictionary/pos_matcher.h',
        'candidate_filter.cc',
        'nbest_generator.cc',
        'segments.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../dictionary/dictionary_base.gyp:pos_matcher',
        '../prediction/prediction_base.gyp:suggestion_filter',
        '../transliteration/transliteration.gyp:transliteration',
        'connector',
        'lattice',
        'segmenter',
      ],
    },
    {
      'target_name': 'converter_util',
      'type': 'static_library',
      'sources': [
        'converter_util.cc',
      ],
      'dependencies': [
        'segments',
      ],
    },
    {
      'target_name': 'immutable_converter_interface',
      'type': 'static_library',
      'sources': [
        'immutable_converter_interface.cc',
      ],
      'dependencies': [
        '../request/request.gyp:conversion_request',
      ],
    },
    {
      'target_name': 'immutable_converter',
      'type': 'static_library',
      'sources': [
        'immutable_converter.cc',
        'key_corrector.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../config/config.gyp:config_handler',
        '../dictionary/dictionary.gyp:suffix_dictionary',
        '../dictionary/dictionary_base.gyp:pos_matcher',
        '../dictionary/dictionary_base.gyp:suppression_dictionary',
        '../protocol/protocol.gyp:commands_proto',
        '../protocol/protocol.gyp:config_proto',
        '../rewriter/rewriter_base.gyp:gen_rewriter_files#host',
        'connector',
        'immutable_converter_interface',
        'segmenter',
        'segments',
      ],
    },
    {
      'target_name': 'converter_mock',
      'type': 'static_library',
      'sources': [
        'converter_mock.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../protocol/protocol.gyp:commands_proto',
        '../request/request.gyp:conversion_request',
        'segments',
      ],
    },
    {
      'target_name': 'gen_segmenter_bitarray',
      'type': 'static_library',
      'toolsets': ['host'],
      'sources': [
        'gen_segmenter_bitarray.cc',
      ],
      'dependencies' : [
        '../base/base.gyp:base',
        '../protocol/protocol.gyp:segmenter_data_proto',
      ]
    },
    {
      'target_name': 'pos_id_printer',
      'type': 'static_library',
      'sources': [
        'pos_id_printer.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
    },
  ],
}
