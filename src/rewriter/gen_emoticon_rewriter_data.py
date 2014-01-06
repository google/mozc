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

"""Converter from emoticon data to embedded_dictionary.

Usage:
  python gen_emoticon_rewriter_data.py --input=input.tsv --output=output_header
"""

__author__ = "hidehiko"

from collections import defaultdict
import logging
import optparse
import re
import sys
from rewriter import embedded_dictionary_compiler


def ParseOptions():
  parser = optparse.OptionParser()
  parser.add_option('--input', dest='input', help='emoticon dictionary file')
  parser.add_option('--output', dest='output', help='output header file')
  return parser.parse_args()[0]


def GetDescription(key_list, key_count):
  """Generates a description from readings.

  We simply add 1) the most general reading and 2) the most specific reading.
  1) and 2) are simply approximated by checking the frequency of the readings.

  Args:
    key_list: a list of key strings.
    key_count: a dictionary of key to the number of key's occurence in the data
      file.
  Returns:
    the description string.
  """
  if len(key_list) == 1:
    return key_list[0]

  sorted_key_list = sorted(key_list, key=lambda key: (key_count[key], key))
  return '%s %s' % (sorted_key_list[-1], sorted_key_list[0])


def ReadEmoticonTsv(stream):
  """Read lines from stream to a Token dictionary for a embedded dictionary."""
  # Skip the first line (header).
  stream.next()

  data = []
  key_count = defaultdict(int)
  for line in stream:
    # The file format is:
    # value <tab> readings(space delimitered)
    field_list = line.rstrip('\n').split('\t')
    # Check the size of columns.
    if len(field_list) < 2:
      logging.critical('format error: %s', line)
      sys.exit(1)
    if len(field_list) > 3:
      logging.warning('ignore extra columns: %s', line)

    # \xE3\x80\x80 is full width space
    key_list = re.split(r'(?: |\xE3\x80\x80)+', field_list[1].strip())
    data.append((field_list[0], key_list))
    for key in key_list:
      key_count[key] += 1

  input_data = defaultdict(list)
  cost = 10
  for value, key_list in data:
    input_value = value
    if input_value == "":
      input_value = None
    description = GetDescription(key_list, key_count)
    if description == "":
      description = None

    for key in key_list:
      input_data[key].append(embedded_dictionary_compiler.Token(
          key, input_value, description, None, 0, 0, cost))
    cost += 10

  return input_data


def main():
  options = ParseOptions()
  with open(options.input, 'r') as input_stream:
    input_data = ReadEmoticonTsv(input_stream)

  with open(options.output, 'w') as output_stream:
    embedded_dictionary_compiler.Compile(
        'EmoticonData', input_data, output_stream)


if __name__ == '__main__':
  main()
