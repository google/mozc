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
  'targets': [
    # Implementation of a Trie data structure based on LOUDS and its builder.
    {
      'target_name': 'louds',
      'type': 'static_library',
      'toolsets': ['target', 'host'],
      'sources': [
        'louds.cc',
      ],
      'dependencies': [
        '<(mozc_src_dir)/base/base.gyp:base',
        'simple_succinct_bit_vector_index',
      ],
    },
    {
      'target_name': 'louds_trie',
      'type': 'static_library',
      'toolsets': ['target', 'host'],
      'sources': [
        'louds_trie.cc',
      ],
      'dependencies': [
        '<(mozc_src_dir)/base/absl.gyp:absl_strings',
        '<(mozc_src_dir)/base/base.gyp:base',
        'bit_stream',
        'louds',
        'simple_succinct_bit_vector_index',
      ],
    },
    {
      'target_name': 'louds_trie_builder',
      'type': 'static_library',
      'toolsets': ['target', 'host'],
      'sources': [
        'louds_trie_builder.cc',
      ],
      'dependencies': [
        '<(mozc_src_dir)/base/base.gyp:base',
        'bit_stream',
      ],
    },
    # Implementation of an array of string based on bit vector.
    {
      'target_name': 'bit_vector_based_array',
      'type': 'static_library',
      'toolsets': ['target', 'host'],
      'sources': [
        'bit_vector_based_array.cc',
      ],
      'dependencies': [
        '<(mozc_src_dir)/base/base.gyp:base',
        'bit_stream',
        'simple_succinct_bit_vector_index',
      ],
    },
    {
      'target_name': 'bit_vector_based_array_builder',
      'type': 'static_library',
      'toolsets': ['target', 'host'],
      'sources': [
        'bit_vector_based_array_builder.cc',
      ],
      'dependencies': [
        '<(mozc_src_dir)/base/base.gyp:base',
        'bit_stream',
      ],
    },
    # Implemantation of the succinct bit vector.
    {
      'target_name': 'simple_succinct_bit_vector_index',
      'type': 'static_library',
      'toolsets': ['target', 'host'],
      'sources': [
        'simple_succinct_bit_vector_index.cc',
      ],
      'dependencies': [
        '<(mozc_src_dir)/base/base.gyp:base',
      ],
    },
    # Bit stream implementation for builders.
    {
      'target_name': 'bit_stream',
      'type': 'static_library',
      'toolsets': ['target', 'host'],
      'sources': [
        'bit_stream.cc',
      ],
      'dependencies': [
        '<(mozc_src_dir)/base/base.gyp:base',
      ],
    },
  ],
}
