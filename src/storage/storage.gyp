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
      'target_name': 'storage',
      'type': 'static_library',
      'toolsets': ['target', 'host'],
      'sources': [
        'existence_filter.cc',
        'lru_storage.cc',
        'registry.cc',
        'sparse_array_image.cc',
        'tiny_storage.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'conditions': [
        ['target_platform=="NaCl" and _toolset=="target"', {
          'sources!': [
            'registry.cc',
            'tiny_storage.cc',
          ],
        }],
      ],
    },
    {
      'target_name': 'storage_test',
      'type': 'executable',
      'sources': [
        'existence_filter_test.cc',
        'sparse_array_image_test.cc',
        'tiny_storage_test.cc',
        'lru_storage_test.cc',
        'registry_test.cc',
      ],
      'dependencies': [
        '../testing/testing.gyp:gtest_main',
        'storage',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    # For Android build, base.gyp:encryptor cannot be compiled for the host,
    # while storage is depended from some build tools. So it is necessary
    # to split the following rules from storage/storage_test.
    {
      'target_name': 'encrypted_string_storage',
      'type': 'static_library',
      'sources': [
        'encrypted_string_storage.cc'
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:encryptor',
      ],
    },
    {
      'target_name': 'encrypted_string_storage_test',
      'type': 'executable',
      'sources': [
        'encrypted_string_storage_test.cc',
      ],
      'dependencies': [
        '../testing/testing.gyp:gtest_main',
        'encrypted_string_storage',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    # Test cases meta target: this target is referred from gyp/tests.gyp
    {
      'target_name': 'storage_all_test',
      'type': 'none',
      'dependencies': [
        'encrypted_string_storage_test',
        'storage_test',
      ],
    },
  ],
}
