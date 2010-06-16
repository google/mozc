# Copyright 2010, Google Inc.
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
      'sources': [
        '<(gen_out_dir)/character_set.h',
        '<(gen_out_dir)/version_def.cc',
        'cpu_stats.cc',
        'crash_report_handler.cc',
        'crash_report_util.cc',
        'encryptor.cc',
        'flags.cc',
        'hash.cc',
        'iconv.cc',
        'init.cc',
        'logging.cc',
        'mutex.cc',
        'password_manager.cc',
        'process.cc',
        'process_mutex.cc',
        'run_level.cc',
        'scheduler.cc',
        'singleton.cc',
        'stats_config_util.cc',
        'stopwatch.cc',
        'svm.cc',
        'text_converter.cc',
        'timer.cc',
        'unnamed_event.cc',
        'update_util.cc',
        'url.cc',
        'util.cc',
        'version.cc',
      ],
      'conditions': [
        ['OS=="mac"', {
          'sources': [
            'mac_process.mm',
            'mac_util.mm',
          ],
          'conditions': [
            ['branding=="GoogleJapaneseInput"', {
              'sources': [
                'crash_report_handler.mm',
               ],
            }],
          ],
        }],
        ['OS=="win"', {
          'sources': [ 'win_util.cc' ],
        }],
      ],
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
            'python', '../build_tools/redirect.py',
            '<(gen_out_dir)/character_set.h',
            'gen_character_set.py', '<@(input_files)',
          ],
        },
        {
          'action_name': 'gen_version_def',
          'inputs': [
            '../mozc_version.txt',
            '../build_tools/replace_version.py',
            'version_def_template.cc',
          ],
          'outputs': [
            '<(gen_out_dir)/version_def.cc',
          ],
          'action': [
            'python', '../build_tools/replace_version.py',
            '--version_file', '../mozc_version.txt',
            '--input', 'version_def_template.cc',
            '--output', '<(gen_out_dir)/version_def.cc',
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
      'actions': [
        {
          'action_name': 'gen_config_file_stream_data',
          'variables': {
            'input_files': [
              '../data/keymap/atok.tsv',
              '../data/keymap/kotoeri.tsv',
              '../data/keymap/ms-ime.tsv',
              '../data/preedit/kana.tsv',
              '../data/preedit/hiragana-romanji.tsv',
              '../data/preedit/romanji-hiragana.tsv',
            ],
          },
          'inputs': [
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/config_file_stream_data.h',
          ],
          'action': [
            'python', '../build_tools/redirect.py',
            '<(gen_out_dir)/config_file_stream_data.h',
            '<(mozc_build_tools_dir)/gen_config_file_stream_data_main',
            '<@(input_files)'
          ],
        },
      ],
      'conditions': [['two_pass_build==0', {
        'dependencies': [
          'install_gen_config_file_stream_data_main',
        ],
      }]],
    },
    {
      'target_name': 'gen_config_file_stream_data_main',
      'type': 'executable',
      'sources': [
        'gen_config_file_stream_data_main.cc',
      ],
      'dependencies': [
        'base',
      ],
    },
    {
      'target_name': 'install_gen_config_file_stream_data_main',
      'type': 'none',
      'variables': {
        'bin_name': 'gen_config_file_stream_data_main'
      },
      'includes' : [
        '../gyp/install_build_tool.gypi',
      ]
    },
    {
      'target_name': 'base_test',
      'type': 'executable',
      'sources': [
        'bitarray_test.cc',
        'cpu_stats_test.cc',
        'crash_report_util_test.cc',
        'encryptor_test.cc',
        'flags_test.cc',
        'hash_tables_test.cc',
        'logging_test.cc',
        'mmap_test.cc',
        'mutex_test.cc',
        'password_manager_test.cc',
        'process_mutex_test.cc',
        'scheduler_test.cc',
        'singleton_test.cc',
        'stopwatch_test.cc',
        'svm_test.cc',
        'thread_test.cc',
        'timer_test.cc',
        'unnamed_event_test.cc',
        # TODO(satorux): temporarily disabled as it was tricky to build
        # with sidestep on Windows.
        # 'update_util_test.cc',
        # 'stats_config_util_test.cc',
        'url_test.cc',
        'util_test.cc',
        'version_test.cc',
      ],
      'dependencies': [
        'base',
        '../testing/testing.gyp:gtest_main',
      ],
    },
    # init_test.cc is separated from all other base_test because it
    # calls finalizers.
    {
      'target_name': 'base_init_test',
      'type': 'executable',
      'sources': [
        'init_test.cc',
      ],
      'dependencies': [
        'base',
        '../testing/testing.gyp:gtest_main',
      ],
    },
  ],
}
