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

"""Converter of reading correction data from TSV to C++ code.

Usage:
  python gen_reading_correction_data.py --input=input.tsv --output=output.h
"""

__author__ = "komatsu"

import logging
import optparse
from build_tools import code_generator_util


def ParseOptions():
  """Parse command line options."""
  parser = optparse.OptionParser()
  parser.add_option('--input', dest='input', help='input TSV file path.')
  parser.add_option('--output', dest='output', help='output .h file path.')
  return parser.parse_args()[0]


def WriteData(input_path, output_path):
  with open(output_path, 'w') as output_stream:
    output_stream.write('static const ReadingCorrectionItem '
                        'kReadingCorrections[] = {\n')

    with open(input_path) as input_stream:
      input_stream = code_generator_util.SkipLineComment(input_stream)
      input_stream = code_generator_util.ParseColumnStream(input_stream,
                                                           num_column=3)
      # ex. (value, error, correction) = ("雰囲気", "ふいんき", "ふんいき")
      for value, error, correction in input_stream:
        output_stream.write('  // %s, %s, %s\n' % (value, error, correction))
        output_stream.write(
          code_generator_util.FormatWithCppEscape(
            '  { %s, %s, %s },\n', value, error, correction))

    output_stream.write('};\n')


def main():
  options = ParseOptions()
  WriteData(options.input, options.output)


if __name__ == "__main__":
  main()
