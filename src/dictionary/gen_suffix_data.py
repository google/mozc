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

import codecs
import optparse
import re
import struct

from build_tools import serialized_string_array_builder


def _ParseOptions():
  parser = optparse.OptionParser()
  parser.add_option('--input', dest='input', help='Input suffix file')
  parser.add_option(
      '--output_key_array',
      dest='output_key_array',
      help='Output serialized string array for keys',
  )
  parser.add_option(
      '--output_value_array',
      dest='output_value_array',
      help='Output serialized string array for values',
  )
  parser.add_option(
      '--output_token_array',
      dest='output_token_array',
      help='Output uint32_t array for lid, rid and cost.',
  )
  return parser.parse_args()[0]


def main():
  opts = _ParseOptions()

  # Remove noisy and unuseful words.
  invalid_re = re.compile(r'^[→↓↑→─あいうえおぁぃぅぇぉつっょ]$')

  result = []
  with codecs.open(opts.input, 'r', encoding='utf-8') as stream:
    for line in stream:
      line = line.rstrip('\r\n')
      fields = line.split('\t')
      key = fields[0]
      lid = int(fields[1])
      rid = int(fields[2])
      cost = int(fields[3])
      value = fields[4]

      if invalid_re.match(value):
        continue

      if key == value:
        value = ''

      result.append((key, value, lid, rid, cost))

  # Sort entries in ascending order of key.
  result.sort(key=lambda e: e[0])

  # Write keys to serialized string array.
  serialized_string_array_builder.SerializeToFile(
      list(entry[0] for entry in result), opts.output_key_array
  )

  # Write values to serialized string array.
  serialized_string_array_builder.SerializeToFile(
      list(entry[1] for entry in result), opts.output_value_array
  )

  # Write a sequence of (lid, rid, cost) to uint32_t array:
  #   {lid[0], rid[0], cost[0], lid[1], rid[1], cost[1], ...}
  # So the final array has 3 * len(result) elements.
  with open(opts.output_token_array, 'wb') as f:
    for _, _, lid, rid, cost in result:
      f.write(struct.pack('<I', lid))
      f.write(struct.pack('<I', rid))
      f.write(struct.pack('<I', cost))


if __name__ == '__main__':
  main()
