# -*- coding: utf-8 -*-
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

"""Single kanji dictionary generator.

How to run this script:
gen_single_kanji_rewriter_data.py \
  --input=input.tsv \
  --min_prob=0.0 \
  --output=single_kanji_data.h \
  --id_file=id.def \
  --special_pos_file=special_pos.def \
  --cforms_file=cforms.def \
  --user_pos_file=user_pos.def
"""

__author__ = "hidehiko"

from collections import defaultdict
import math
import optparse
from build_tools import code_generator_util
from dictionary import pos_util
from rewriter import embedded_dictionary_compiler

RENDAKU_DEMOTION_FACTOR = 0.01
RENDAKU_MAP = {
    'が': 'か',
    'ぎ': 'き',
    'ぐ': 'く',
    'げ': 'け',
    'ご': 'こ',
    'ざ': 'さ',
    'じ': 'し',
    'ず': 'す',
    'ぜ': 'せ',
    'ぞ': 'そ',
    'だ': 'た',
    'ぢ': 'ち',
    'づ': 'つ',
    'で': 'て',
    'ど': 'と',
    'ば': 'は',
    'び': 'ひ',
    'ぶ': 'ふ',
    'べ': 'へ',
    'ぼ': 'ほ',
}


def _NormalizeRendaku(s):
  # We do not normalize one-kana character string.
  if len(s) > 3:
    replace = RENDAKU_MAP.get(s[:3])
    if replace:
      return replace + s[3:]
  return s


def ReadSingleKanji(user_pos, min_probability, stream):
  """Parses single kanji dictionary data from stream."""
  stream = code_generator_util.ParseColumnStream(stream)
  dictionary = {}
  for columns in stream:
    value, key, probability, frequency = columns[:4]
    probability = float(probability)
    frequency = float(frequency)
    # Filter kanji with minor reading.
    if probability > min_probability and frequency > 0:
      description = columns[6] if len(columns) >= 7 else ''
      dictionary[(key, value)] = (frequency, description)

  # Rendaku (sequential voicing) Normalization.
  update = []
  for (key, value), (frequency, description) in dictionary.iteritems():
    normalized_key = _NormalizeRendaku(key)
    if normalized_key != key and (normalized_key, value) in dictionary:
      update.append((key, value))
  for key, value in update:
    frequency, description = dictionary[(key, value)]
    frequency *= RENDAKU_DEMOTION_FACTOR
    if frequency <= 0:
      raise RuntimeError('frequency must be positive: %s, %s' % (key, value))
    dictionary[(key, value)] = (frequency, description)

  # Format to the map of token list for embedded_dictionary_compiler.
  entry_map = defaultdict(list)
  total_frequency = 0
  for (key, value), (frequency, description) in dictionary.iteritems():
    entry_map[key].append((frequency, value, description))
    total_frequency += frequency

  if total_frequency <= 0:
    raise RuntimeError('Total frequency must be positive')

  noun_id = user_pos.GetPosId('名詞')
  token_map = {}
  for key, entry_list in sorted(entry_map.items()):
    entry_list.sort(reverse=True)
    token_list = []
    for frequency, value, description in entry_list:
      # The max cost of single kanji data is 32765, in case something
      # bad behavior happens.
      cost = min(int(-500 * math.log(frequency / total_frequency)), 32765)
      if cost <= 0:
        raise RuntimeError('Cost must be positive: %s, %s' % (key, value))
      token_list.append(embedded_dictionary_compiler.Token(
          key, value, description, '', noun_id, noun_id, cost))
    token_map[key] = token_list

  return token_map


def _ParseOptions():
  parser = optparse.OptionParser()

  parser.add_option('--input', dest='input',
                    help='Single kanji dictionary file')
  parser.add_option('--min_prob', dest='min_prob', type='float',
                    default=0.1, help='Minimum prob threshold.')
  parser.add_option('--output', dest='output', help='Output header file.')

  parser.add_option('--id_file', dest='id_file', help='Path to id.def.')
  parser.add_option('--special_pos_file', dest='special_pos_file',
                    help='Path to special_pos.def')
  parser.add_option('--cforms_file', dest='cforms_file',
                    help='Path to cforms.def')
  parser.add_option('--user_pos_file', dest='user_pos_file',
                    help='Path to user_pos,def')

  return parser.parse_args()[0]


def main():
  options = _ParseOptions()

  # Construct user pos data.
  pos_database = pos_util.PosDataBase()
  pos_database.Parse(options.id_file, options.special_pos_file)
  inflection_map = pos_util.InflectionMap()
  inflection_map.Parse(options.cforms_file)
  user_pos = pos_util.UserPos(pos_database, inflection_map)
  user_pos.Parse(options.user_pos_file)

  with open(options.input, 'r') as input_stream:
    input_data = ReadSingleKanji(user_pos, options.min_prob, input_stream)

  with open(options.output, 'w') as output_stream:
    embedded_dictionary_compiler.Compile(
        'SingleKanjiData', input_data, output_stream)


if __name__ == '__main__':
  main()
