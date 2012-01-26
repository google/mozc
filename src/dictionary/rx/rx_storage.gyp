# Copyright 2010-2012, Google Inc.
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
      'target_name': 'rx_trie',
      'type': 'static_library',
      'sources': [
        'rx_trie.cc',
      ],
      'dependencies': [
        '../../base/base.gyp:base_core',
        '<(DEPTH)/third_party/rx/rx.gyp:rx',
      ],
    },
    {
      'target_name': 'rx_trie_builder',
      'type': 'static_library',
      'sources': [
        'rx_trie_builder.cc'
      ],
      'dependencies': [
        '../../base/base.gyp:base_core',
        '<(DEPTH)/third_party/rx/rx.gyp:rx',
      ],
    },
    {
      'target_name': 'rbx_array',
      'type': 'static_library',
      'sources': [
        'rbx_array.cc',
      ],
      'dependencies': [
        '../../base/base.gyp:base_core',
        '<(DEPTH)/third_party/rx/rx.gyp:rx',
      ],
    },
    {
      'target_name': 'rbx_array_builder',
      'type': 'static_library',
      'sources': [
        'rbx_array_builder.cc'
      ],
      'dependencies': [
        '../../base/base.gyp:base_core',
        '<(DEPTH)/third_party/rx/rx.gyp:rx',
      ],
    },
    {
      'target_name': 'rx_trie_test',
      'type': 'executable',
      'sources': [
        'rx_trie_test.cc',
      ],
      'dependencies': [
        '../../testing/testing.gyp:gtest_main',
        'rx_trie',
        'rx_trie_builder',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'rx_trie_builder_test',
      'type': 'executable',
      'sources': [
        'rx_trie_builder_test.cc',
      ],
      'dependencies': [
        '../../testing/testing.gyp:gtest_main',
        'rx_trie_builder',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'rbx_array_test',
      'type': 'executable',
      'sources': [
        'rbx_array_test.cc',
      ],
      'dependencies': [
        '../../testing/testing.gyp:gtest_main',
        'rbx_array',
        'rbx_array_builder',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    # Test cases meta target: this target is referred from gyp/tests.gyp
    {
      'target_name': 'rx_all_test',
      'type': 'none',
      'dependencies': [
        'rbx_array_test',
        'rx_trie_builder_test',
        'rx_trie_test',
      ],
    },
  ],
}
