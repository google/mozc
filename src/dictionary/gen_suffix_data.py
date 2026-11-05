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

"""Generates empty placeholders of the deprecated suffix dictionary data.

The suffix dictionary is no longer used (b/579137069), but older Mozc binaries
still require these entries in the data set. Writes empty serialized string
arrays (4-byte zero size field) for keys and values, and an empty token array.
"""

import optparse

from build_tools import serialized_string_array_builder


def _ParseOptions():
  parser = optparse.OptionParser()
  parser.add_option('--input', dest='input', help='Input suffix file (unused)')
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

  # Write empty serialized string arrays for keys and values.
  serialized_string_array_builder.SerializeToFile([], opts.output_key_array)
  serialized_string_array_builder.SerializeToFile([], opts.output_value_array)

  # Write an empty token array.
  with open(opts.output_token_array, 'wb') as f:
    pass


if __name__ == '__main__':
  main()
