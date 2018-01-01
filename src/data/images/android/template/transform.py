# -*- coding: utf-8 -*-
# Copyright 2010-2018, Google Inc.
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

"""Generates svg files from templates.

Usage:
$ mkdir OUTPUT_PATH
$ python transform.py --input_dir=data/images/android/template --output_dir=OUTPUT_PATH

Note:
We use half-width characters for following to adjust the position
on software keyboards.
See also: b/16722519

"(", ")", "!", "?", ":"

Half-width is also used for "/" for better look.
"""

import logging
import optparse
import os
import tempfile
import zipfile


_REPLACE_CHAR = '中'
_REPLACE_CHAR_QWERTY_SUPPORT = '副'

_REPLACE_CHARS = ('中', '左', '上', '右', '下')

_REPLACE_ID_SUFFIXEX = (
    '{id_suffix_center}', '{id_suffix_left}', '{id_suffix_up}',
    '{id_suffix_right}', '{id_suffix_down}')

_EXPAND_SUPPORT_SUFFIXES = (
    'center', 'left', 'up', 'right', 'down', 'release')

_GODAN_KANA_SUPPORT_KEY_DEFINITIONS = [
    {'name': '01',
     'chars': ('A', '', '', '', '1'),
     'id_suffixes': ('', '-light', '-light', '-light', '-light')},
    {'name': '02',
     'chars': ('K', '', 'Q', 'G', '2'),
     'id_suffixes': ('', '-light', '-light', '-light', '-light')},
    {'name': '03',
     'chars': ('H', 'P', 'F', 'B', '3'),
     'id_suffixes': ('', '-light', '-light', '-light', '-light')},
    {'name': '04',
     'chars': ('I', '', '', '', '4'),
     'id_suffixes': ('', '-light', '-light', '-light', '-light')},
    {'name': '05',
     'chars': ('S', '', 'J', 'Z', '5'),
     'id_suffixes': ('', '-light', '-light', '-light', '-light')},
    {'name': '06',
     'chars': ('M', '/', 'L', 'ー', '6'),
     'id_suffixes': ('', '-light', '-light', '-light', '-light')},
    {'name': '07',
     'chars': ('U', '', '', '', '7'),
     'id_suffixes': ('', '-light', '-light', '-light', '-light')},
    {'name': '08',
     'chars': ('T', '', 'C', 'D', '8'),
     'id_suffixes': ('', '-light', '-light', '-light', '-light')},
    {'name': '09',
     'chars': ('Y', '(', 'X', ')', '9'),
     'id_suffixes': ('', '-light', '-light', '-light', '-light')},
    {'name': '10',
     'chars': ('E', '', '', '', ''),
     'id_suffixes': ('', '-light', '-light', '-light', '-light')},
    {'name': '11',
     'chars': ('N', ':', '', '・', '0'),
     'id_suffixes': ('', '-light', '-light', '-light', '-light')},
    # 12: Needs another template. Defined bellow
    {'name': '13',
     'chars': ('O', '', '', '', ''),
     'id_suffixes': ('', '-light', '-light', '-light', '-light')},
    # 14: "大小" key(godan__kana__14.svg): Will be modified directly
    {'name': '15',
     'chars': ('W', '「', 'V', '」', ''),
     'id_suffixes': ('', '-light', '-light', '-light', '-light')},
    ]

_GODAN_KANA_SUPPORT_12_KEY_DEFINITION = [
    {'name': '12',
     'chars': ('R', '。', '?', '!', '、'),
     'id_suffixes': ('', '-light', '-light', '-light', '-light')},
    ]

_GODAN_KANA_SUPPORT_POPUP_KEY_DEFINITIONS = [
    {'name': '01', 'chars': ('A', '', '', '', '1')},
    {'name': '02', 'chars': ('K', '', 'Q', 'G', '2')},
    {'name': '03', 'chars': ('H', 'P', 'F', 'B', '3')},
    {'name': '04', 'chars': ('I', '', '', '', '4')},
    {'name': '05', 'chars': ('S', '', 'J', 'Z', '5')},
    {'name': '06', 'chars': ('M', '/', 'L', 'ー', '6')},
    {'name': '07', 'chars': ('U', '', '', '', '7')},
    {'name': '08', 'chars': ('T', '', 'C', 'D', '8')},
    {'name': '09', 'chars': ('Y', '(', 'X', ')', '9')},
    {'name': '10', 'chars': ('E', '', '', '', '')},
    # Use half-width ':' to adjust character position.
    {'name': '11', 'chars': ('N', ':', 'ん', '・', '０')},
    {'name': '12', 'chars': ('R', '。', '?', '!', '、')},
    {'name': '13', 'chars': ('O', '', '', '', '')},
    # 14: "大小" key: godan__kana__14.svg will be used.
    # Please check mozc/android/resources/res/xml/
    {'name': '15', 'chars': ('W', '「', 'V', '」', '')},
    ]

_QWERTY_SUPPORT_KEY_MAPPING = {
    'q': '1',
    'w': '2',
    'e': '3',
    'r': '4',
    't': '5',
    'y': '6',
    'u': '7',
    'i': '8',
    'o': '9',
    'p': '0',
}

# 'A'-'Z' will be generated by GenerateQwertyAlphabetKeyDefinitions().
_QWERTY_KEYICON_KEY_DEFINITIONS = [
    {'name': 'digit_zero', 'char': '0'},
    {'name': 'digit_one', 'char': '1'},
    {'name': 'digit_two', 'char': '2'},
    {'name': 'digit_three', 'char': '3'},
    {'name': 'digit_four', 'char': '4'},
    {'name': 'digit_five', 'char': '5'},
    {'name': 'digit_six', 'char': '6'},
    {'name': 'digit_seven', 'char': '7'},
    {'name': 'digit_eight', 'char': '8'},
    {'name': 'digit_nine', 'char': '9'},

    {'name': 'ampersand', 'char': '&amp;'},
    {'name': 'apostrophe', 'char': '&apos;'},
    {'name': 'asterisk', 'char': '*'},
    {'name': 'circumflex_accent', 'char': '^'},
    {'name': 'colon', 'char': ':'},
    {'name': 'comma', 'char': ','},
    {'name': 'commercial_at', 'char': '@'},
    {'name': 'dollar_sign', 'char': '$'},
    {'name': 'equals_sign', 'char': '='},
    {'name': 'exclamation_mark', 'char': '!'},
    {'name': 'full_stop', 'char': '.'},
    {'name': 'grave_accent', 'char': '`'},
    {'name': 'less_than_sign', 'char': '&lt;'},
    {'name': 'greater_than_sign', 'char': '&gt;'},
    {'name': 'hyphen_minus', 'char': '-'},
    {'name': 'ideographic_comma', 'char': '、'},
    {'name': 'ideographic_full_stop', 'char': '。'},
    {'name': 'katakana_middle_dot', 'char': '・'},
    {'name': 'left_corner_bracket', 'char': '「'},
    {'name': 'right_corner_bracket', 'char': '」'},
    {'name': 'left_curly_bracket', 'char': '{'},
    {'name': 'right_curly_bracket', 'char': '}'},
    {'name': 'left_parenthesis', 'char': '('},
    {'name': 'right_parenthesis', 'char': ')'},
    {'name': 'left_square_bracket', 'char': '['},
    {'name': 'right_square_bracket', 'char': ']'},
    {'name': 'low_line', 'char': '_'},
    {'name': 'minus_sign', 'char': '−'},
    {'name': 'number_sign', 'char': '#'},
    {'name': 'percent_sign', 'char': '%'},
    {'name': 'plus_sign', 'char': '+'},
    {'name': 'question_mark', 'char': '?'},
    {'name': 'quotation_mark', 'char': '&quot;'},
    {'name': 'reverse_solidus', 'char': '\\'},
    {'name': 'semicolon', 'char': ';'},
    {'name': 'solidus', 'char': '/'},
    {'name': 'tilde', 'char': '~'},
    {'name': 'vertical_line', 'char': '|'},

    # Use half-width ':' to adjust character position.
    {'name': 'fullwidth_colon', 'char': ':'},

    # Use half-width '/' for a better look.
    # TODO(team): This rule generates qwerty__keyicon__fullwidth__solidus but
    # currently it's used only by GODAN.  Rename it.
    {'name': 'fullwidth_solidus', 'char': '/'},
    ]

_TWELVEKEYS_ALPHABET_KEY_DEFINITIONS = [
    # 01, 11 and 12 have spaces among their characters.
    # Without them shown characters are too close.
    {'name': '01', 'char': '@ - _ /'},
    {'name': '02', 'char': 'ABC'},
    {'name': '03', 'char': 'DEF'},
    {'name': '04', 'char': 'GHI'},
    {'name': '05', 'char': 'JKL'},
    {'name': '06', 'char': 'MNO'},
    {'name': '07', 'char': 'PQRS'},
    {'name': '08', 'char': 'TUV'},
    {'name': '09', 'char': 'WXYZ'},
    # 10: "a-A" key. Will be modified directly.
    {'name': '11', 'char': '\' " : ;'},
    {'name': '12', 'char': '. , ? !'},
    ]

_TWELVEKEYS_ALPHABET_SUPPORT_KEY_DEFINITIONS = [
    # 01, 11 and 12 have spaces among their characters.
    # Without them shown characters are too close.
    {'name': '01', 'chars': ('@ - _ /', '', '', '', '1')},
    {'name': '02', 'chars': ('ABC', '', '', '', '2')},
    {'name': '03', 'chars': ('DEF', '', '', '', '3')},
    {'name': '04', 'chars': ('GHI', '', '', '', '4')},
    {'name': '05', 'chars': ('JKL', '', '', '', '5')},
    {'name': '06', 'chars': ('MNO', '', '', '', '6')},
    {'name': '07', 'chars': ('PQRS', '', '', '', '7')},
    {'name': '08', 'chars': ('TUV', '', '', '', '8')},
    {'name': '09', 'chars': ('WXYZ', '', '', '', '9')},
    # 10: "a-A" key. Will be modified directly.
    {'name': '11', 'chars': ('\' " : ;', '', '', '', '0')},
    {'name': '12', 'chars': ('. , ? !', '', '', '', '')},
    ]

_TWELVEKEYS_ALPHABET_SUPPORT_POPUP_KEY_DEFINITIONS = [
    {'name': '01', 'chars': ('@', '-', '_', '/', '1')},
    {'name': '02', 'chars': ('A', 'B', 'C', '', '2')},
    {'name': '03', 'chars': ('D', 'E', 'F', '', '3')},
    {'name': '04', 'chars': ('G', 'H', 'I', '', '4')},
    {'name': '05', 'chars': ('J', 'K', 'L', '', '5')},
    {'name': '06', 'chars': ('M', 'N', 'O', '', '6')},
    {'name': '07', 'chars': ('P', 'Q', 'R', 'S', '7')},
    {'name': '08', 'chars': ('T', 'U', 'V', '', '8')},
    {'name': '09', 'chars': ('W', 'X', 'Y', 'Z', '9')},
    # 10: "a-A" key: twelvekeys__alphabet__10.svg will be used.
    # Please check mozc/android/resources/res/xml/
    {'name': '11', 'chars': ('&apos;', '&quot;', ':', ';', '0')},
    {'name': '12', 'chars': ('.', ',', '?', '!', '')},
    ]

_TWELVEKEYS_KANA_KEYICON_KEY_DEFINITIONS = [
    {'name': 'exclamation_mark', 'char': '！'},
    {'name': 'ideographic_period', 'char': '。'},
    {'name': 'left_parenthesis', 'char': '('},
    {'name': 'prolonged_sound_mark', 'char': 'ー'},
    {'name': 'question_mark', 'char': '？'},
    {'name': 'right_parenthesis', 'char': ')'},
    {'name': 'horizontal_ellipsis', 'char': '…'},
    {'name': 'wave_dash', 'char': '〜'},

    {'name': 'a', 'char': 'あ'},
    {'name': 'i', 'char': 'い'},
    {'name': 'u', 'char': 'う'},
    {'name': 'e', 'char': 'え'},
    {'name': 'o', 'char': 'お'},
    {'name': 'an', 'char': 'あん'},
    {'name': 'in', 'char': 'いん'},
    {'name': 'un', 'char': 'うん'},
    {'name': 'en', 'char': 'えん'},
    {'name': 'on', 'char': 'おん'},
    {'name': 'axtu', 'char': 'あっ'},
    {'name': 'ixtu', 'char': 'いっ'},
    {'name': 'uxtu', 'char': 'うっ'},
    {'name': 'extu', 'char': 'えっ'},
    {'name': 'oxtu', 'char': 'おっ'},
    {'name': 'xi', 'char': 'ぃ'},
    {'name': 'xe', 'char': 'ぇ'},

    {'name': 'ka', 'char': 'か'},
    {'name': 'ki', 'char': 'き'},
    {'name': 'ku', 'char': 'く'},
    {'name': 'ke', 'char': 'け'},
    {'name': 'ko', 'char': 'こ'},

    {'name': 'sa', 'char': 'さ'},
    {'name': 'shi', 'char': 'し'},
    {'name': 'su', 'char': 'す'},
    {'name': 'se', 'char': 'せ'},
    {'name': 'so', 'char': 'そ'},

    {'name': 'ta', 'char': 'た'},
    {'name': 'chi', 'char': 'ち'},
    {'name': 'tsu', 'char': 'つ'},
    {'name': 'te', 'char': 'て'},
    {'name': 'to', 'char': 'と'},

    {'name': 'na', 'char': 'な'},
    {'name': 'ni', 'char': 'に'},
    {'name': 'nu', 'char': 'ぬ'},
    {'name': 'ne', 'char': 'ね'},
    {'name': 'no', 'char': 'の'},

    {'name': 'ha', 'char': 'は'},
    {'name': 'hi', 'char': 'ひ'},
    {'name': 'fu', 'char': 'ふ'},
    {'name': 'he', 'char': 'へ'},
    {'name': 'ho', 'char': 'ほ'},

    {'name': 'ma', 'char': 'ま'},
    {'name': 'mi', 'char': 'み'},
    {'name': 'mu', 'char': 'む'},
    {'name': 'me', 'char': 'め'},
    {'name': 'mo', 'char': 'も'},

    {'name': 'ya', 'char': 'や'},
    {'name': 'yu', 'char': 'ゆ'},
    {'name': 'yo', 'char': 'よ'},
    {'name': 'xya', 'char': 'ゃ'},
    {'name': 'xyu', 'char': 'ゅ'},
    {'name': 'xyo', 'char': 'ょ'},

    {'name': 'ra', 'char': 'ら'},
    {'name': 'ri', 'char': 'り'},
    {'name': 'ru', 'char': 'る'},
    {'name': 're', 'char': 'れ'},
    {'name': 'ro', 'char': 'ろ'},

    {'name': 'wa', 'char': 'わ'},
    {'name': 'wo', 'char': 'を'},
    {'name': 'nn', 'char': 'ん'},
    ]

_TWELVEKEYS_KANA_SUPPORT_KEY_DEFINITIONS = [
    {'name': '01',
     'chars': ('あ', 'い', 'う', 'え', 'お')},
    {'name': '02',
     'chars': ('か', 'き', 'く', 'け', 'こ')},
    {'name': '03',
     'chars': ('さ', 'し', 'す', 'せ', 'そ')},
    {'name': '04',
     'chars': ('た', 'ち', 'つ', 'て', 'と')},
    {'name': '05',
     'chars': ('な', 'に', 'ぬ', 'ね', 'の')},
    {'name': '06',
     'chars': ('は', 'ひ', 'ふ', 'へ', 'ほ')},
    {'name': '07',
     'chars': ('ま', 'み', 'む', 'め', 'も')},
    {'name': '08',
     'chars': ('や', '(', 'ゆ', ')', 'よ')},
    {'name': '09',
     'chars': ('ら', 'り', 'る', 'れ', 'ろ')},
    # 10: "大小" key(twelvekeys__kana__10.svg): Will be modified directly
    {'name': '11',
     'chars': ('わ', 'を', 'ん', 'ー', '〜')},
    # 12: Needs another template. Defined bellow.
    ]

_TWELVEKEYS_KANA_SUPPORT_12_KEY_DEFINITION = [
    {'name': '12',
     'chars': ('、', '。', '?', '!', '…')}
    ]

_TWELVEKEYS_KANA_SUPPORT_POPUP_KEY_DEFINITIONS = [
    {'name': '01', 'chars': ('あ', 'い', 'う', 'え', 'お')},
    {'name': '02', 'chars': ('か', 'き', 'く', 'け', 'こ')},
    {'name': '03', 'chars': ('さ', 'し', 'す', 'せ', 'そ')},
    {'name': '04', 'chars': ('た', 'ち', 'つ', 'て', 'と')},
    {'name': '05', 'chars': ('な', 'に', 'ぬ', 'ね', 'の')},
    {'name': '06', 'chars': ('は', 'ひ', 'ふ', 'へ', 'ほ')},
    {'name': '07', 'chars': ('ま', 'み', 'む', 'め', 'も')},
    {'name': '08', 'chars': ('や', '(', 'ゆ', ')', 'よ')},
    {'name': '09', 'chars': ('ら', 'り', 'る', 'れ', 'ろ')},
    # 10: "大小" key: twelvekeys__kana__10.svg will be used.
    # Please check mozc/android/resources/res/xml/
    {'name': '11', 'chars': ('わ', 'を', 'ん', 'ー', '〜')},
    {'name': '12', 'chars': ('、', '。', '?', '!', '…')},
    ]

_TWELVEKEYS_NUMBER_KEY_DEFINITIONS = [
    {'name': 'zero', 'char': '0'},
    {'name': 'one', 'char': '1'},
    {'name': 'two', 'char': '2'},
    {'name': 'three', 'char': '3'},
    {'name': 'four', 'char': '4'},
    {'name': 'five', 'char': '5'},
    {'name': 'six', 'char': '6'},
    {'name': 'seven', 'char': '7'},
    {'name': 'eight', 'char': '8'},
    {'name': 'nine', 'char': '9'},
    {'name': 'asterisk', 'char': '*'},
    {'name': 'number_sign', 'char': '#'},
    ]

_TWELVEKEYS_NUMBER_FUNCTION_KEY_DEFINITIONS = [
    {'name': 'comma', 'char': ','},
    {'name': 'equals_sign', 'char': '='},
    {'name': 'full_stop', 'char': '.'},
    {'name': 'hyphen_minus', 'char': '-'},
    {'name': 'plus_sign', 'char': '+'},
    {'name': 'solidus', 'char': '/'},
    ]

_TWELVEKEYS_OTHER_POPUP_DEFINITIONS = [
    # number keyboard
    {'name': 'low_line', 'char': '_'},
    {'name': 'quotation_mark', 'char': '"'},
    {'name': 'colon', 'char': ':'},
    {'name': 'semicolon', 'char': ';'},
    # 12keys and godan keyboard
    {'name': 'ideographic_comma', 'char': '、'},
    {'name': 'ideographic_full_stop', 'char': '。'},
    # godan keyboard
    {'name': 'fullwidth_solidus', 'char': '／'},
    {'name': 'fullwidth_colon', 'char': '：'},
    {'name': 'katakana_middle_dot', 'char': '・'},
    {'name': 'left_corner_bracket', 'char': '「'},
    {'name': 'right_corner_bracket', 'char': '」'},
    ]

def GenerateQwertyAlphabetKeyDefinitions():
  """Returns key definitions for qwerty alphabet keys."""
  key_definitions = []
  for i in range(ord('z') - ord('a') + 1):
    small = chr(i + ord('a'))
    capital = chr(i + ord('A'))
    support = _QWERTY_SUPPORT_KEY_MAPPING.get(small, '')
    key_definitions.append({
        'name': 'latin_capital_letter_' + small,
        'char': capital,
        'support': support})
    key_definitions.append({
        'name': 'latin_small_letter_' + small,
        'char': small,
        'support': support})
  return key_definitions


def GenerateTwelvekeysAlphabetKeyDefinitions():
  """Returns key definitions for qwerty alphabet keys."""
  key_definitions = []
  for i in range(ord('z') - ord('a') + 1):
    small = chr(i + ord('a'))
    capital = chr(i + ord('A'))
    key_definitions.append({
        'name': 'latin_' + small,
        'char': capital})
  return key_definitions


def ExpandAndWriteSupportKeys(template_file, output_file_basename, output_dir,
                              key_definition_list):
  """Generates and writes keyicons for support keys from template.

  We will expand keyicons for each flick directions.
  For example, we will generate XX_center, XX_left, XX_up, XX_right, XX_down
  adding to XX_release (default, no highlight).
  Expanded keyicons have highlighted key character corresponding to the flick
  direction.

  Args:
    template_file: Path for template file.
    output_file_basename: A string indicating the base name of the output.
    output_dir: Destination directory path.
    key_definition_list: Key definition list.
  """
  for key_definition in key_definition_list:
    assert len(key_definition['chars']) == len(_REPLACE_CHARS)

    with open(template_file) as template_f:
      template = template_f.read()

    for i, pattern in enumerate(_REPLACE_CHARS):
      template = template.replace(pattern, key_definition['chars'][i])

    for highlight_index, name_suffix in enumerate(_EXPAND_SUPPORT_SUFFIXES):
      if (highlight_index < len(key_definition['chars']) and
          not key_definition['chars'][highlight_index]):
        # We don't need highlighted key for non-existent support.
        continue

      keytop = template
      for i, id_pattern in enumerate(_REPLACE_ID_SUFFIXEX):
        default_id_suffix = ('' if 'id_suffixes' not in key_definition
                             else key_definition['id_suffixes'][i])
        id_suffix = default_id_suffix if i != highlight_index else '-highlight'
        keytop = keytop.replace(id_pattern, id_suffix)

      file_name = '%s__%s_%s.svg' % (output_file_basename,
                                     key_definition['name'],
                                     name_suffix)
      with open(os.path.join(output_dir, file_name), 'w') as write_f:
        write_f.write(keytop)


def WriteSupportPopupKeys(
    template_file, output_file_basename, output_dir, key_definition_list):
  """Generates and writes keyicons for support popup from template.

  Support popup keys may have 5 key characters to be replaced.

  Args:
    template_file: Path for template file.
    output_file_basename: A string indicating the base name of the output.
    output_dir: Destination directory path.
    key_definition_list: Key definition list.
  """
  for key_definition in key_definition_list:
    assert len(key_definition['chars']) == len(_REPLACE_CHARS)

    with open(template_file) as template_f:
      keytop = template_f.read()
    for i, pattern in enumerate(_REPLACE_CHARS):
      keytop = keytop.replace(pattern, key_definition['chars'][i])

    file_name = '%s__%s.svg' % (output_file_basename,
                                key_definition['name'])
    with open(os.path.join(output_dir, file_name), 'w') as write_f:
      write_f.write(keytop)


def WriteKeys(
    template_file, output_file_basename, output_dir, key_definition_list):
  """Generates and writes keyicons.

  Keys have 1 key character to be replaced.

  Args:
    template_file: Path for template file.
    output_file_basename: A string indicating the base name of the output.
    output_dir: Destination directory path.
    key_definition_list: Key definition list.
  """
  for key_definition in key_definition_list:
    assert key_definition['char']

    with open(template_file) as template_f:
      keytop = template_f.read()
    keytop = keytop.replace(_REPLACE_CHAR, key_definition['char'])
    keytop = keytop.replace(_REPLACE_CHAR_QWERTY_SUPPORT,
                            key_definition.get('support', ''))

    file_name = '%s__%s.svg' % (output_file_basename,
                                key_definition['name'])
    with open(os.path.join(output_dir, file_name), 'w') as write_f:
      write_f.write(keytop)


def Transform(input_dir, output_dir):
  """Transforms template .svg files into output_dir."""
  ExpandAndWriteSupportKeys(
      os.path.join(input_dir, 'godan__kana__support__template.svg'),
      'godan__kana__support',
      output_dir,
      _GODAN_KANA_SUPPORT_KEY_DEFINITIONS)

  ExpandAndWriteSupportKeys(
      os.path.join(input_dir, 'godan__kana__support__12__template.svg'),
      'godan__kana__support',
      output_dir,
      _GODAN_KANA_SUPPORT_12_KEY_DEFINITION)

  WriteSupportPopupKeys(
      os.path.join(input_dir, 'support__popup__template.svg'),
      'godan__kana__support__popup',
      output_dir,
      _GODAN_KANA_SUPPORT_POPUP_KEY_DEFINITIONS)

  qwerty_definitions = (
      _QWERTY_KEYICON_KEY_DEFINITIONS + GenerateQwertyAlphabetKeyDefinitions())

  WriteKeys(
      os.path.join(input_dir, 'qwerty__keyicon__template.svg'),
      'qwerty__keyicon',
      output_dir,
      qwerty_definitions)

  WriteKeys(
      os.path.join(input_dir, 'qwerty__popup__template.svg'),
      'qwerty__popup',
      output_dir,
      [x for x in qwerty_definitions if x.get('support', '')])

  WriteKeys(
      os.path.join(input_dir, 'twelvekeys__popup__template.svg'),
      'twelvekeys__popup',
      output_dir,
      GenerateTwelvekeysAlphabetKeyDefinitions())

  WriteKeys(
      os.path.join(input_dir, 'twelvekeys__alphabet__template.svg'),
      'twelvekeys__alphabet',
      output_dir,
      _TWELVEKEYS_ALPHABET_KEY_DEFINITIONS)

  WriteKeys(
      os.path.join(input_dir, 'twelvekeys__alphabet__popup__template.svg'),
      'twelvekeys__alphabet__popup',
      output_dir,
      _TWELVEKEYS_ALPHABET_KEY_DEFINITIONS)

  ExpandAndWriteSupportKeys(
      os.path.join(input_dir,
                   'twelvekeys__alphabet__support__template.svg'),
      'twelvekeys__alphabet__support',
      output_dir,
      _TWELVEKEYS_ALPHABET_SUPPORT_KEY_DEFINITIONS)

  WriteSupportPopupKeys(
      os.path.join(input_dir, 'support__popup__template.svg'),
      'twelvekeys__alphabet__support__popup',
      output_dir,
      _TWELVEKEYS_ALPHABET_SUPPORT_POPUP_KEY_DEFINITIONS)

  WriteKeys(
      os.path.join(input_dir, 'twelvekeys__kana__keyicon__template.svg'),
      'twelvekeys__kana__keyicon',
      output_dir,
      _TWELVEKEYS_KANA_KEYICON_KEY_DEFINITIONS)

  WriteKeys(
      os.path.join(input_dir, 'twelvekeys__popup__template.svg'),
      'twelvekeys__popup',
      output_dir,
      _TWELVEKEYS_KANA_KEYICON_KEY_DEFINITIONS)

  ExpandAndWriteSupportKeys(
      os.path.join(input_dir, 'twelvekeys__kana__support__template.svg'),
      'twelvekeys__kana__support',
      output_dir,
      _TWELVEKEYS_KANA_SUPPORT_KEY_DEFINITIONS)

  ExpandAndWriteSupportKeys(
      os.path.join(input_dir,
                   'twelvekeys__kana__support__12__template.svg'),
      'twelvekeys__kana__support',
      output_dir,
      _TWELVEKEYS_KANA_SUPPORT_12_KEY_DEFINITION)

  WriteSupportPopupKeys(
      os.path.join(input_dir, 'support__popup__template.svg'),
      'twelvekeys__kana__support__popup',
      output_dir,
      _TWELVEKEYS_KANA_SUPPORT_POPUP_KEY_DEFINITIONS)

  WriteKeys(
      os.path.join(input_dir, 'twelvekeys__number__template.svg'),
      'twelvekeys__number',
      output_dir,
      _TWELVEKEYS_NUMBER_KEY_DEFINITIONS)

  WriteKeys(
      os.path.join(input_dir, 'twelvekeys__popup__template.svg'),
      'twelvekeys__popup',
      output_dir,
      _TWELVEKEYS_NUMBER_KEY_DEFINITIONS)

  WriteKeys(
      os.path.join(input_dir,
                   'twelvekeys__number__function__template.svg'),
      'twelvekeys__number__function',
      output_dir,
      _TWELVEKEYS_NUMBER_FUNCTION_KEY_DEFINITIONS)

  WriteKeys(
      os.path.join(input_dir,
                   'twelvekeys__popup__template.svg'),
      'twelvekeys__popup',
      output_dir,
      _TWELVEKEYS_NUMBER_FUNCTION_KEY_DEFINITIONS)

  WriteKeys(
      os.path.join(input_dir, 'twelvekeys__popup__template.svg'),
      'twelvekeys__popup',
      output_dir,
      _TWELVEKEYS_OTHER_POPUP_DEFINITIONS)


def ParseOptions():
  parser = optparse.OptionParser()
  parser.add_option('--input_dir', dest='input_dir',
                    help='Input directory containing source svg files.')
  parser.add_option('--output_dir', dest='output_dir',
                    help='Output directory for generated svg files.')
  parser.add_option('--output_zip', dest='output_zip',
                    help='Output zip file for generated svg files.')
  return parser.parse_args()[0]


def main():
  options = ParseOptions()
  if not options.input_dir:
    logging.fatal('You must specify --input_dir.')
    return
  if not options.output_dir and not options.output_zip:
    logging.fatal('You must specify --output_dir and/or --output_zip.')
    return
  if options.output_dir:
    output_dir = options.output_dir
    if not os.path.exists(output_dir):
      os.makedirs(output_dir)
  else:
    output_dir = tempfile.mkdtemp()
  Transform(options.input_dir, output_dir)
  if options.output_zip:
    with zipfile.ZipFile(options.output_zip, 'w', zipfile.ZIP_DEFLATED) as z:
      for f in os.listdir(output_dir):
        if os.path.isdir(f):
          raise IOError('Restriction: output_dir should not include directory')
        z.write(os.path.join(output_dir, f), f)

if __name__ == '__main__':
  main()
