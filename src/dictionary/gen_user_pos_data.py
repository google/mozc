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

"""Utility to generate User POS binary data."""

import optparse
import struct

from build_tools import serialized_string_array_builder
from dictionary import pos_util


def ToString(s):
  return s or ''


def OutputUserPosData(user_pos_data, output_token_array, output_string_array):
  string_index = {}
  for user_pos, conjugation_list in user_pos_data:
    string_index[ToString(user_pos)] = 0
    for value_suffix, key_suffix, _ in conjugation_list:
      string_index[ToString(value_suffix)] = 0
      string_index[ToString(key_suffix)] = 0
  for index, s in enumerate(sorted(string_index)):
    string_index[s] = index

  with open(output_token_array, 'wb') as f:
    for user_pos, conjugation_list in sorted(user_pos_data):
      user_pos_index = string_index[ToString(user_pos)]
      for value_suffix, key_suffix, conjugation_id in conjugation_list:
        # One entry is serialized to 8 byte (four uint16_t components).
        f.write(struct.pack('<H', user_pos_index))
        f.write(struct.pack('<H', string_index[ToString(value_suffix)]))
        f.write(struct.pack('<H', string_index[ToString(key_suffix)]))
        f.write(struct.pack('<H', conjugation_id))

  serialized_string_array_builder.SerializeToFile(
      sorted(string_index.keys()), output_string_array
  )


def ParseOptions():
  parser = optparse.OptionParser()
  # Input: id.def, special_pos.def, user_pos.def, cforms.def
  parser.add_option('--id_file', dest='id_file', help='Path to id.def.')
  parser.add_option(
      '--special_pos_file',
      dest='special_pos_file',
      help='Path to special_pos.def',
  )
  parser.add_option(
      '--cforms_file', dest='cforms_file', help='Path to cforms.def'
  )
  parser.add_option(
      '--user_pos_file', dest='user_pos_file', help='Path to user_pos,def'
  )
  parser.add_option(
      '--output_token_array',
      dest='output_token_array',
      help='Path to output token array binary data',
  )
  parser.add_option(
      '--output_string_array',
      dest='output_string_array',
      help='Path to output string array data',
  )
  parser.add_option(
      '--output_pos_list',
      dest='output_pos_list',
      help='Path to output POS list binary file',
  )
  return parser.parse_args()[0]


def main():
  options = ParseOptions()
  pos_database = pos_util.PosDataBase()
  pos_database.Parse(options.id_file, options.special_pos_file)
  inflection_map = pos_util.InflectionMap()
  inflection_map.Parse(options.cforms_file)
  user_pos = pos_util.UserPos(pos_database, inflection_map)
  user_pos.Parse(options.user_pos_file)

  OutputUserPosData(
      user_pos.data, options.output_token_array, options.output_string_array
  )

  if options.output_pos_list:
    serialized_string_array_builder.SerializeToFile(
        [pos for (pos, _) in user_pos.data], options.output_pos_list
    )


if __name__ == '__main__':
  main()
