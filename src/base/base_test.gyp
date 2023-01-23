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
    'relative_dir': 'base',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  # Platform-specific targets.
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'win_util_test_dll',
          'type': 'shared_library',
          'sources': [
            'win_util_test_dll.cc',
            'win_util_test_dll.def',
          ],
          'dependencies': [
            'base.gyp:base',
          ],
        },
      ],
    }],
  ],
  'targets': [
    {
      'target_name': 'base_test',
      'type': 'executable',
      'sources': [
        'codegen_bytearray_stream_test.cc',
        'cpu_stats_test.cc',
        'process_mutex_test.cc',
        'stopwatch_test.cc',
      ],
      'conditions': [
        ['OS=="mac"', {
          'sources': [
            'mac_util_test.mm',
          ],
        }],
        ['OS=="win"', {
          'sources': [
            'win_api_test_helper_test.cc',
            'win_sandbox_test.cc',
          ],
        }],
      ],
      'dependencies': [
        '../testing/testing.gyp:gtest_main',
        'absl.gyp:absl_status',
        'base.gyp:base',
        'base.gyp:codegen_bytearray_stream#host',
        'clock_mock',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'url_test',
      'type': 'executable',
      'sources': [
        'url_test.cc',
      ],
      'dependencies': [
        '../testing/testing.gyp:gtest_main',
        'absl.gyp:absl_strings',
        'base.gyp:base_core',  # for util
        'base.gyp:url',
      ],
    },
    {
      'target_name': 'base_core_test',
      'type': 'executable',
      'sources': [
        'bitarray_test.cc',
        'logging_test.cc',
        'mmap_test.cc',
        'singleton_test.cc',
        'text_normalizer_test.cc',
        'thread_test.cc',
        'version_test.cc',
      ],
      'conditions': [
        ['OS=="win"', {
          'sources': [
            'win_util_test.cc',
          ],
        }],
      ],
      'dependencies': [
        '../testing/testing.gyp:gtest_main',
        'absl.gyp:absl_strings',
        'base.gyp:base_core',
        'base.gyp:version',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'clock_mock',
      'toolsets': ['host', 'target'],
      'type': 'static_library',
      'sources': [
        'clock_mock.cc'
      ],
      'dependencies': [
        'absl.gyp:absl_time',
      ],
    },
    {
      'target_name': 'clock_mock_test',
      'type': 'executable',
      'sources': [
        'clock_mock_test.cc',
      ],
      'dependencies': [
        '../testing/testing.gyp:gtest_main',
        'clock_mock',
      ],
    },
    {
      'target_name': 'update_util_test',
      'type': 'executable',
      'sources': [
        'update_util_test.cc'
      ],
      'dependencies': [
        '../testing/testing.gyp:gtest_main',
        'base.gyp:update_util',
      ],
    },
    {
      'target_name': 'japanese_util_test',
      'type': 'executable',
      'sources': [
        'japanese_util_test.cc',
      ],
      'dependencies': [
        '../testing/testing.gyp:gtest_main',
        'absl.gyp:absl_strings',
        'base.gyp:japanese_util',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'util_test',
      'type': 'executable',
      'sources': [
        'util_test.cc',
      ],
      'dependencies': [
        '../testing/testing.gyp:gtest_main',
        '../testing/testing.gyp:mozctest',
        'absl.gyp:absl_strings',
        'base.gyp:base_core',
        'base.gyp:number_util',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'hash_test',
      'type': 'executable',
      'sources': [
        'hash_test.cc',
      ],
      'dependencies': [
        '../testing/testing.gyp:gtest_main',
        'base.gyp:base_core',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'clock_test',
      'type': 'executable',
      'sources': [
        'clock_test.cc',
      ],
      'dependencies': [
        '../testing/testing.gyp:gtest_main',
        'base.gyp:base',
        'clock_mock',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'number_util_test',
      'type': 'executable',
      'sources': [
        'number_util_test.cc',
      ],
      'dependencies': [
        '../testing/testing.gyp:gtest_main',
        'base.gyp:base_core',
        'base.gyp:number_util',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'file_util_test',
      'type': 'executable',
      'sources': [
        'file_util_test.cc',
      ],
      'dependencies': [
        '../testing/testing.gyp:gtest_main',
        'absl.gyp:absl_strings',
        'base.gyp:base_core',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'system_util_test',
      'type': 'executable',
      'sources': [
        'system_util_test.cc',
      ],
      'dependencies': [
        '../testing/testing.gyp:gtest_main',
        'base.gyp:base_core',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'obfuscator_support_test',
      'type': 'executable',
      'sources': [
        'unverified_aes256_test.cc',
        'unverified_sha1_test.cc',
      ],
      'dependencies': [
        '../testing/testing.gyp:gtest_main',
        'base.gyp:obfuscator_support',
      ],
    },
    {
      'target_name': 'encryptor_test',
      'type': 'executable',
      'sources': [
        'encryptor_test.cc',
        'password_manager_test.cc',
      ],
      'dependencies': [
        '../testing/testing.gyp:gtest_main',
        'base.gyp:encryptor',
      ],
    },
    {
      'target_name': 'config_file_stream_test',
      'type': 'executable',
      'sources': [
        'config_file_stream_test.cc',
      ],
      'dependencies': [
        '../testing/testing.gyp:gtest_main',
        'base.gyp:config_file_stream',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'trie_test',
      'type': 'executable',
      'sources': [
        'trie_test.cc',
      ],
      'dependencies': [
        '../testing/testing.gyp:gtest_main',
        'base.gyp:base',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'multifile_test',
      'type': 'executable',
      'sources': [
        'multifile_test.cc',
      ],
      'dependencies': [
        '../testing/testing.gyp:gtest_main',
        'absl.gyp:absl_strings',
        'base.gyp:multifile',
      ],
    },
    {
      'target_name': 'gen_embedded_file_test_data',
      'type': 'none',
      'toolsets': ['host'],
      'actions': [
        {
          'action_name': 'gen_embedded_file_test_data',
          'variables': {
            'input': 'embedded_file.h',
            'gen_header_path': '<(gen_out_dir)/embedded_file_test_data.inc',
          },
          'inputs': [
            '<(input)',
          ],
          'outputs': [
            '<(gen_header_path)',
          ],
          'action': [
            '<(python)', '../build_tools/embed_file.py',
            '--input', '<(input)',
            '--name', 'kEmbeddedFileTestData',
            '--output', '<(gen_header_path)',
          ],
        },
      ],
    },
    {
      'target_name': 'install_embedded_file_h',
      'type': 'none',
      'variables': {
        # Copy the test data for embedded file test.
        'test_data_subdir': 'base',
        'test_data': ['../<(test_data_subdir)/embedded_file.h'],
      },
      'includes': [ '../gyp/install_testdata.gypi' ],
    },
    {
      'target_name': 'embedded_file_test',
      'type': 'executable',
      'sources': [
        'embedded_file_test.cc',
      ],
      'dependencies': [
        '../testing/testing.gyp:gtest_main',
        '../testing/testing.gyp:mozctest',
        'gen_embedded_file_test_data#host',
        'install_embedded_file_h',
      ],
    },
    {
      'target_name': 'serialized_string_array_test',
      'type': 'executable',
      'sources': [
        'serialized_string_array_test.cc',
      ],
      'dependencies': [
        '../testing/testing.gyp:gtest_main',
        'base.gyp:base',
        'base.gyp:serialized_string_array',
      ],
    },
    # Test cases meta target: this target is referred from gyp/tests.gyp
    {
      'target_name': 'base_all_test',
      'type': 'none',
      'dependencies': [
        'base_core_test',
        'base_test',
        'clock_mock_test',
        'clock_test',
        'config_file_stream_test',
        'embedded_file_test',
        'encryptor_test',
        'file_util_test',
        'hash_test',
        'japanese_util_test',
        'multifile_test',
        'number_util_test',
        'obfuscator_support_test',
        'serialized_string_array_test',
        'system_util_test',
        'trie_test',
        'update_util_test',
        'url_test',
        'util_test',
      ],
      'conditions': [
        # To work around a link error on Ninja build, we put this target in
        # 'base_all_test'.
        ['OS=="win"', {
          'dependencies': [
            'win_util_test_dll',
          ],
        }],
      ],
    },
  ],
}
