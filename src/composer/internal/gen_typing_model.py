# -*- coding: utf-8 -*-
# Copyright 2010-2014, Google Inc.
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

"""Converts a typing model file to C++ code.

Usage:
  $ gen_typing_model.py model.tsv > output.h
"""

__author__ = "noriyukit"

import bisect
import codecs
import collections
import optparse
import struct

UNDEFINED_COST = -1
MAX_UINT16 = struct.unpack('H', '\xFF\xFF')[0]
MAX_UINT8 = struct.unpack('B', '\xFF')[0]


def ParseArgs():
  """Parses command line options and returns them."""
  parser = optparse.OptionParser()
  parser.add_option('--input_path', dest='input_path',
                    default='typing_model.tsv',
                    help='Input file path')
  parser.add_option('--variable_name', dest='variable_name',
                    default='typingmodel',
                    help='Suffix of created variable name.')
  parser.add_option('--output_path', dest='output_path',
                    default='/tmp/typing_model.h',
                    help='Output file path.')
  return parser.parse_args()[0]


def GetUniqueCharacters(keys):
  unique_chars = set()
  for key in keys:
    unique_chars.update(list(key))
  return sorted(list(unique_chars))


def GetIndexFromKey(unique_characters, key):
  # The index is like the result of atoi function.
  # If 'abcd' is given as unique_characters, then
  # following mapping is assumed.
  # a->1, b->2, c->3, d->4. The radix is 5 (including implicit digit 0).
  # So if key is 'abd', then the index is
  # 1*5^2 + 2*5^1 + 3*5^0 = 38
  radix = len(unique_characters) + 1
  index = 0
  for char in key:
    index = index * radix + unique_characters.index(char) + 1
  return index


def GetMappingTable(values, mapping_table_size):
  """Creates mapping table.

  Cost value needs 16bit field but the values are so many that
  directly storeing them increses .so's size.
  Thus we'd store the values in 8bit values, which are
  index of cost-mapping-table.
  Args:
    values: Raw cost table.
    mapping_table_size: The size of mapping table. Typically 256.
  Returns:
    Mapping table (list). The last entry is UNDEFINED_COST.
  """
  sorted_values = list(sorted(set(values)))
  mapping_table = sorted_values[0]
  mapping_table_size_without_special_value = mapping_table_size - 1
  span = len(sorted_values) / (mapping_table_size_without_special_value - 1)
  mapping_table = [sorted_values[i * span]
                   for i
                   in range(0, mapping_table_size_without_special_value - 1)]
  mapping_table.append(sorted_values[-1])
  mapping_table.append(UNDEFINED_COST)
  return mapping_table


def GetNearestMappingTableIndex(mapping_table, value):
  """Gets the index of mapping_table.

  Args:
    mapping_table: mapping table, created by GetMappingTable.
    value: the value of which index we need.
  Returns:
    Index value fo mapping_table. mapping_table[index] is the nearest value
    of given value.
  """
  if value == UNDEFINED_COST:
    return len(mapping_table) - 1
  found_left = bisect.bisect_left(mapping_table, value,
                                  0, len(mapping_table) - 1)
  if mapping_table[found_left] == value or found_left == 0:
    return found_left
  if found_left >= len(mapping_table):
    return len(mapping_table) - 1
  found_value = mapping_table[found_left]
  left_value = mapping_table[found_left - 1]
  if abs(left_value - value) > abs(found_value - value):
    return found_left
  else:
    return found_left - 1


def GetValueTable(unique_characters, mapping_table, dictionary):
  result = []
  for key, value in dictionary.iteritems():
    index = GetIndexFromKey(unique_characters, key)
    while len(result) <= index:
      result.append(len(mapping_table) - 1)
    nearest_mapping_index = GetNearestMappingTableIndex(mapping_table, value)
    result[index] = nearest_mapping_index
  return result


def WriteResult(romaji_transition_cost, output_path, variable_name):
  unique_characters = GetUniqueCharacters(romaji_transition_cost.keys())
  mapping_table = GetMappingTable(romaji_transition_cost.values(),
                                  MAX_UINT8 + 1)
  value_list = GetValueTable(unique_characters, mapping_table,
                             romaji_transition_cost)
  quoted_unique_characters = ''.join(
      [r'\x%X' % ord(c) for c in unique_characters])
  with open(output_path, 'w') as out_file:
    out_file.write('const size_t kKeyCharactersSize_%s = %d;\n' %
                   (variable_name, len(unique_characters)))
    out_file.write('const char* kKeyCharacters_%s = "%s";\n' %
                   (variable_name, ''.join(quoted_unique_characters)))
    out_file.write('const size_t kCostTableSize_%s = %d;\n' %
                   (variable_name, len(value_list)))
    out_file.write('const uint8 kCostTable_%s[] = {\n' %
                   variable_name)
    for value in value_list:
      out_file.write('%d,\n' % value)
    out_file.write('};\n')
    out_file.write('const int32 kCostMappingTable_%s[] = {\n' %
                   variable_name)
    for value in mapping_table:
      out_file.write('%d,\n' % value)
    out_file.write('};\n')


def main():
  options = ParseArgs()
  # Read cost of unigram and trigram from argv[1]. Namely:
  #   - unigram['x'] = -500 * log(P(x))
  #   - trigram['vw']['x'] = -500 * log(P(x | 'vw'))
  unigram = {}
  trigram = collections.defaultdict(dict)
  for line in codecs.open(options.input_path, 'r', encoding='utf-8'):
    line = line.rstrip()
    ngram, cost = line.split('\t')
    cost = int(cost)
    if len(ngram) == 1:
      unigram[ngram] = cost
    else:
      trigram[ngram[:-1]][ngram[-1]] = cost

  # Calculate ngram-related cost for each 'vw' and 'x':
  #     -500 * log( P('x' | 'vw') / P('x') )
  #   = trigram['vw']['x'] - unigram['x']
  min_cost = 1e+9
  romaji_transition_cost = {}
  for prev in trigram:
    for current in trigram[prev]:
      cost = trigram[prev][current] - unigram[current]
      romaji_transition_cost[prev + current] = cost
      if cost < min_cost:
        min_cost = cost

  # The constant bias term is uniformly added to keep cost nonnegative (for
  # decoding by dynamic programming). Note that adding any constant doesn't
  # affect the ranking.
  for ngram in romaji_transition_cost:
    adjusted_cost = romaji_transition_cost[ngram] - min_cost
    # We use unsigned short to store cost value so range check is needed.
    romaji_transition_cost[ngram] = adjusted_cost

  WriteResult(romaji_transition_cost, options.output_path,
              options.variable_name)


if __name__ == '__main__':
  main()
