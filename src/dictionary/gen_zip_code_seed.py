# -*- coding: utf-8 -*-
# Copyright 2010-2013, Google Inc.
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

"""Zip code dictionary generator.

 The tool for generating zip code dictionary.
 Input files are shift-jis csv.
 Output lines will be printed as utf-8.

 usage:
 ./gen_zip_code_seed.py --zip_code=zip_code.csv --jigyosyo=jigyosyo.csv
  > zip_code_seed.tsv

 Zip code sample input line:
 01101,"060  ","0600007","ﾎｯｶｲﾄﾞｳ",
 "ｻｯﾎﾟﾛｼﾁｭｳｵｳｸ","ｷﾀ7ｼﾞｮｳﾆｼ","北海道",
 "札幌市中央区","北七条西",0

 Jigyosyo zip code sample input line:
 01101,"ｻﾂﾎﾟﾛｼﾁﾕｳｵｳｸﾔｸｼﾖ",
 "札幌市中央区役所","北海道","札幌市中央区",
 "南三条西","１１丁目","0608612","060  ","札幌",0,0,0
"""

__author__ = "toshiyuki"

import codecs
import optparse
import re
import sys
import unicodedata


ZIP_CODE_LABEL = 'ZIP_CODE'
ZIP_CODE_COST = 7000


class ZipEntry (object):
  """Container class for zip code entry."""

  def __init__(self, zip_code, address):
    self.zip_code = zip_code
    self.address = address

  def FormatZip(self, zip_code):
    """Get formatted zip code."""
    # XXX-XXXX format
    return '-'.join([zip_code[0:3], zip_code[3:]])

  def Output(self):
    """Output entry."""
    zip_code = self.FormatZip(self.zip_code)
    address = unicodedata.normalize('NFKC', self.address)
    line = '\t'.join([zip_code, '0', '0', str(ZIP_CODE_COST),
                      address, ZIP_CODE_LABEL])
    print line.encode('utf-8')


def ProcessZipCodeCSV(file_name):
  """Process zip code csv."""
  csv_lines = ReadCSV(file_name)
  merged_csv_lines = MergeCSV(csv_lines)
  for tokens in merged_csv_lines:
    for entry in ReadZipCodeEntries(tokens[2], tokens[6], tokens[7], tokens[8]):
      entry.Output()


def ProcessJigyosyoCSV(file_name):
  """Process jigyosyo csv."""
  for tokens in ReadCSV(file_name):
    entry = ReadJigyosyoEntry(tokens[7], tokens[3], tokens[4],
                              tokens[5], tokens[2])
    entry.Output()


def ReadCSV(file_name):
  """Read CSV file."""
  # Do not use csv reader module because it does not support unicode
  return [GetCells(line) for line in codecs.open(file_name,
                                                 'r',
                                                 'shift_jis',
                                                 errors='replace')]


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
      last_entry[8] += entry[8]  # '町域'
  return ret


def ShouldMerge(zip_count, entry):
  """Return true if this entry should be merged to the previous entry."""
  zip_code = entry[2]
  flag_multi = (entry[12] == '1')
  should_merge = (zip_count[zip_code] > 1 and not flag_multi)
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
    SpecialMergeZip(u'5900111', u'大阪府', u'堺市中区', [u'三原台']),
    SpecialMergeZip(u'8710046', u'大分県', u'中津市', [u'金谷',
                                                       u'西堀端',
                                                       u'東堀端',
                                                       u'古金谷']),
    ]


def ShouldMergeSpecial(entry):
  """Return true for special cases to be merged."""
  zip_code = entry[2]
  level1 = entry[6]
  level2 = entry[7]
  level3 = entry[8]
  for special_case in _SPECIAL_CASES:
    if (zip_code == special_case.zip_code and
        level1 == special_case.pref and
        level2 == special_case.city and
        ContinuedLine(level3, special_case.towns)):
      return True
  return False


def ContinuedLine(level3, towns):
  """Return true if this seems continued line."""
  for town in towns:
    if level3.startswith(town):
      return False
  return True


def ReadZipCodeEntries(zip_code, level1, level2, level3):
  """Read zip code entries."""
  return [ZipEntry(zip_code, u''.join([level1, level2, town]))
          for town in ParseTownName(level3)]


def ReadJigyosyoEntry(zip_code, level1, level2, level3, name):
  """Read jigyosyo entry."""
  return ZipEntry(zip_code,
                  u''.join([level1, level2, level3, u' ', name]))


def ParseTownName(level3):
  """Parse town name."""
  if level3.find(u'以下に掲載がない場合') != -1:
    return ['']

  assert CanParseAddress(level3), ('failed to be merged %s'
                                   % level3.encode('utf-8'))

  # We ignore additional information here.
  level3 = re.sub(u'（.*）', u'', level3, re.U)

  # For 地割, we have these cases.
  #  XX1地割
  #  XX1地割、XX2地割、
  #  XX1地割〜XX2地割、
  #  XX第1地割
  #  XX第1地割、XX第2地割、
  #  XX第1地割〜XX第2地割、
  # We simply use XX for them.
  chiwari_match = re.match(u'(\D*?)第?\d+地割.*', level3, re.U)
  if chiwari_match:
    town = chiwari_match.group(1)
    return [town]

  # For "、"
  #  XX町YY、ZZ
  #   -> XX町YY and (XX町)ZZ
  #  YY、ZZ
  #   -> YY and ZZ
  chou_match = re.match(u'(.*町)?(.*)', level3, re.U)
  if chou_match:
    chou = u''
    if chou_match.group(1):
      chou = chou_match.group(1)
    rests = chou_match.group(2)
    return [chou + rest for rest in rests.split(u'、')]

  return [level3]


def CanParseAddress(address):
  """Return true for valid address."""
  return (address.find(u'（') == -1 or
          address.find(u'）') != -1)


def ParseOptions():
  """Parse command line options."""
  parser = optparse.OptionParser(usage='Usage: %prog [options]')
  parser.add_option('--zip_code', dest='zip_code',
                    action='store', default='',
                    help='specify zip code csv file path.')
  parser.add_option('--jigyosyo', dest='jigyosyo',
                    action='store', default='',
                    help='specify zip code csv file path.')
  (options, unused_args) = parser.parse_args()
  return options


def main():
  options = ParseOptions()

  if options.zip_code:
    ProcessZipCodeCSV(options.zip_code)

  if options.jigyosyo:
    ProcessJigyosyoCSV(options.jigyosyo)

  return 0


if __name__ == '__main__':
  sys.exit(main())
