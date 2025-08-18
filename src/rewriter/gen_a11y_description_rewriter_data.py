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

"""A11y description dictionary generator.

How to run this script:
gen_a11y_description_rewriter_data.py
  --input=japanese_phonetic_reading.tsv
  --output_token_array=output_token_array
  --output_string_array=output_token_array
"""

import argparse
import codecs
import struct
from typing import List

from build_tools import code_generator_util
from build_tools import serialized_string_array_builder

KeyValuePair = List[str]


def ReadJapanesePhoneticReading(file: str) -> List[KeyValuePair]:
  with codecs.open(file, 'r', encoding='utf-8') as f:
    f = code_generator_util.SkipLineComment(f)
    f = code_generator_util.ParseColumnStream(f, num_column=2, delimiter='\t')
    outputs = list(f)
    outputs.sort(key=lambda x: x[0])
    return outputs


def WriteOutput(
    key_value_pairs: List[KeyValuePair],
    token_array_file: str,
    string_array_file: str,
):
  """Output token and string arrays to files.

  Args:
    key_value_pairs: Pairs of character and the phonetic reading.
    token_array_file: Token array file to consist SerializedDictionary.
    string_array_file: String array file to consist SerializedDictionary.
  """
  strings = []
  with open(token_array_file, 'wb') as f:
    for index, (key, value) in enumerate(key_value_pairs):
      f.write(struct.pack('<I', 2 * index))
      f.write(struct.pack('<I', 2 * index + 1))
      f.write(struct.pack('<I', 0))
      f.write(struct.pack('<I', 0))
      f.write(struct.pack('<H', 0))
      f.write(struct.pack('<H', 0))
      f.write(struct.pack('<H', 0))
      f.write(struct.pack('<H', 0))
      strings.append(key)
      strings.append(value)
  serialized_string_array_builder.SerializeToFile(strings, string_array_file)


def ParseArgs() -> argparse.Namespace:
  """Parse command line args.

  Returns:
    Parsed command line args.
  """
  parser = argparse.ArgumentParser()

  parser.add_argument(
      '--input', dest='input', help='Japanese phonetic reading file'
  )
  parser.add_argument(
      '--output_token_array',
      dest='output_token_array',
      help='Output token array file.',
  )
  parser.add_argument(
      '--output_string_array',
      dest='output_string_array',
      help='Output string array file.',
  )
  return parser.parse_args()


def main() -> None:
  args = ParseArgs()
  key_value_pairs = ReadJapanesePhoneticReading(args.input)
  WriteOutput(
      key_value_pairs, args.output_token_array, args.output_string_array
  )


if __name__ == '__main__':
  main()
