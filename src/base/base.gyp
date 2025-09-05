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
        'stopwatch.cc',
      ],
      'dependencies': [
        'base_core',
        'absl.gyp:absl_strings',
        'absl.gyp:absl_synchronization',
        'absl.gyp:absl_time',
      ],
      'conditions': [
        ['OS=="mac"', {
          'sources': [
            '../base/mac/mac_process.mm',
            '../base/mac/mac_util.mm',
          ],
        }],
        ['target_platform=="iOS" and _toolset=="target"', {
          'sources!': [
            '../base/mac/mac_process.mm',
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
            'win32/win_api_test_helper.cc',
            'win32/win_sandbox.cc',
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
        'environ.cc',
        'file/recursive.cc',
        'file/temp_dir.cc',
        'file_stream.cc',
        'file_util.cc',
        'init_mozc.cc',
        'log_file.cc',
        'mmap.cc',
        'random.cc',
        'strings/unicode.cc',
        'strings/internal/utf8_internal.cc',
        'system_util.cc',
        'text_normalizer.cc',
        'util.cc',
        'vlog.cc',
      ],
      'dependencies': [
        'clock',
        'flags',
        'hash',
        'singleton',
        'absl.gyp:absl_log',
        'absl.gyp:absl_random',
        'absl.gyp:absl_status',
        'absl.gyp:absl_strings',
        'absl.gyp:absl_synchronization',
        'absl.gyp:absl_time',
      ],
      'conditions': [
        ['OS=="win"', {
          'sources': [
            'win32/hresult.cc',
            'win32/wide_char.cc',
            'win32/win_util.cc',
          ],
          'dependencies': [
            'absl.gyp:absl_base',
          ],
          'link_settings': {
            'msvs_settings': {
              'VCLinkerTool': {
                'AdditionalDependencies': [
                  'aux_ulib.lib',  # used in 'win_util.cc'
                  'pathcch.lib',  # used in 'file/recursive.cc'
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
        'strings/japanese.cc',
        'strings/internal/double_array.cc',
        'strings/internal/japanese_rules.cc',
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
        'base_core',
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
            '<(mozc_src_dir)/mozc_version.txt',
            '<(mozc_oss_src_dir)/build_tools/replace_version.py',
            'version_def_template.h',
          ],
          'outputs': [
            '<(gen_out_dir)/version_def.h',
          ],
          'action': [
            '<(python)', '<(mozc_oss_src_dir)/build_tools/replace_version.py',
            '--version_file', '<(mozc_src_dir)/mozc_version.txt',
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
        'base_core',
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
            '<(mozc_oss_src_dir)/data/keymap/atok.tsv',
            '<(mozc_oss_src_dir)/data/keymap/chromeos.tsv',
            '<(mozc_oss_src_dir)/data/keymap/kotoeri.tsv',
            '<(mozc_oss_src_dir)/data/keymap/mobile.tsv',
            '<(mozc_oss_src_dir)/data/keymap/ms-ime.tsv',
            '<(mozc_oss_src_dir)/data/keymap/overlay_henkan_muhenkan_to_ime_on_off.tsv',
            '<(mozc_oss_src_dir)/data/preedit/12keys-halfwidthascii.tsv',
            '<(mozc_oss_src_dir)/data/preedit/12keys-hiragana.tsv',
            '<(mozc_oss_src_dir)/data/preedit/12keys-hiragana_intuitive.tsv',
            '<(mozc_oss_src_dir)/data/preedit/50keys-hiragana.tsv',
            '<(mozc_oss_src_dir)/data/preedit/flick-halfwidthascii.tsv',
            '<(mozc_oss_src_dir)/data/preedit/flick-halfwidthascii_ios.tsv',
            '<(mozc_oss_src_dir)/data/preedit/flick-hiragana.tsv',
            '<(mozc_oss_src_dir)/data/preedit/flick-hiragana_intuitive.tsv',
            '<(mozc_oss_src_dir)/data/preedit/flick-number.tsv',
            '<(mozc_oss_src_dir)/data/preedit/hiragana-romanji.tsv',
            '<(mozc_oss_src_dir)/data/preedit/kana.tsv',
            '<(mozc_oss_src_dir)/data/preedit/notouch-hiragana.tsv',
            '<(mozc_oss_src_dir)/data/preedit/godan-hiragana.tsv',
            '<(mozc_oss_src_dir)/data/preedit/qwerty_mobile-halfwidthascii.tsv',
            '<(mozc_oss_src_dir)/data/preedit/qwerty_mobile-hiragana.tsv',
            '<(mozc_oss_src_dir)/data/preedit/romanji-hiragana.tsv',
            '<(mozc_oss_src_dir)/data/preedit/toggle_flick-halfwidthascii.tsv',
            '<(mozc_oss_src_dir)/data/preedit/toggle_flick-halfwidthascii_ios.tsv',
            '<(mozc_oss_src_dir)/data/preedit/toggle_flick-hiragana.tsv',
            '<(mozc_oss_src_dir)/data/preedit/toggle_flick-hiragana_intuitive.tsv',
            '<(mozc_oss_src_dir)/data/preedit/toggle_flick-number.tsv',
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
      'target_name': 'serialized_string_array',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'sources': [
        'container/serialized_string_array.cc',
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
              '<(python)', '<(mozc_oss_src_dir)/build_tools/build_breakpad.py',
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
      ]},
    ],
  ],
}
