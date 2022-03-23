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
    'document_dir%': '/usr/lib/mozc/documents',
  },
  'targets': [
    {
      'target_name': 'base',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'sources': [
        'cpu_stats.cc',
        'process.cc',
        'process_mutex.cc',
        'run_level.cc',
        'scheduler.cc',
        'stopwatch.cc',
        'unnamed_event.cc',
      ],
      'dependencies': [
        'base_core',
        'absl.gyp:absl_strings',
        'absl.gyp:absl_synchronization',
      ],
      'conditions': [
        ['OS=="mac"', {
          'sources': [
            'mac_process.mm',
            'mac_util.mm',
          ],
        }],
        ['target_platform=="iOS" and _toolset=="target"', {
          'sources!': [
            'mac_process.mm',
            'process.cc',
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/UIKit.framework',
            ],
          },
        }],
        ['OS=="win"', {
          'sources': [
            'win_api_test_helper.cc',
            'win_sandbox.cc',
          ],
        }],
      ],
    },
    {
      'target_name': 'url',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'sources': [
        'url.cc',
      ],
      'dependencies': [
        'base.gyp:version',
        'base_core',  # for logging, util, version
        'singleton',
      ],
    },
    {
      'target_name': 'base_core',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'sources': [
        '<(gen_out_dir)/character_set.inc',
        'environ.cc',
        'file_stream.cc',
        'file_util.cc',
        'init_mozc.cc',
        'logging.cc',
        'mmap.cc',
        'system_util.cc',
        'text_normalizer.cc',
        'thread.cc',
        'util.cc',
        'win_util.cc',
      ],
      'dependencies': [
        'clock',
        'flags',
        'gen_character_set#host',
        'hash',
        'singleton',
        'absl.gyp:absl_status',
        'absl.gyp:absl_strings',
        'absl.gyp:absl_synchronization',
        'absl.gyp:absl_time',
      ],
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            'scoped_handle',
            'absl.gyp:absl_base',
          ],
          'link_settings': {
            'msvs_settings': {
              'VCLinkerTool': {
                'AdditionalDependencies': [
                  'aux_ulib.lib',  # used in 'win_util.cc'
                  'KtmW32.lib',  # used in 'file_util.cc'
                ],
              },
            },
          },
        }],
        ['target_platform=="Linux" and server_dir!=""', {
          'defines': [
            'MOZC_SERVER_DIR="<(server_dir)"',
          ],
        }],
        ['target_platform=="Linux" and document_dir!=""', {
          'defines': [
            'MOZC_DOCUMENT_DIR="<(document_dir)"',
          ],
        }],
      ],
    },
    {
      'target_name': 'update_util',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'sources': [
        'update_util.cc',
      ],
    },
    {
      'target_name': 'japanese_util',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'sources': [
        'japanese_util.cc',
        'japanese_util_rule.cc',
      ],
    },
    {
      'target_name': 'number_util',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'sources': [
        'number_util.cc',
      ],
      'dependencies': [
        'japanese_util',
      ],
    },
    {
      'target_name': 'version',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'sources': [
        '<(gen_out_dir)/version_def.h',
        'version.cc',
      ],
      'dependencies': [
        'gen_version_def#host',
        'number_util',
      ],
    },
    {
      'target_name': 'scoped_handle',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'sources': [
        'scoped_handle.cc',
      ],
    },
    {
      'target_name': 'singleton',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'sources': [
        'singleton.cc',
      ],
      'dependencies': [
        'absl.gyp:absl_base',
      ],
    },
    {
      'target_name': 'flags',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'dependencies': [
        'absl.gyp:absl_flags',
      ],
    },
    {
      'target_name': 'clock',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'sources': [
        'clock.cc',
      ],
      'dependencies': [
        'singleton',
        'absl.gyp:absl_time',
      ],
    },
    {
      'target_name': 'hash',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'sources': [
        'hash.cc',
      ],
      'dependencies': [
        'absl.gyp:absl_strings',
      ],
    },
    {
      'target_name': 'gen_character_set',
      'type': 'none',
      'toolsets': ['host'],
      'actions': [
        {
          'action_name': 'gen_character_set',
          'variables': {
            'input_files': [
              '../data/unicode/JIS0201.TXT',
              '../data/unicode/JIS0208.TXT',
            ],
          },
          'inputs': [
            'gen_character_set.py',
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/character_set.inc',
          ],
          'action': [
            '<(python)', 'gen_character_set.py',
            '--jisx0201file=../data/unicode/JIS0201.TXT',
            '--jisx0208file=../data/unicode/JIS0208.TXT',
            '--output=<(gen_out_dir)/character_set.inc'
          ],
        },
      ],
    },
    {
      'target_name': 'codegen_bytearray_stream',
      'type': 'static_library',
      'toolsets': ['host'],
      'sources': [
        'codegen_bytearray_stream.cc',
      ],
      'dependencies': [
        'base_core#host',
      ],
    },
    {
      'target_name': 'obfuscator_support',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'sources': [
        'unverified_aes256.cc',
        'unverified_sha1.cc',
      ],
      'dependencies': [
        'base',
      ],
    },
    {
      'target_name': 'encryptor',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'sources': [
        'encryptor.cc',
        'password_manager.cc',
      ],
      'dependencies': [
        'absl.gyp:absl_strings',
        'absl.gyp:absl_synchronization',
        'base',
        'obfuscator_support',
      ],
      'conditions': [
        ['OS=="win"', {
          'link_settings': {
            'msvs_settings': {
              'VCLinkerTool': {
                'AdditionalDependencies': [
                  'crypt32.lib',  # used in 'encryptor.cc'
                ],
              },
            },
          },
        }],
      ],
    },
    {
      'target_name': 'gen_version_def',
      'type': 'none',
      'toolsets': ['host'],
      'actions': [
        {
          'action_name': 'gen_version_def',
          'inputs': [
            '../mozc_version.txt',
            '../build_tools/replace_version.py',
            'version_def_template.h',
          ],
          'outputs': [
            '<(gen_out_dir)/version_def.h',
          ],
          'action': [
            '<(python)', '../build_tools/replace_version.py',
            '--version_file', '../mozc_version.txt',
            '--input', 'version_def_template.h',
            '--output', '<(gen_out_dir)/version_def.h',
            '--branding', '<(branding)',
          ],
        },
      ],
    },
    {
      'target_name': 'config_file_stream',
      'type': 'static_library',
      'sources': [
        '<(gen_out_dir)/config_file_stream_data.inc',
        'config_file_stream.cc',
      ],
      'dependencies': [
        'gen_config_file_stream_data#host',
        'absl.gyp:absl_strings',
      ],
    },
    {
      'target_name': 'gen_config_file_stream_data',
      'type': 'none',
      'toolsets': ['host'],
      'actions': [
        {
          'action_name': 'gen_config_file_stream_data',
          'inputs': [
            '../data/keymap/atok.tsv',
            '../data/keymap/chromeos.tsv',
            '../data/keymap/kotoeri.tsv',
            '../data/keymap/mobile.tsv',
            '../data/keymap/ms-ime.tsv',
            '../data/preedit/12keys-halfwidthascii.tsv',
            '../data/preedit/12keys-hiragana.tsv',
            '../data/preedit/12keys-hiragana_intuitive.tsv',
            '../data/preedit/flick-halfwidthascii.tsv',
            '../data/preedit/flick-halfwidthascii_ios.tsv',
            '../data/preedit/flick-hiragana.tsv',
            '../data/preedit/flick-hiragana_intuitive.tsv',
            '../data/preedit/hiragana-romanji.tsv',
            '../data/preedit/kana.tsv',
            '../data/preedit/notouch-hiragana.tsv',
            '../data/preedit/godan-hiragana.tsv',
            '../data/preedit/qwerty_mobile-halfwidthascii.tsv',
            '../data/preedit/qwerty_mobile-hiragana.tsv',
            '../data/preedit/romanji-hiragana.tsv',
            '../data/preedit/toggle_flick-halfwidthascii.tsv',
            '../data/preedit/toggle_flick-halfwidthascii_ios.tsv',
            '../data/preedit/toggle_flick-hiragana.tsv',
            '../data/preedit/toggle_flick-hiragana_intuitive.tsv',
          ],
          'outputs': [
            '<(gen_out_dir)/config_file_stream_data.inc',
          ],
          'action': [
            '<(python)', 'gen_config_file_stream_data.py',
            '--output', '<@(_outputs)',
            '<@(_inputs)',
          ],
          'dependencies': [
            'gen_config_file_stream_data.py',
          ],
        },
      ],
    },
    {
      'target_name': 'multifile',
      'type': 'static_library',
      'toolsets': ['target', 'host'],
      'sources': [
        'multifile.cc',
      ],
      'dependencies': [
        'absl.gyp:absl_strings',
        'base_core',
      ],
    },
    {
      'target_name': 'crash_report_handler',
      'type': 'static_library',
      'sources': [
        'crash_report_handler.cc',
      ],
      'dependencies': [
        'base',
        'base.gyp:version',
      ],
      'conditions': [
        ['OS=="win" and branding=="GoogleJapaneseInput"', {
          'dependencies': [
            'breakpad',
          ],
        }],
        ['OS=="mac"', {
          'hard_dependency': 1,
          'sources': [
            'crash_report_handler_mac.mm',
          ],
          'sources!': [
            'crash_report_handler.cc',
          ],
          'dependencies': [
            'breakpad',
          ],
        }],
      ],
    },
    {
      'target_name': 'serialized_string_array',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'sources': [
        'serialized_string_array.cc',
      ],
      'dependencies': [
        'base_core',
      ],
    },
  ],
  'conditions': [
    ['OS=="win" and branding=="GoogleJapaneseInput"', {
      'targets': [
        {
          'target_name': 'breakpad',
          'type': 'static_library',
          'variables': {
            'breakpad_root': '<(third_party_dir)/breakpad',
          },
          'include_dirs': [
            # Use the local glog configured for Windows.
            # See b/2954681 for details.
            '<(breakpad_root)/src/third_party/glog/glog/src/windows',
            '<(breakpad_root)/src',
          ],
          'sources': [
            '<(breakpad_root)/src/client/windows/crash_generation/client_info.cc',
            '<(breakpad_root)/src/client/windows/crash_generation/crash_generation_client.cc',
            '<(breakpad_root)/src/client/windows/crash_generation/crash_generation_server.cc',
            '<(breakpad_root)/src/client/windows/crash_generation/minidump_generator.cc',
            '<(breakpad_root)/src/client/windows/handler/exception_handler.cc',
            '<(breakpad_root)/src/client/windows/sender/crash_report_sender.cc',
            '<(breakpad_root)/src/common/windows/guid_string.cc',
            '<(breakpad_root)/src/common/windows/http_upload.cc'
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '<(breakpad_root)/src',
            ],
          },
          'link_settings': {
            'msvs_settings': {
              'VCLinkerTool': {
                'AdditionalDependencies': [
                  'dbghelp.lib',
                ],
              },
            },
          },
        },
      ]},
    ],
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'win_font_test_helper',
          'type': 'static_library',
          'sources': [
            'win_font_test_helper.cc',
          ],
          'dependencies': [
            'base',
          ],
          'copies': [
            {
              'files': [
                '<(DEPTH)/third_party/ipa_font/ipaexg.ttf',
                '<(DEPTH)/third_party/ipa_font/ipaexm.ttf',
              ],
              'destination': '<(PRODUCT_DIR)/data',
            },
          ],
        },
      ]},
    ],
    ['OS=="mac"', {
      'targets': [
        {
          'target_name': 'breakpad',
          'type': 'none',
          'variables': {
            'bpdir': '<(third_party_dir)/breakpad',
          },
          'actions': [{
            'action_name': 'build_breakpad',
            'inputs': [
              '<(bpdir)/src/client/mac/Breakpad.xcodeproj/project.pbxproj',
            ],
            'outputs': [
              '<(mac_breakpad_dir)/Breakpad.framework',
              '<(mac_breakpad_dir)/Inspector',
              '<(mac_breakpad_dir)/dump_syms',
              '<(mac_breakpad_dir)/symupload',
            ],
            'action': [
              '<(python)', '../build_tools/build_breakpad.py',
              '--bpdir', '<(bpdir)',
              '--outdir', '<(mac_breakpad_dir)',
              '--sdk', 'macosx<(mac_sdk)',
              '--deployment_target', '<(mac_deployment_target)',
            ],
          }],
          'direct_dependent_settings': {
            'mac_framework_dirs': [
              '<(mac_breakpad_dir)',
            ],
          },
        },
        {
          'target_name': 'mac_util_main',
          'type': 'executable',
          'sources': [
            'mac_util_main.cc',
          ],
          'dependencies': [
            'base',
          ],
        },
      ]},
    ],
  ],
}
