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
  'targets': [
    {
      'target_name': 'key_expansion_table_test',
      'type': 'executable',
      'sources': [
        'key_expansion_table_test.cc',
      ],
      'dependencies': [
        '../../testing/testing.gyp:gtest_main',
        'louds.gyp:key_expansion_table',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'louds_trie_test',
      'type': 'executable',
      'sources': [
        'louds_trie_test.cc',
      ],
      'dependencies': [
        '../../testing/testing.gyp:gtest_main',
        'louds.gyp:louds_trie',
        'louds.gyp:louds_trie_builder',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'bit_vector_based_array_test',
      'type': 'executable',
      'sources': [
        'bit_vector_based_array_test.cc',
      ],
      'dependencies': [
        '../../testing/testing.gyp:gtest_main',
        'louds.gyp:bit_vector_based_array',
        'louds.gyp:bit_vector_based_array_builder',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'simple_succinct_bit_vector_index_test',
      'type': 'executable',
      'sources': [
        'simple_succinct_bit_vector_index_test.cc',
      ],
      'dependencies': [
        '../../testing/testing.gyp:gtest_main',
        'louds.gyp:simple_succinct_bit_vector_index',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'bit_stream_test',
      'type': 'executable',
      'sources': [
        'bit_stream_test.cc',
      ],
      'dependencies': [
        '../../testing/testing.gyp:gtest_main',
        'louds.gyp:bit_stream',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    # Test cases meta target: this target is referred from gyp/tests.gyp
    {
      'target_name': 'storage_louds_all_test',
      'type': 'none',
      'dependencies': [
        'bit_stream_test',
        'bit_vector_based_array_test',
        'key_expansion_table_test',
        'louds_trie_test',
        'simple_succinct_bit_vector_index_test',
      ],
    },
  ],
}
