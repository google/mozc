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
    'relative_dir': 'testing',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  'conditions': [
    ['target_platform=="iOS"', {
      'targets': [
        {
          'target_name': 'gen_ios_test_info_plist',
          'type': 'none',
          'actions': [
            {
              'action_name': 'tweak_info_plist',
              'inputs': [ '<(mozc_oss_src_dir)/data/mac/ios_test_info' ],
              'outputs': [ '<(SHARED_INTERMEDIATE_DIR)/ios_test_Info.plist' ],
              'action': [
                '<(python)', '<(mozc_oss_src_dir)/build_tools/tweak_info_plist.py',
                '--output', '<@(_outputs)',
                '--input', '<@(_inputs)',
                '--version_file', '<(mozc_src_dir)/mozc_version.txt',
                '--branding', '<(branding)',
              ]
            }
          ]
        }
      ]
    }],
  ],
  'targets': [
    {
      'target_name': 'testing',
      'type': 'static_library',
      'variables': {
        'gtest_defines': [
          'GTEST_LANG_CXX11=1',
          'GTEST_HAS_TR1_TUPLE=0',  # disable tr1 tuple in favor of C++11 tuple.
        ],
      },
      'sources': [
        '<(gmock_dir)/src/gmock-cardinalities.cc',
        '<(gmock_dir)/src/gmock-internal-utils.cc',
        '<(gmock_dir)/src/gmock-matchers.cc',
        '<(gmock_dir)/src/gmock-spec-builders.cc',
        '<(gmock_dir)/src/gmock.cc',
        '<(gtest_dir)/src/gtest-assertion-result.cc',
        '<(gtest_dir)/src/gtest-death-test.cc',
        '<(gtest_dir)/src/gtest-filepath.cc',
        '<(gtest_dir)/src/gtest-matchers.cc',
        '<(gtest_dir)/src/gtest-port.cc',
        '<(gtest_dir)/src/gtest-printers.cc',
        '<(gtest_dir)/src/gtest-test-part.cc',
        '<(gtest_dir)/src/gtest-typed-test.cc',
        '<(gtest_dir)/src/gtest.cc',
      ],
      'include_dirs': [
        '<(gmock_dir)',
        '<(gmock_dir)/include',
        '<(gtest_dir)',
        '<(gtest_dir)/include',
      ],
      'defines': [
        '<@(gtest_defines)',
      ],
      'all_dependent_settings': {
        'defines': [
          '<@(gtest_defines)',
        ],
        'include_dirs': [
          '<(gmock_dir)/include',
          '<(gtest_dir)/include',
        ],
      },
      'conditions': [
        ['(_toolset=="target" and compiler_target=="clang") or '
         '(_toolset=="host" and compiler_host=="clang")', {
          'cflags': [
            '-Wno-missing-field-initializers',
            '-Wno-unused-private-field',
          ],
        }],
      ],
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
            '<(python)', '<(mozc_oss_src_dir)/build_tools/embed_pathname.py',
            '--path_to_be_embedded', '<(mozc_oss_data_dir)',
            '--constant_name', 'kMozcDataDir',
            '--output', '<(gen_header_path)',
          ],
        },
      ],
    },
    {
      'target_name': 'gtest_main',
      'type': 'static_library',
      'sources': [
        'gtest_main.cc',
      ],
      'dependencies': [
        '<(mozc_oss_src_dir)/base/base.gyp:base',
        'gen_mozc_data_dir_header#host',
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
              ['target_platform=="iOS" and _toolset=="target"', {
                'mac_bundle': '1',
                'xcode_settings': {
                  'INFOPLIST_FILE': '<(SHARED_INTERMEDIATE_DIR)/ios_test_Info.plist'
                },
              }],
            ],
          }],
        ],
      },
      'conditions': [
        ['target_platform=="iOS" and _toolset=="target"', {
          'dependencies': [
            'gen_ios_test_info_plist',
          ],
        }],
      ],
    },
    {
      'target_name': 'testing_util',
      'type': 'static_library',
      'sources': [
        'testing_util.cc',
      ],
      'dependencies': [
        '<(mozc_oss_src_dir)/base/base.gyp:base_core',
        '<(mozc_oss_src_dir)/protobuf/protobuf.gyp:protobuf',
        'testing',
      ],
    },
    {
      'target_name': 'mozctest',
      'type': 'static_library',
      'sources': [
        'mozctest.cc',
      ],
      'dependencies': [
        '<(mozc_oss_src_dir)/base/absl.gyp:absl_status',
        '<(mozc_oss_src_dir)/base/absl.gyp:absl_strings',
        '<(mozc_oss_src_dir)/base/base.gyp:base_core',
        'gen_mozc_data_dir_header#host',
        'testing',
      ],
    },
  ],
}
