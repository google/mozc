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

r"""Zip code dictionary generator.

 The tool for generating zip code dictionary.
 Input files are shift-jis csv.
 Output lines will be printed as utf-8.

 usage:
   PYTHONPATH="${PYTHONPATH}:<Mozc's src root dir>"  \
     python ./gen_zip_code_seed.py  \
     --zip_code=zip_code.csv --jigyosyo=jigyosyo.csv  \
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

import optparse
import re
import sys
import unicodedata

from dictionary import zip_code_util


ZIP_CODE_LABEL = 'ZIP_CODE'
ZIP_CODE_COST = 7000


class ZipEntry(object):
  """Container class for zip code entry."""

  def __init__(self, zip_code, address):
    self.zip_code = zip_code
    self.address = address

  def FormatZip(self, zip_code):
    """Get formatted zip code."""
    # XXX-XXXX format
    return '-'.join([zip_code[0:3], zip_code[3:]])

  def GetLine(self):
    """Return the output line."""
    zip_code = self.FormatZip(self.zip_code)
    address = unicodedata.normalize('NFKC', self.address)
    line = '\t'.join(
        [zip_code, '0', '0', str(ZIP_CODE_COST), address, ZIP_CODE_LABEL]
    )
    return line

  def Output(self):
    """Outout line."""
    print(self.GetLine())


def ProcessZipCodeCSV(file_name):
  """Process zip code csv."""
  csv_lines = zip_code_util.ReadCSV(file_name)
  merged_csv_lines = zip_code_util.MergeCSV(csv_lines)
  output = []
  for tokens in merged_csv_lines:
    for entry in ReadZipCodeEntries(tokens[2], tokens[6], tokens[7], tokens[8]):
      output.append(entry.GetLine())
  return output


def ProcessJigyosyoCSV(file_name):
  """Process jigyosyo csv."""
  output = []
  for tokens in zip_code_util.ReadCSV(file_name):
    entry = ReadJigyosyoEntry(
        tokens[7], tokens[3], tokens[4], tokens[5], tokens[2]
    )
    output.append(entry.GetLine())
  return output


def ReadZipCodeEntries(zip_code, level1, level2, level3):
  """Read zip code entries."""
  return [
      ZipEntry(zip_code, ''.join([level1, level2, town]))
      for town in ParseTownName(level3)
  ]


def ReadJigyosyoEntry(zip_code, level1, level2, level3, name):
  """Read jigyosyo entry."""
  return ZipEntry(zip_code, ''.join([level1, level2, level3, ' ', name]))


def ParseTownName(level3):
  """Parse town name."""
  # Skip some exceptional cases
  # 中津市の次に番地がくる場合 : 871-0099 大分県中津市
  # 小菅村の次に1~663番地がくる場合 : 409-0142 山梨県北都留郡小菅村
  # 小菅村の次に664番地以降がくる場合 : 409-0211 山梨県北都留郡小菅村
  # 渡名喜村一円 : 901-3601 沖縄県島尻郡渡名喜村
  # 琴平町の次に４２７番地以降がくる場合（川西） : 766-0001 香川県仲多度郡琴平町
  # 琴平町の次に１～４２６番地がくる場合（川東） : 766-0002 香川県仲多度郡琴平町
  if (
      level3.find('以下に掲載がない場合') != -1
      or level3.find('がくる場合') != -1
      or level3.endswith('村一円')
  ):
    return ['']

  assert CanParseAddress(level3), 'failed to be merged %s' % level3.encode(
      'utf-8'
  )

  # We ignore additional information here.
  level3 = re.sub('（.*）', '', level3, flags=re.U)

  # For 地割, we have these cases.
  #  XX1地割
  #  XX1地割、XX2地割、
  #  XX1地割〜XX2地割、
  #  XX第1地割
  #  XX第1地割、XX第2地割、
  #  XX第1地割〜XX第2地割、
  # We simply use XX for them.
  chiwari_match = re.match('(\\D*?)第?\\d+地割.*', level3, re.U)
  if chiwari_match:
    town = chiwari_match.group(1)
    return [town]

  # For "、"
  #  XX町YY、ZZ
  #   -> XX町YY and (XX町)ZZ
  #  YY、ZZ
  #   -> YY and ZZ
  chou_match = re.match('(.*町)?(.*)', level3, re.U)
  if chou_match:
    chou = ''
    if chou_match.group(1):
      chou = chou_match.group(1)
    rests = chou_match.group(2)
    return [chou + rest for rest in rests.split('、')]

  return [level3]


def CanParseAddress(address):
  """Return true for valid address."""
  return address.find('（') == -1 or address.find('）') != -1


def ParseOptions():
  """Parse command line options."""
  parser = optparse.OptionParser(usage='Usage: %prog [options]')
  parser.add_option(
      '--zip_code',
      dest='zip_code',
      action='store',
      default='',
      help='specify zip code csv file path.',
  )
  parser.add_option(
      '--jigyosyo',
      dest='jigyosyo',
      action='store',
      default='',
      help='specify zip code csv file path.',
  )
  parser.add_option(
      '--output',
      dest='output',
      action='store',
      default=None,
      help='specify output file path.',
  )
  (options, unused_args) = parser.parse_args()
  return options


def main():
  options = ParseOptions()

  lines = []
  if options.zip_code:
    lines += ProcessZipCodeCSV(options.zip_code)

  if options.jigyosyo:
    lines += ProcessJigyosyoCSV(options.jigyosyo)

  if options.output:
    with open(options.output, 'w', encoding='utf-8') as output:
      if lines:
        output.write('\n'.join(lines) + '\n')
  else:
    print('\n'.join(lines))
  return 0


if __name__ == '__main__':
  sys.exit(main())
