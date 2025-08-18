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

"""Utilities to handle pos related stuff for source code generation."""

import codecs
import collections
import logging
import re

from build_tools import code_generator_util


class PosDataBase(object):
  """Utility to look up data in id.def and special_pos.def."""

  def __init__(self):
    self.id_list = []

  def Parse(self, id_file, special_pos_file):
    id_list = []
    with codecs.open(id_file, 'r', encoding='utf-8') as stream:
      stream = code_generator_util.SkipLineComment(stream)
      stream = code_generator_util.ParseColumnStream(stream, num_column=2)
      for pos_id, feature in stream:
        id_list.append((feature, int(pos_id)))

    max_id = max(pos_id for _, pos_id in id_list)
    with codecs.open(special_pos_file, 'r', encoding='utf-8') as stream:
      stream = code_generator_util.SkipLineComment(stream)
      for pos_id, line in enumerate(stream, start=max_id + 1):
        id_list.append((line, pos_id))
    self.id_list = id_list

  def GetPosId(self, feature):
    """Returns id for the feature if found. Otherwise None."""
    assert feature
    for line, pos_id in self.id_list:
      # Return by prefix match.
      if line.startswith(feature):
        return pos_id

    logging.warning('Cannot find the POS for: %s', feature)

  @staticmethod
  def _GroupConsecutiveId(iterable):
    result = []
    for value in iterable:
      if result and result[-1] + 1 != value:
        yield result
        result = []
      result.append(value)
    if result:
      yield result

  def GetRange(self, pattern):
    id_list = [pos_id for line, pos_id in self.id_list if pattern.match(line)]
    id_list.sort()
    return [
        (id_range[0], id_range[-1])
        for id_range in PosDataBase._GroupConsecutiveId(id_list)
    ]


class PosMatcher(object):

  def __init__(self, pos_database):
    self.pos_database = pos_database
    self._match_rule_map = {}

  def Parse(self, pos_matcher_rule_file):
    with codecs.open(pos_matcher_rule_file, 'r', encoding='utf-8') as stream:
      stream = code_generator_util.SkipLineComment(stream)
      stream = code_generator_util.ParseColumnStream(stream, num_column=2)
      self._match_rule_map = dict(
          (name, (pattern, re.compile(pattern.replace('*', '[^,]+')), sortkey))
          for sortkey, (name, pattern) in enumerate(stream)
      )

  def GetRuleNameList(self):
    """Returns a list of rule names in the original file's order."""
    sorted_rule_list = sorted(
        list(self._match_rule_map.items()), key=lambda item: item[1][2]
    )
    return [rule_name for rule_name, _ in sorted_rule_list]

  def GetRange(self, name):
    return self.pos_database.GetRange(self._match_rule_map[name][1])

  def GetId(self, name):
    return self.pos_database.GetRange(self._match_rule_map[name][1])[0][0]

  def GetOriginalPattern(self, name):
    return self._match_rule_map[name][0]


class InflectionMap(object):
  """Utility to handle inflection map.

  Inflection map is a map from key to (form, value_suffix, key_suffix).
  """

  def __init__(self):
    self._map = {}

  def Parse(self, filepath):
    result = collections.defaultdict(list)
    with codecs.open(filepath, 'r', encoding='utf-8') as stream:
      stream = code_generator_util.SkipLineComment(stream)
      stream = code_generator_util.ParseColumnStream(stream, num_column=4)
      for key, form, value_suffix, key_suffix in stream:
        result[key].append((
            form,
            value_suffix if value_suffix != '*' else '',
            key_suffix if key_suffix != '*' else '',
        ))
    self._map = result

  def Get(self, key):
    return self._map[key]


class UserPos(object):
  """Utility to handle user pos.

  The data is assoc list from user_pos (string) to conjugation_list.
  conjugation_list is a list of (value_suffix, key_suffix, pos_id).
  """

  def __init__(self, pos_database, inflection_map):
    self._pos_database = pos_database
    self._inflection_map = inflection_map
    self.data = []

  def Parse(self, filepath):
    result = []
    with codecs.open(filepath, 'r', encoding='utf-8') as stream:
      stream = code_generator_util.SkipLineComment(stream)
      stream = code_generator_util.ParseColumnStream(stream, num_column=4)
      for user_pos, _, ctype, feature in stream:
        conjugation_list = []
        if ctype == '*':
          conjugation_list.append(
              (None, None, self._pos_database.GetPosId(feature))
          )
        else:
          for form, value_suffix, key_suffix in self._inflection_map.Get(ctype):
            # repalce <cfrom> with actual cform
            pos_id = self._pos_database.GetPosId(
                feature.replace('<cform>', form)
            )

            # Known error items.
            # 動詞,自立,*,*,五段動詞,体言接続特殊２,*
            # 形容詞,自立,*,*,形容詞・アウオ段,文語基本形,*
            if pos_id is not None:
              conjugation_list.append((value_suffix, key_suffix, pos_id))

        result.append((user_pos, conjugation_list))
    self.data = result

  def GetPosId(self, pos):
    """Returns id of the given pos."""
    for user_pos, conjugation_list in self.data:
      if user_pos == pos:
        return conjugation_list[0][2]
