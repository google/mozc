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
    # Implementation of a Trie data structure based on LOUDS and its builder.
    {
      'target_name': 'louds_trie_adapter',
      'type': 'static_library',
      'toolsets': ['target', 'host'],
      'sources': [
        'louds_trie_adapter.cc',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../storage/louds/louds.gyp:key_expansion_table',
        '../../storage/louds/louds.gyp:louds_trie',
      ],
    },
    {
      'target_name': 'louds_trie_adapter_test',
      'type': 'executable',
      'sources': [
        'louds_trie_adapter_test.cc',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../storage/louds/louds.gyp:louds_trie_builder',
        '../../testing/testing.gyp:gtest_main',
        'louds_trie_adapter',
      ],
      'variables': {
        'test_size': 'small',
      },
    },

    # Test cases meta target: this target is referred from gyp/tests.gyp
    {
      'target_name': 'dictionary_louds_all_test',
      'type': 'none',
      'dependencies': [
        'louds_trie_adapter_test',
      ],
    },
  ],
}
