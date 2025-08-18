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

"""Util module for Japanese postal(ZIP) code."""

import codecs


def ReadCSV(file_name):
  """Read CSV file."""
  # Do not use csv reader module because it does not support unicode
  return [
      GetCells(line)
      for line in codecs.open(file_name, 'r', 'cp932', errors='replace')
  ]


def GetCells(line):
  """Get cells."""
  # [A, B, C, ..] from "A","B ",C,..
  return [column.strip('"').strip() for column in line.strip().split(',')]


def MergeCSV(csv_lines):
  """Merge CSV."""
  # When the flag says a zip code have no multiple entry while we can see
  # multiple line for that zip code, we have to merge them.
  zip_count = {}
  ret = []
  for entry in csv_lines:
    zip_code = entry[2]
    zip_count[zip_code] = zip_count.get(zip_code, 0) + 1
    if not ShouldMerge(zip_count, entry):
      ret.append(entry)
    else:
      last_entry = ret[-1]
      last_entry[5] += entry[5]  # reading of '町域'
      last_entry[8] += entry[8]  # '町域'
  return ret


def ShouldMerge(zip_count, entry):
  """Return true if this entry should be merged to the previous entry."""
  zip_code = entry[2]
  flag_multi = entry[12] == '1'
  should_merge = zip_count[zip_code] > 1 and not flag_multi
  should_merge_special = ShouldMergeSpecial(entry)
  return should_merge or should_merge_special


class SpecialMergeZip(object):
  """Container class for special zip code entry to be merged."""

  def __init__(self, zip_code, pref, city, towns):
    self.zip_code = zip_code
    self.pref = pref
    self.city = city
    self.towns = towns


_SPECIAL_CASES = [
    SpecialMergeZip('5900111', '大阪府', '堺市中区', ['三原台']),
    SpecialMergeZip(
        '8710046', '大分県', '中津市', ['金谷', '西堀端', '東堀端', '古金谷']
    ),
    SpecialMergeZip('9218046', '石川県', '金沢市', ['大桑町', '三小牛町']),
]


def ShouldMergeSpecial(entry):
  """Return true for special cases to be merged."""
  zip_code = entry[2]
  level1 = entry[6]
  level2 = entry[7]
  level3 = entry[8]
  for special_case in _SPECIAL_CASES:
    if (
        zip_code == special_case.zip_code
        and level1 == special_case.pref
        and level2 == special_case.city
        and ContinuedLine(level3, special_case.towns)
    ):
      return True
  return False


def ContinuedLine(level3, towns):
  """Return true if this seems continued line."""
  for town in towns:
    if level3.startswith(town):
      return False
  return True
