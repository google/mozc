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
        '<(mozc_oss_src_dir)/base/base.gyp:base',
      ],
    },
    {
      'target_name': 'connector',
      'type': 'static_library',
      'sources': [
        'connector.cc',
      ],
      'dependencies': [
        '<(mozc_oss_src_dir)/base/absl.gyp:absl_status',
        '<(mozc_oss_src_dir)/base/base.gyp:base',
        '<(mozc_oss_src_dir)/storage/louds/louds.gyp:simple_succinct_bit_vector_index',
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
        '<(mozc_oss_src_dir)/base/base.gyp:base',
      ],
    },
    {
      'target_name': 'segments',
      'type': 'static_library',
      'sources': [
        '<(gen_out_mozc_dir)/dictionary/pos_matcher_impl.inc',
        'candidate.cc',
        'candidate_filter.cc',
        'nbest_generator.cc',
        'segments.cc',
      ],
      'dependencies': [
        '<(mozc_oss_src_dir)/base/absl.gyp:absl_log',
        '<(mozc_oss_src_dir)/base/absl.gyp:absl_strings',
        '<(mozc_oss_src_dir)/base/base.gyp:base',
        '<(mozc_oss_src_dir)/dictionary/dictionary_base.gyp:pos_matcher',
        '<(mozc_oss_src_dir)/prediction/prediction_base.gyp:suggestion_filter',
        '<(mozc_oss_src_dir)/protocol/protocol.gyp:commands_proto',
        '<(mozc_oss_src_dir)/transliteration/transliteration.gyp:transliteration',
        'connector',
        'lattice',
        'segmenter',
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
        '<(mozc_oss_src_dir)/base/base.gyp:base',
        '<(mozc_oss_src_dir)/protocol/protocol.gyp:segmenter_data_proto',
      ]
    },
    {
      'target_name': 'pos_id_printer',
      'type': 'static_library',
      'sources': [
        'pos_id_printer.cc',
      ],
      'dependencies': [
        '<(mozc_oss_src_dir)/base/absl.gyp:absl_strings',
        '<(mozc_oss_src_dir)/base/base.gyp:base',
        '<(mozc_oss_src_dir)/base/base.gyp:number_util',
      ],
    },
  ],
}
