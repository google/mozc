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

import codecs
import optparse


def IsPrefix(s, key):
  try:
    n = s.index(key)
  except:
    n = -1

  if n == 0:
    return True
  else:
    return False


def LoadRewriteMapRule(filename):
  fh = codecs.open(filename, encoding='utf-8')
  rule = []
  for line in fh:
    line = line.rstrip('\n')
    if not line or line.startswith('#'):
      continue
    fields = line.split()
    rule.append([fields[0], fields[1]])
  return rule


def ReadPOSID(id_file, special_pos_file):
  pos_list = []

  for line in codecs.open(id_file, 'r', encoding='utf-8'):
    fields = line.split()
    pos_list.append(fields[1])

  for line in codecs.open(special_pos_file, 'r', encoding='utf-8'):
    if len(line) <= 1 or line[0] == '#':
      continue
    fields = line.split()
    pos_list.append(fields[0])

  return pos_list


def ParseOptions():
  parser = optparse.OptionParser()
  parser.add_option('--id_def', dest='id_def', help='POS ID definition file')
  parser.add_option(
      '--special_pos', dest='special_pos', help='Special POS definition file'
  )
  parser.add_option(
      '--pos_group_def',
      dest='pos_group_def',
      help='Left POS ID group definition file',
  )
  parser.add_option(
      '--output', dest='output', help='Output file for binary mode'
  )
  return parser.parse_args()[0]


def main():
  opts = ParseOptions()

  # read lid file
  pos_list = ReadPOSID(opts.id_def, opts.special_pos)

  # read rule file
  rules = LoadRewriteMapRule(opts.pos_group_def)

  current_id = 1
  id_map = {}
  ids = bytearray()

  for target in pos_list:
    id = 0
    for rule in rules:
      if IsPrefix(target, rule[0]):
        if rule[1] in id_map:
          id = id_map[rule[1]]
        else:
          id = current_id
          id_map[rule[1]] = current_id
          current_id += 1
    ids.append(id)

  with open(opts.output, 'wb') as f:
    f.write(ids)


if __name__ == '__main__':
  main()
