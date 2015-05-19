# Copyright 2010-2013, Google Inc.
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
  'targets': [
    {
      'target_name': 'base',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'sources': [
        'clock_mock.cc',
        'cpu_stats.cc',
        'crash_report_util.cc',
        'iconv.cc',
        'process.cc',
        'process_mutex.cc',
        'run_level.cc',
        'scheduler.cc',
        'stopwatch.cc',
        'svm.cc',
        'timer.cc',
        'unnamed_event.cc',
        'update_checker.cc',
        'update_util.cc',
        'url.cc',
      ],
      'dependencies': [
        'base_core',
      ],
      'conditions': [
        ['OS=="mac"', {
          'sources': [
            'mac_process.mm',
            'mac_util.mm',
          ],
          'link_settings': {
            'libraries': [
              '/usr/lib/libiconv.dylib',  # used in iconv.cc
            ],
          },
        }],
        ['OS=="win"', {
          'sources': [
            'win_sandbox.cc',
          ],
          'conditions': [
            ['branding=="GoogleJapaneseInput"', {
              'dependencies': [
                '<(DEPTH)/third_party/breakpad/breakpad.gyp:breakpad',
              ],
            }],
          ],
        }],
        # When the target platform is 'Android', build settings are currently
        # shared among *host* binaries and *target* binaries. This means that
        # you should implement *host* binaries by using limited libraries
        # which are also available on NDK.
        ['OS=="linux" and target_platform!="Android" and '
         'not (target_platform=="NaCl" and _toolset=="target")', {
          'defines': [
            'HAVE_LIBRT=1',
          ],
          'link_settings': {
            'libraries': [
              '-lrt',  # used in util.cc for Util::GetTicks()/GetFrequency()
            ],
          },
        }],
        ['target_platform=="Android"', {
          'sources!': [
            'iconv.cc',
            'process.cc',
          ],
        }],
        ['target_platform=="NaCl" and _toolset=="target"', {
          'sources!': [
            'crash_report_handler.cc',
            'crash_report_util.cc',
            'process.cc',
          ],
        }],
      ],
    },
    {
      'target_name': 'base_core',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'sources': [
        '<(gen_out_dir)/character_set.h',
        '<(gen_out_dir)/version_def.h',
        'debug.cc',
        'file_stream.cc',
        'file_util.cc',
        'flags.cc',
        'hash.cc',
        'init.cc',
        'logging.cc',
        'mmap.cc',
        'mutex.cc',
        'number_util.cc',
        'scoped_handle.cc',
        'singleton.cc',
        'string_piece.cc',
        'system_util.cc',
        'text_converter.cc',
        'text_normalizer.cc',
        'thread.cc',
        'util.cc',
        'version.cc',
        'win_util.cc',
      ],
      'dependencies': [
        'gen_character_set#host',
        'gen_version_def#host',
      ],
      'conditions': [
        ['OS=="win"', {
          'link_settings': {
            'msvs_settings': {
              'VCLinkerTool': {
                'AdditionalDependencies': [
                  'aux_ulib.lib',  # used in 'win_util.cc'
                  'propsys.lib',   # used in 'win_util.cc'
                  'version.lib',  # used in 'util.cc'
                ],
              },
            },
          },
        }],
        ['target_platform=="Android" and _toolset=="target"', {
          'sources': [
            'android_util.cc',
          ],
          'dependencies': [
            '../android/android_base.gyp:android_sysconf',
          ],
        }],
        ['target_platform=="NaCl" and _toolset=="target"', {
          'sources': [
            'pepper_file_util.cc',
          ],
        }],
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
              '../data/unicode/CP932.TXT',
              '../data/unicode/JIS0201.TXT',
              '../data/unicode/JIS0208.TXT',
              '../data/unicode/JIS0212.TXT',
              '../data/unicode/jisx0213-2004-std.txt',
            ],
          },
          'inputs': [
            'gen_character_set.py',
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/character_set.h',
          ],
          'action': [
            'python', 'gen_character_set.py',
            '--cp932file=../data/unicode/CP932.TXT',
            '--jisx0201file=../data/unicode/JIS0201.TXT',
            '--jisx0208file=../data/unicode/JIS0208.TXT',
            '--jisx0212file=../data/unicode/JIS0212.TXT',
            '--jisx0213file=../data/unicode/jisx0213-2004-std.txt',
            '--output=<(gen_out_dir)/character_set.h'
          ],
        },
      ],
    },
    {
      'target_name': 'codegen_bytearray_stream',
      'type': 'none',
      'toolsets': ['host'],
      'sources': [
        'codegen_bytearray_stream.h',  # this is header only library.
      ],
      'dependencies': [
        'base_core#host',
      ],
    },
    {
      'target_name': 'jni_proxy',
      'type': 'static_library',
      'conditions': [
        ['target_platform=="Android"', {
          'sources': [
            'android_jni_proxy.cc'
          ],
        }],
      ],
    },
    {
      'target_name': 'encryptor',
      'type': 'static_library',
      'sources': [
        'encryptor.cc',
        'password_manager.cc',
      ],
      'dependencies': [
        'base',
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
        ['OS=="mac"', {
          'defines': [
            'HAVE_OPENSSL=1',
          ],
          'link_settings': {
            'libraries': [
              '/usr/lib/libcrypto.dylib',  # used in 'encryptor.cc'
              '/usr/lib/libssl.dylib',     # used in 'encryptor.cc'
            ],
          }
        }],
        ['OS=="linux" and target_platform!="Android" and '
         'not (target_platform=="NaCl" and _toolset=="target")', {
          'cflags': [
            '<!@(<(pkg_config_command) --cflags-only-other openssl)',
          ],
          'defines': [
            'HAVE_OPENSSL=1',
          ],
          'include_dirs': [
            '<!@(<(pkg_config_command) --cflags-only-I openssl)',
          ],
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg_config_command) --libs-only-L openssl)',
            ],
            'libraries': [
              '<!@(<(pkg_config_command) --libs-only-l openssl)',
            ],
          },
        }],
        ['target_platform=="Android"', {
          'dependencies': [
            'jni_proxy'
          ],
        }],
        ['target_platform=="NaCl" and _toolset=="target"', {
          'dependencies': [
            'openssl_crypto_aes',
            'openssl_config',
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
        'base_core',
        '../protobuf/protobuf.gyp:protobuf',
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
            'python', '../build_tools/replace_version.py',
            '--version_file', '../mozc_version.txt',
            '--input', 'version_def_template.h',
            '--output', '<(gen_out_dir)/version_def.h',
          ],
        },
      ],
    },
    {
      'target_name': 'config_file_stream',
      'type': 'static_library',
      'sources': [
        '<(gen_out_dir)/config_file_stream_data.h',
        'config_file_stream.cc',
      ],
      'dependencies': [
        'gen_config_file_stream_data#host',
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
            '../data/keymap/kotoeri.tsv',
            '../data/keymap/mobile.tsv',
            '../data/keymap/ms-ime.tsv',
            '../data/preedit/12keys-halfwidthascii.tsv',
            '../data/preedit/12keys-hiragana.tsv',
            '../data/preedit/12keys-number.tsv',
            '../data/preedit/flick-halfwidthascii.tsv',
            '../data/preedit/flick-hiragana.tsv',
            '../data/preedit/flick-number.tsv',
            '../data/preedit/hiragana-romanji.tsv',
            '../data/preedit/kana.tsv',
            '../data/preedit/godan-hiragana.tsv',
            '../data/preedit/qwerty_mobile-halfwidthascii.tsv',
            '../data/preedit/qwerty_mobile-hiragana-number.tsv',
            '../data/preedit/qwerty_mobile-hiragana.tsv',
            '../data/preedit/romanji-hiragana.tsv',
            '../data/preedit/toggle_flick-halfwidthascii.tsv',
            '../data/preedit/toggle_flick-hiragana.tsv',
            '../data/preedit/toggle_flick-number.tsv',
          ],
          'outputs': [
            '<(gen_out_dir)/config_file_stream_data.h',
          ],
          'action': [
            'python', 'gen_config_file_stream_data.py',
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
      'target_name': 'task',
      'type': 'static_library',
      'sources': [
        'task_manager.cc',
        'task_runner.cc',
        'task_token.cc',
      ],
      'dependencies': [
        'base',
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
        'base_core',
      ],
    },
    {
      'target_name': 'pepper_file_system_mock',
      'type': 'static_library',
      'sources': [
        'pepper_file_system_mock.cc',
      ],
      'dependencies': [
        'base.gyp:base',
      ],
    },
    {
      'target_name': 'crash_report_handler',
      'type': 'static_library',
      'toolsets': ['target'],
      'sources': [
        'crash_report_handler.cc',
      ],
      'dependencies': [
        'base',
      ],
      'conditions': [
        ['OS=="mac"', {
          'dependencies': [
            # We cannot add 'crash_report_handler.mm' to 'sources' here
            # because we already have 'crash_report_handler.cc' in this
            # target. When a static library target has multiple files
            # with the same basenames in its 'sources' list,
            # gyp (r1354+) treats it as an error.
            'crash_report_handler_mac',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['OS=="mac"', {
      'targets': [
        {
          'target_name': 'crash_report_handler_mac',
          'type': 'static_library',
          'sources': [
            'crash_report_handler.mm',
          ],
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
    ['target_platform=="NaCl"', {
      'targets': [
        {
          'target_name': 'openssl_crypto_aes',
          'type': 'static_library',
          'variables': {
            'openssl_dir%': '<(DEPTH)/third_party/openssl/openssl',
          },
          'direct_dependent_settings': {
            'defines': [
              'HAVE_OPENSSL=1',
            ],
            'include_dirs': [
              '<(gen_out_dir)/openssl/include',
              '<(openssl_dir)/include',
            ],
          },
          'cflags': [
            # Suppresses warnings in third_party codes.
            '-Wno-unused-value',
          ],
          'include_dirs': [
            '<(gen_out_dir)/openssl/include',
            '<(openssl_dir)',
            '<(openssl_dir)/crypto',
            '<(openssl_dir)/include',
          ],
          'hard_dependency': 1,
          'sources': [
            # The following files are the minumum set for 'encryptor.cc'
            '<(openssl_dir)/crypto/aes/aes_cbc.c',
            '<(openssl_dir)/crypto/aes/aes_core.c',
            '<(openssl_dir)/crypto/aes/aes_misc.c',
            '<(openssl_dir)/crypto/mem_clr.c',
            '<(openssl_dir)/crypto/modes/cbc128.c',
            '<(openssl_dir)/crypto/sha/sha1dgst.c',
            '<(openssl_dir)/crypto/sha/sha1_one.c',
            '<(openssl_dir)/crypto/sha/sha256.c',
            '<(openssl_dir)/crypto/sha/sha512.c',
            '<(openssl_dir)/crypto/sha/sha_dgst.c',
            '<(openssl_dir)/crypto/sha/sha_one.c',
          ],
          'dependencies': [
            'openssl_config',
          ],
          'export_dependent_settings': [
            'openssl_config',
          ],
        },
        {
          'target_name': 'openssl_config',
          'type': 'none',
          'actions': [
            {
              'action_name': 'openssl_config',
              'inputs': [
                'openssl_config.sh',
              ],
              'outputs': [
                '<(gen_out_dir)/openssl/include/openssl/opensslconf.h',
              ],
              'action': [
                'bash', 'openssl_config.sh', '<(gen_out_dir)', '$(abspath ./)',
              ],
            },
          ],
        },
      ]},
    ],
  ],
}
