# Copyright 2010-2020, Google Inc.
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
    'relative_dir': 'data/test/session/scenario',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  'targets': [
    {
      'target_name': 'install_session_handler_scenario_test_data',
      'type': 'none',
      'variables': {
        'test_data': [
          'auto_partial_suggestion.txt',
          'b12751061_scenario.txt',
          'b16123009_scenario.txt',
          'b18112966_scenario.txt',
          'b7132535_scenario.txt',
          'b7321313_scenario.txt',
          'b7548679_scenario.txt',
          'b8690065_scenario.txt',
          'b8703702_scenario.txt',
          'change_request.txt',
          'clear_user_prediction.txt',
          'commit.txt',
          'composition_display_as.txt',
          'conversion.txt',
          'conversion_display_as.txt',
          'conversion_with_history_segment.txt',
          'conversion_with_long_history_segments.txt',
          'convert_from_full_ascii_to_t13n.txt',
          'convert_from_full_katakana_to_t13n.txt',
          'convert_from_half_ascii_to_t13n.txt',
          'convert_from_half_katakana_to_t13n.txt',
          'convert_from_hiragana_to_t13n.txt',
          'delete_history.txt',
          'desktop_t13n_candidates.txt',
          'domain_suggestion.txt',
          'input_mode.txt',
          'insert_characters.txt',
          'mobile_partial_variant_candidates.txt',
          'mobile_qwerty_transliteration_scenario.txt',
          'mobile_t13n_candidates.txt',
          'on_off_cancel.txt',
          'partial_suggestion.txt',
          'pending_character.txt',
          'predict_and_convert.txt',
          'reconvert.txt',
          'revert.txt',
          'segment_focus.txt',
          'segment_width.txt',
          'twelvekeys_switch_inputmode_scenario.txt',
          'twelvekeys_toggle_flick_alphabet_scenario.txt',
          'twelvekeys_toggle_hiragana_preedit_scenario.txt',
          'undo.txt',
        ],
        'test_data_subdir': 'data/test/session/scenario',
      },
      'includes': ['../../../../gyp/install_testdata.gypi'],
    },
  ],
}
