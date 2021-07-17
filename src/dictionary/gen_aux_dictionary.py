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

"""A tool to generate aux_dictionary.txt from aux_dictionary.tsv.

* Create a dictionary with entries of key and value in the TSV file.
* lid, rid and cost are copied from entries of base_key and base_value.

gen_aux_dictionary.py --output aux_dictionary.txt
--aux_tsv data/oss/aux_dictionary.tsv
--dictionary_txts data/dictionary_oss/dictionary0*.txt
"""

import argparse


class Dictionary():
  """Class for dictionary0*.txt."""

  def __init__(self):
    self.entry_set = set()
    self.data = {}

  def Parse(self, dictionary_txts):
    for file in dictionary_txts:
      self._ParseFile(file)

  def _ParseFile(self, file):
    """Parses the file and set values to data and entry_set."""
    for line in open(file, encoding='utf-8'):
      key, lid, rid, cost, value, *_ = line.rstrip().split('\t')

      # Update data
      data_key = '\t'.join([key, value])
      data_value = [lid, rid, cost]
      self.data.setdefault(data_key, []).append(data_value)

      cost = int(cost)

      # Update entry_set
      entry_key = '\t'.join([lid, rid, key, value])
      self.entry_set.add(entry_key)

  def Exists(self, key, value, lid, rid):
    entry_key = '\t'.join([lid, rid, key, value])
    return entry_key in self.entry_set

  def GetDataList(self, key, value):
    data_key = '\t'.join([key, value])
    return self.data[data_key]


class AuxDictionary():
  """Class for aux_dictionary.tsv."""

  def __init__(self, dictionary):
    self.dictionary = dictionary
    self.aux_list = []

  def Parse(self, aux_tsv):
    """Parses the file and update aux_list."""
    for line in open(aux_tsv, encoding='utf-8'):
      if line.startswith('#'):
        continue
      key, value, base_key, base_value = line.rstrip().split('\t')
      for data in self.dictionary.GetDataList(base_key, base_value):
        base_lid, base_rid, base_cost = data
        if self.dictionary.Exists(key, value, base_lid, base_rid):
          # Not overwrite the existing entry.
          continue
        self.aux_list.append([key, base_lid, base_rid, base_cost, value])

  def WriteFile(self, output):
    with open(output, 'w', encoding='utf-8') as file:
      for aux_entry in self.aux_list:
        file.write('\t'.join(aux_entry))


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--aux_tsv')
  parser.add_argument('--dictionary_txts', nargs='+')
  parser.add_argument('--output')
  args = parser.parse_args()

  dictionary = Dictionary()
  dictionary.Parse(args.dictionary_txts)

  aux = AuxDictionary(dictionary)
  aux.Parse(args.aux_tsv)
  aux.WriteFile(args.output)


if __name__ == '__main__':
  main()
