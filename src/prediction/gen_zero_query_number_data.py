# -*- coding: utf-8 -*-
# Copyright 2010-2015, Google Inc.
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

"""Generate zero query data file for number suffixes."""

from collections import defaultdict
import optparse
from prediction import codegen_util_for_zero_query as util


_VAR_NAME_FOR_HEADER = 'kZeroQueryNum'


def ReadZeroQueryNumberData(input_stream):
  """Reads zero query number data from stream and returns zero query data."""
  zero_query_dict = defaultdict(list)

  for line in input_stream:
    if line.startswith('#'):
      continue
    line = line.rstrip('\r\n')
    if not line:
      continue

    tokens = line.split('\t')
    key = tokens[0]
    values = tokens[1].split(',')

    for value in values:
      zero_query_dict[key].append(
          util.ZeroQueryEntry(util.ZERO_QUERY_TYPE_NUMBER_SUFFIX,
                              value, util.EMOJI_TYPE_NONE, 0))
  return zero_query_dict


def ParseOption():
  """Parses command line options."""
  parser = optparse.OptionParser()
  parser.add_option('--input', dest='input', help='Input file path')
  parser.add_option('--output', dest='output', help='Output file path')
  return parser.parse_args()[0]


def main():
  options = ParseOption()
  with open(options.input, 'r') as input_stream:
    zero_query_dict = ReadZeroQueryNumberData(input_stream)

  with open(options.output, 'w') as output_stream:
    util.WriteHeaderFileForZeroQuery(
        zero_query_dict, options.output, _VAR_NAME_FOR_HEADER, output_stream)


if __name__ == '__main__':
  main()
