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
  'variables': {
    'relative_dir': 'testing',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  'targets': [
    {
      'target_name': 'testing',
      'type': 'static_library',
      'sources': [
        '<(DEPTH)/third_party/gmock/src/gmock-cardinalities.cc',
        '<(DEPTH)/third_party/gmock/src/gmock-internal-utils.cc',
        '<(DEPTH)/third_party/gmock/src/gmock-matchers.cc',
        '<(DEPTH)/third_party/gmock/src/gmock-spec-builders.cc',
        '<(DEPTH)/third_party/gmock/src/gmock.cc',
        '<(DEPTH)/third_party/gtest/src/gtest-death-test.cc',
        '<(DEPTH)/third_party/gtest/src/gtest-filepath.cc',
        '<(DEPTH)/third_party/gtest/src/gtest-port.cc',
        '<(DEPTH)/third_party/gtest/src/gtest-printers.cc',
        '<(DEPTH)/third_party/gtest/src/gtest-test-part.cc',
        '<(DEPTH)/third_party/gtest/src/gtest-typed-test.cc',
        '<(DEPTH)/third_party/gtest/src/gtest.cc',
      ],
      'include_dirs': [
        '<(DEPTH)/third_party/gmock',
        '<(DEPTH)/third_party/gmock/include',
        '<(DEPTH)/third_party/gtest',
        '<(DEPTH)/third_party/gtest/include',
      ]
    },
    {
      'target_name': 'gen_mozc_data_dir_header',
      'type': 'none',
      'toolsets': ['host'],
      'actions': [
        {
          'action_name': 'gen_mozc_data_dir_header',
          'variables': {
            'gen_header_path': '<(gen_out_dir)/mozc_data_dir.h',
          },
          'inputs': [
          ],
          'outputs': [
            '<(gen_header_path)',
          ],
          'action': [
            'python', '../build_tools/embed_pathname.py',
            '--path_to_be_embedded', '<(mozc_data_dir)',
            '--constant_name', 'kMozcDataDir',
            '--output', '<(gen_header_path)',
          ],
        },
      ],
    },
    {
      'target_name': 'googletest_lib',
      'type': 'static_library',
      'sources': [
        'base/internal/googletest.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'gen_mozc_data_dir_header#host',
        'testing',
      ],
    },
    {
      'target_name': 'gtest_main',
      'type': 'static_library',
      'sources': [
        'base/internal/gtest_main.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'gen_mozc_data_dir_header#host',
        'googletest_lib',
        'testing',
      ],
      'link_settings': {
        'target_conditions': [
          ['_type=="executable"', {
            'conditions': [
              ['OS=="win"', {
                'msvs_settings': {
                  'VCLinkerTool': {
                    'SubSystem': '1',  # 1 == subSystemConsole
                  },
                },
                'run_as': {
                  'working_directory': '$(TargetDir)',
                  'action': ['$(TargetPath)'],
                },
              }],
              ['OS=="mac"', {
                'run_as': {
                  'working_directory': '${BUILT_PRODUCTS_DIR}',
                  'action': ['${BUILT_PRODUCTS_DIR}/${PRODUCT_NAME}'],
                },
              }],
            ],
          }],
        ],
      },
    },
  ],
}
