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
  + Fields: [key, value, base_key, base_value, cost_offset]
* lid, rid and cost are copied from entries of base_key and base_value.
* cost_offset adjusts the cost copied from the base entry. If the cost_offset
  is -1, the new entry is ranked higher than the base entry.

gen_aux_dictionary.py --output aux_dictionary.txt
--aux_tsv data/dictionary_oss/aux_dictionary.tsv
--dictionary_txts data/dictionary_oss/dictionary0*.txt
"""

import argparse
import sys


class Entry():
  """Class for an entry in the dictionary."""

  def __init__(self, key: str, value: str, lid: str, rid: str, cost: int):
    self.key = key
    self.value = value
    self.lid = lid
    self.rid = rid
    self.cost = cost

  def EntryKey(self) -> str:
    return '\t'.join([self.lid, self.rid, self.key, self.value])

  @classmethod
  def Parse(cls, line: str) -> 'Entry':
    key, lid, rid, cost, value, *_ = line.rstrip().split('\t')
    return Entry(key, value, lid, rid, int(cost))


class EntryList():
  """Class for a list of entries."""

  def __init__(self):
    self.entries = []
    self.sorted = False

  def Add(self, entry: Entry) -> None:
    self.entries.append(entry)
    self.sorted = False

  def Sort(self) -> None:
    if self.sorted:
      return
    self.entries.sort(key=lambda entry: entry.cost)
    self.sorted = True

  def AtRatio(self, ratio: float) -> Entry:
    self.Sort()
    ratio = max(0, min(ratio, 1.0))
    index = int(max(len(self.entries) - 1, 0) * ratio)
    return self.entries[index]


class PosIds():
  """Class for pos ids."""

  def __init__(self):
    self.ids = {}  # "pos_name" -> "pos_id"
    # Keys are subset of data/rules/user_pos.def
    self.pos_alias = {
        '名詞': '名詞,一般,*,*,*,*,*',
        '固有名詞': '名詞,固有名詞,一般,*,*,*,*',
        '人名': '名詞,固有名詞,人名,一般,*,*,*',
        '姓': '名詞,固有名詞,人名,姓,*,*,*',
        '名': '名詞,固有名詞,人名,名,*,*,*',
        '組織': '名詞,固有名詞,組織,*,*,*,*',
        '地名': '名詞,固有名詞,地域,一般,*,*,*',
        '名詞サ変': '名詞,サ変接続,*,*,*,*,*',
        '名詞形動': '名詞,形容動詞語幹,*,*,*,*,*',
        '副詞': '副詞,一般,*,*,*,*,*',
        '連体詞': '連体詞,*,*,*,*,*,*',
        '接続詞': '接続詞,*,*,*,*,*,*',
        '感動詞': '感動詞,*,*,*,*,*,*',
        '接頭語': '接頭詞,名詞接続,*,*,*,*,*',
        '助数詞': '名詞,接尾,助数詞,*,*,*,*',
        '接尾一般': '名詞,接尾,一般,*,*,*,*',
        '接尾人名': '名詞,接尾,人名,*,*,*,*',
        '接尾地名': '名詞,接尾,地域,*,*,*,*',
    }

  def ParseFile(self, id_def: str):
    for line in open(id_def, encoding='utf-8'):
      pos_id, name = line.rstrip().split(' ')
      self.ids[name] = pos_id

    for alias, name in self.pos_alias.items():
      pos_id = self.ids[name]
      self.ids[alias] = pos_id

  def GetId(self, pos_name: str) -> str:
    return self.ids.get(pos_name, '')


class Dictionary():
  """Class for dictionary0*.txt."""

  def __init__(self):
    self.entry_set = set()
    self.data = {}  # "<key>\t<value>" -> [entry1, entry2, ...]
    self.pos_entry_map = {}  # "<lid>\t<rid>" -> EntryList

  def Parse(self, dictionary_txts):
    for file in dictionary_txts:
      self._ParseFile(file)

  def _ParseFile(self, file):
    """Parses the file and set values to data and entry_set."""
    for line in open(file, encoding='utf-8'):
      entry = Entry.Parse(line)

      # Update data
      data_key = '\t'.join([entry.key, entry.value])
      self.data.setdefault(data_key, []).append(entry)

      # Update entry_set
      self.entry_set.add(entry.EntryKey())

      # Update pos_map
      pos_key = '\t'.join([entry.lid, entry.rid])
      if pos_key not in self.pos_entry_map:
        self.pos_entry_map[pos_key] = EntryList()
      self.pos_entry_map[pos_key].Add(entry)

  def Exists(self, key, value, lid, rid):
    entry_key = '\t'.join([lid, rid, key, value])
    return entry_key in self.entry_set

  def GetDataList(self, key, value):
    data_key = '\t'.join([key, value])
    return self.data.get(data_key)

  def GetCost(self, pos_id: str, ratio: float) -> int:
    pos_key = '\t'.join([pos_id, pos_id])
    return self.pos_entry_map[pos_key].AtRatio(ratio).cost


class AuxDictionary():
  """Class for aux_dictionary.tsv."""

  def __init__(self, dictionary):
    self.dictionary = dictionary
    self.aux_list = []
    self.aux_set = set()

  def Parse(self, aux_tsv, strict=False):
    """Parses the file and update aux_list."""
    for line in open(aux_tsv, encoding='utf-8'):
      if line.startswith('#'):
        continue
      key, value, base_key, base_value, cost_offset = line.rstrip().split('\t')
      data_list = self.dictionary.GetDataList(base_key, base_value)
      if not data_list:
        message = '%s and %s are not in the dictionary' % (base_key, base_value)
        if strict:
          print('Error: ' + message, file=sys.stderr)
          sys.exit(1)
        else:
          print('Warning: ' + message, file=sys.stderr)
          continue

      for entry in data_list:
        if self.dictionary.Exists(key, value, entry.lid, entry.rid):
          # Not overwrite the existing entry.
          continue
        cost = int(entry.cost) + int(cost_offset)
        self.aux_list.append([key, entry.lid, entry.rid, str(cost), value])
        self.aux_set.add('\t'.join([entry.lid, entry.rid, key, value]))

  def Exists(self, key, value, lid, rid):
    entry_key = '\t'.join([lid, rid, key, value])
    return entry_key in self.aux_set

  def WriteFile(self, output):
    with open(output, 'w', encoding='utf-8') as file:
      for aux_entry in self.aux_list:
        file.write('\t'.join(aux_entry) + '\n')


class WordsDictionary():
  """Class for aux_dictionary.tsv."""

  def __init__(self, dictionary, aux):
    self.dictionary = dictionary
    self.aux = aux
    self.words = []

  def Parse(self, words_tsv, id_def, ratio=0.5):
    """Parses the file and update aux_list."""
    pos_ids = PosIds()
    pos_ids.ParseFile(id_def)

    for line in open(words_tsv, encoding='utf-8'):
      if line.startswith('#') or not line.rstrip():
        continue

      key, value, pos = line.rstrip().split('\t')
      pos_id = pos_ids.GetId(pos)
      if not pos_id:
        print('Error: ' + pos + ' is an invalid pos', file=sys.stderr)
        print(line, file=sys.stderr)
        sys.exit(1)

      # Do not overwrite the existing entry.
      if self.dictionary.Exists(key, value, pos_id, pos_id) or self.aux.Exists(
          key, value, pos_id, pos_id
      ):
        continue

      # Use the same cost as the entry of pos_id and ratio.
      # ratio indicates the position in the words sorted by cost.
      # 0.5 means the middle of the list. 0.0 means the top of the list.
      cost = self.dictionary.GetCost(pos_id, ratio)
      self.words.append([key, pos_id, pos_id, str(cost), value])

  def WriteFile(self, output):
    with open(output, 'a', encoding='utf-8') as file:
      for word in self.words:
        file.write('\t'.join(word) + '\n')


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--aux_tsv')
  parser.add_argument('--words_tsv')
  parser.add_argument('--dictionary_txts', nargs='+')
  parser.add_argument('--id_def')
  parser.add_argument('--output')
  parser.add_argument('--strict', action='store_true')
  args = parser.parse_args()

  dictionary = Dictionary()
  dictionary.Parse(args.dictionary_txts)

  aux = AuxDictionary(dictionary)
  aux.Parse(args.aux_tsv, strict=args.strict)
  aux.WriteFile(args.output)

  if args.words_tsv and args.id_def:
    words = WordsDictionary(dictionary, aux)
    words.Parse(args.words_tsv, args.id_def)
    words.WriteFile(args.output)

if __name__ == '__main__':
  main()
