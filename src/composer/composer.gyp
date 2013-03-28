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
    'mozc_dir': '..',
    'relative_dir': 'composer',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  'targets': [
    {
      'target_name': 'composer',
      'type': 'static_library',
      'sources': [
        'composer.cc',
        'internal/char_chunk.cc',
        'internal/composition.cc',
        'internal/composition_input.cc',
        'internal/converter.cc',
        'internal/mode_switching_handler.cc',
        'internal/transliterators.cc',
        'internal/typing_corrector.cc',
        'internal/typing_model.cc',
        'table.cc',
      ],
      'dependencies': [
        'embedded_typing_model#host',
        '../base/base.gyp:base',
        '../base/base.gyp:config_file_stream',
        '../config/config.gyp:character_form_manager',
        '../config/config.gyp:config_handler',
        '../config/config.gyp:config_protocol',
        '../protobuf/protobuf.gyp:protobuf',
        '../session/session_base.gyp:key_event_util',
        '../session/session_base.gyp:key_parser',
        # This is needed. GYP is not smart enough about indirect dependencies.
        '../session/session_base.gyp:session_protocol',
        '../transliteration/transliteration.gyp:transliteration',
      ],
    },
    {
      'target_name': 'composer_test',
      'type': 'executable',
      'sources': [
        'composer_test.cc',
        'internal/char_chunk_test.cc',
        'internal/composition_input_test.cc',
        'internal/composition_test.cc',
        'internal/converter_test.cc',
        'internal/mode_switching_handler_test.cc',
        'internal/transliterators_test.cc',
        'internal/typing_corrector_test.cc',
        'table_test.cc',
      ],
      'dependencies': [
        '../config/config.gyp:config_handler',
        '../config/config.gyp:config_protocol',
        '../session/session_base.gyp:request_test_util',
        '../session/session_base.gyp:session_protocol',
        '../testing/testing.gyp:gtest_main',
        'composer',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    # Test cases meta target: this target is referred from gyp/tests.gyp
    {
      'target_name': 'composer_all_test',
      'type': 'none',
      'dependencies': [
        'composer_test',
      ],
    },
    {
      'target_name': 'gen_typing_model',
      'type': 'none',
      'toolsets': ['host'],
      'actions': [
        {
          'action_name': 'gen_qwerty_mobile-hiragana_typing_model',
          'variables': {
            'input_files': [
              '<(mozc_dir)/data/typing/typing_model_qwerty_mobile-hiragana.tsv',
            ],
          },
          'inputs': [
            '<(mozc_dir)/composer/internal/gen_typing_model.py',
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/internal/typing_model_qwerty_mobile-hiragana.h',
          ],
          'action': [
            'python',
            '<(mozc_dir)/composer/internal/gen_typing_model.py',
            '--input_path',
            '<@(input_files)',
            '--output_path',
            '<@(_outputs)',
            '--variable_name',
            'QwertyMobileHiragana'
          ],
        },
        {
          'action_name': 'gen_12keys-hiragana_typing_model',
          'variables': {
            'input_files': [
              '<(mozc_dir)/data/typing/typing_model_12keys-hiragana.tsv',
            ],
          },
          'inputs': [
            '<(mozc_dir)/composer/internal/gen_typing_model.py',
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/internal/typing_model_12keys-hiragana.h',
          ],
          'action': [
            'python',
            '<(mozc_dir)/composer/internal/gen_typing_model.py',
            '--input_path',
            '<@(input_files)',
            '--output_path',
            '<@(_outputs)',
            '--variable_name',
            '12keysHiragana'
          ],
        },
        {
          'action_name': 'gen_flick-hiragana_typing_model',
          'variables': {
            'input_files': [
              '<(mozc_dir)/data/typing/typing_model_flick-hiragana.tsv',
            ],
          },
          'inputs': [
            '<(mozc_dir)/composer/internal/gen_typing_model.py',
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/internal/typing_model_flick-hiragana.h',
          ],
          'action': [
            'python',
            '<(mozc_dir)/composer/internal/gen_typing_model.py',
            '--input_path',
            '<@(input_files)',
            '--output_path',
            '<@(_outputs)',
            '--variable_name',
            'FlickHiragana'
          ],
        },
        {
          'action_name': 'gen_godan-hiragana_typing_model',
          'variables': {
            'input_files': [
              '<(mozc_dir)/data/typing/typing_model_godan-hiragana.tsv',
            ],
          },
          'inputs': [
            '<(mozc_dir)/composer/internal/gen_typing_model.py',
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/internal/typing_model_godan-hiragana.h',
          ],
          'action': [
            'python',
            '<(mozc_dir)/composer/internal/gen_typing_model.py',
            '--input_path',
            '<@(input_files)',
            '--output_path',
            '<@(_outputs)',
            '--variable_name',
            'GodanHiragana'
          ],
        },
        {
          'action_name': 'gen_toggle_flick-hiragana_typing_model',
          'variables': {
            'input_files': [
              '<(mozc_dir)/data/typing/typing_model_toggle_flick-hiragana.tsv',
            ],
          },
          'inputs': [
            '<(mozc_dir)/composer/internal/gen_typing_model.py',
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/internal/typing_model_toggle_flick-hiragana.h',
          ],
          'action': [
            'python',
            '<(mozc_dir)/composer/internal/gen_typing_model.py',
            '--input_path',
            '<@(input_files)',
            '--output_path',
            '<@(_outputs)',
            '--variable_name',
            'ToggleFlickHiragana'
          ],
        },
      ],
    },
    {
      'target_name': 'embedded_typing_model',
      'type': 'none',
      'toolsets': ['host'],
      'hard_dependency': 1,
      'dependencies': [
        'gen_typing_model#host',
      ],
      'export_dependent_settings': [
        'gen_typing_model#host',
      ]
    },
  ],
}
