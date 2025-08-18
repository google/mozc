# -*- coding: utf-8 -*-
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

"""Generates touch event's data stream file from csv file.

Typical usage is:
python android/gen_touch_event_stat.py --output_dir android/assets \
--stats_data data/typing/touch_event_stats.csv \
--collected_keyboards android/collected_keyboards.csv
"""

from collections import defaultdict
import csv
import optparse
import os
import struct
import urllib.parse


def ReadCollectedKeyboards(stream):
  """Parses collected keyboard list (CSV).

  Args:
    stream: An input stream.

  Returns:
    A frozeset of tuples (base_name, major, minor, revision).
  """
  result = set()
  reader = csv.DictReader(stream)
  for row in reader:
    result.add((row['base_name'], row['major'], row['minor'], row['revision']))
  return frozenset(result)


def ReadData(stream, collected_keyboards):
  """Reads CSV file of the stats and returns the result as dictionary.

  Args:
    stream: An input stream of the file.
    collected_keyboards: A collection of the keyboard spec tuple (base_name,
      major, minor, revision) of which data is used.

  Returns:
    A dictionary: base_name_orientation -> source_id -> stats_type ->
                  (cumulative_average, count)
  """
  reader = csv.DictReader(stream)
  stats = defaultdict(lambda: defaultdict(lambda: defaultdict(lambda: (0, 0))))
  for row in reader:
    key = (row['base_name'], row['major'], row['minor'], row['revision'])
    if not key in collected_keyboards:
      continue
    base_name_orientation = (row['base_name'], row['orientation'])
    source_id = int(row['source_id'])
    stats_type = row['stats_type']
    (cumulative_average, count) = stats[base_name_orientation][source_id][
        stats_type
    ]
    new_value = (
        cumulative_average + int(row['sum']),
        count + int(row['count']),
    )
    stats[base_name_orientation][source_id][stats_type] = new_value
  return stats


def WriteKeyboardData(keyboard_value, stream):
  """Writes serialized binary of the stats for a keyboard.

  Args:
    keyboard_value: A dictionary from souce_id to source_value. source_value is
      a dictionary from stats_type to (cumulative_average, count).
    stream: An output stream.
  """

  def GetAverage(source_value, stats_type):
    (cumulative_average, count) = source_value[stats_type]
    if count == 0:
      return 0
    # Cumulative average values are multiplied by 10^7
    # when usage stats are sent to the log server.
    # Here we get original value by dividing the average value.
    return cumulative_average / 1e7 / count

  # c.f. usage_stats/usage_stats_uploader.cc
  keys = ('sxa', 'sya', 'sxv', 'syv', 'dxa', 'dya', 'dxv', 'dyv')
  stream.write(struct.pack('>i', len(keyboard_value)))
  for source_id, source_value in keyboard_value.items():
    stream.write(struct.pack('>i', source_id))
    # Note that we are calculating
    # "Average of average" and "Average of variance".
    # Such values are theoritically meaningless but we collect
    # only restricted value to avoid from privacy issue
    # so we have to use them instead.
    stream.write(
        struct.pack(
            '>ffffffff', *[GetAverage(source_value, key) for key in keys]
        )
    )


def WriteData(stats, output_dir):
  for base_name_orientation in stats.keys():
    with open(
        os.path.join(
            output_dir,
            '%s_%s.touch_stats'
            % (
                urllib.parse.unquote(base_name_orientation[0]),
                base_name_orientation[1],
            ),
        ),
        'wb',
    ) as stream:
      WriteKeyboardData(stats[base_name_orientation], stream)


def ParseOptions():
  parser = optparse.OptionParser()
  parser.add_option(
      '--collected_keyboards',
      dest='collected_keyboards',
      help='Keyboards to be collected',
  )
  parser.add_option(
      '--stats_data', dest='stats_data', help='Path to touch_event_stats.csv'
  )
  parser.add_option('--output_dir', dest='output_dir', help='Output file name')
  return parser.parse_args()[0]


def main():
  options = ParseOptions()
  with open(options.collected_keyboards) as stream:
    collected_keyboards = ReadCollectedKeyboards(stream)
  with open(options.stats_data) as stream:
    stats = ReadData(stream, collected_keyboards)
  output_dir = options.output_dir
  if not os.path.exists(output_dir):
    os.makedirs(output_dir)
  WriteData(stats, output_dir)


if __name__ == '__main__':
  main()
