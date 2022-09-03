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

r"""Converter of Hentaigana data from Unicode data file to C++ inc file.

Usage:
  python gen_single_hentaigana_rewriter_data.py --input hentaigana.tsv
  --output single_hentaigana_rewriter_data.cc.inc

File format: tsv with # comment
glyph codepoint character_type reading origin  unicode_description
𛀂       U+1B002 hentaigana      あ       安       HENTAIGANA LETTER A-1
"""

import argparse
import collections
from typing import Tuple, List


def make_cpp_string(key: str, value: List[Tuple[str, str]]) -> str:
  r"""Make C++ code for key value pair.

  Args:
    key: str, reading entry
    value: List[Tuple[str, str]], (glyph, origin) list

  Returns:
    for key: あ, value: [(glyph: 𛀂, origin: 安)]
    output: {"あ", {
            {"\U0001B002", "安"}, // 𛀂
            }}
  """

  def make_row(glyph: str, origin: str) -> str:
    return '{{"\\U{codepoint:08X}", "{origin}"}}, // {glyph}'.format(
        codepoint=ord(glyph), origin=origin, glyph=glyph)

  return """{{"{key}", {{
  {rows}
  }}}},
  """.format(
      key=key,
      rows="\n".join([make_row(glyph, origin) for glyph, origin in value]))


# For Hentaigana of ゐ/ゑ, readings い/え and うぃ/うぇ are automatically added,
# for input convenience.
ADDITIONAL_READINGS = {
    "いぇ": ["え"],
    "ゐ": ["い", "うぃ"],
    "ゑ": ["え", "うぇ"],
    "を": ["お", "うぉ"]
}


def parse_tsv(input_file: str) -> List[Tuple[str, str, str]]:
  """input_file: path for data/hentaigana/hentaigana.tsv."""
  hentaigana_data = []  # (glyph, reading, origin)
  with open(input_file, "r", encoding="utf-8") as f:
    for line in f:
      # empty line
      if not line:
        continue
      # comment
      if line[0] == "#":
        continue

      # (glyph, codepoint, character_type, reading, origin, unicode_description)
      data = line.split("\t")
      hentaigana_data.append((data[0], data[3], data[4]))
      if data[3] in ADDITIONAL_READINGS:
        for additional_reading in ADDITIONAL_READINGS:
          hentaigana_data.append((data[0], additional_reading, data[4]))

  return hentaigana_data


def output_data(hentaigana_data: List[Tuple[str, str, str]],
                output_file: str) -> None:
  """Write hentaigana_data in output_file.

  Args:
    hentaigana_data: hentaigana entries in format (glyph, codepoint,
      character_type, reading, origin, unicode_description).
    output_file: file path for output.
  """
  cpp_data = collections.defaultdict(list)
  for glyph, reading, origin in hentaigana_data:
    cpp_data[reading].append((glyph, origin))

  cpp_entry = "\n".join(
      [make_cpp_string(key, value) for key, value in cpp_data.items()])

  with open(output_file, "w", encoding="utf-8") as f:
    f.write(cpp_entry)


def parse_options() -> argparse.Namespace:
  parser = argparse.ArgumentParser()
  parser.add_argument(
      "--input",
      dest="input",
      help="data/hentaigana/hentaigana.tsv file generated from Unicode resources"
  )
  parser.add_argument(
      "--output", dest="output", help="output destination file name (c++)")
  return parser.parse_args()


def main() -> None:
  options = parse_options()
  hentaigana_data = parse_tsv(options.input)
  output_data(hentaigana_data, options.output)


if __name__ == "__main__":
  main()
