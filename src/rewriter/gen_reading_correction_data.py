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

"""Converter of reading correction data from TSV to binary format.

Usage:
  python gen_reading_correction_data.py
    --input=input.tsv
    --output_value_array=value_array.data
    --output_error_array=error_array.data
    --output_correction_array=correction_array.data
"""


import codecs
import optparse

from build_tools import code_generator_util
from build_tools import serialized_string_array_builder


def ParseOptions():
  """Parse command line options."""
  parser = optparse.OptionParser()
  parser.add_option('--input', dest='input', help='Input TSV file path.')
  parser.add_option(
      '--output_value_array',
      dest='output_value_array',
      help='Output serialized string array for values.',
  )
  parser.add_option(
      '--output_error_array',
      dest='output_error_array',
      help='Output serialized string array for errors.',
  )
  parser.add_option(
      '--output_correction_array',
      dest='output_correction_array',
      help='Output serialized string array for corrections.',
  )
  return parser.parse_args()[0]


def WriteData(
    input_path,
    output_value_array_path,
    output_error_array_path,
    output_correction_array_path,
):
  outputs = []
  with codecs.open(input_path, 'r', encoding='utf-8') as input_stream:
    input_stream = code_generator_util.SkipLineComment(input_stream)
    input_stream = code_generator_util.ParseColumnStream(
        input_stream, num_column=3
    )
    # ex. (value, error, correction) = ("雰囲気", "ふいんき", "ふんいき")
    for value, error, correction in input_stream:
      outputs.append([value, error, correction])

  # In order to lookup the entries via |error| with binary search,
  # sort outputs here.
  outputs.sort(key=lambda x: (x[1], x[0]))

  serialized_string_array_builder.SerializeToFile(
      [value for (value, _, _) in outputs], output_value_array_path
  )
  serialized_string_array_builder.SerializeToFile(
      [error for (_, error, _) in outputs], output_error_array_path
  )
  serialized_string_array_builder.SerializeToFile(
      [correction for (_, _, correction) in outputs],
      output_correction_array_path,
  )


def main():
  options = ParseOptions()
  WriteData(
      options.input,
      options.output_value_array,
      options.output_error_array,
      options.output_correction_array,
  )


if __name__ == '__main__':
  main()
