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

r"""A tool to remove specified words from dictionary files.

gen_filtered_dictionary.py --output filtered_dictionary.txt
--filter_tsv data/dictionary_oss/dictionary_filter.tsv
--dictionary_txts data/dictionary_oss/dictionary0*.txt

### Format of dictionary_filter.tsv

| key      | value    |
| :------: | :------: |
| あるぱか | 或るパカ |

* `key` and `value` are the regexp patterns of words to be removed.
* Each line is extended to the following regexp pattern:
  + `'^{key}\t\\d+\t\\d+\t\\d+\t{value}(\t.*)?\n$'`
"""

import argparse
import re


class Dictionary:
  """Class for dictionary0*.txt."""

  def __init__(self):
    self.lines = []
    self.filters = []

  def LoadFiles(self, dictionary_txts, filter_tsv):
    self._LoadFilter(filter_tsv)
    for file in dictionary_txts:
      self._LoadDict(file)

  def _LoadFilter(self, file):
    for line in open(file, encoding='utf-8'):
      if line.startswith('#'):
        continue
      key, value = line.rstrip('\n').split('\t')
      self.filters.append(key + '\t\\d+\t\\d+\t\\d+\t' + value + '(\t.*)?\n')

  def _LoadDict(self, file):
    """Parses the file and set values to data and entry_set."""
    for line in open(file, encoding='utf-8'):
      if self._CheckFilter(line):
        continue
      self.lines.append(line)

  def _CheckFilter(self, line):
    for pattern in self.filters:
      if re.fullmatch(pattern, line):
        return True
    return False

  def WriteFile(self, output):
    with open(output, 'w', encoding='utf-8', newline='\n') as file:
      for line in self.lines:
        file.write(line)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--filter_tsv')
  parser.add_argument('--dictionary_txts', nargs='+')
  parser.add_argument('--output')
  args = parser.parse_args()

  dictionary = Dictionary()
  dictionary.LoadFiles(args.dictionary_txts, args.filter_tsv)
  dictionary.WriteFile(args.output)


if __name__ == '__main__':
  main()
