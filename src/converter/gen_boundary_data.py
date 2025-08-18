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

"""A tool to generate boundary data.

Bounday data binary image is an array of uint16_t whose length is 2N, where N is
the number of POS IDs including special POS.  The array has the following
structure:

-------------------------------------
prefix penalty of POS ID 0 (2 bytes)
-------------------------------------
suffix penalty of POS ID 0 (2 bytes)
-------------------------------------
prefix penalty of POS ID 1 (2 bytes)
-------------------------------------
suffix penalty of POS ID 1 (2 bytes)
-------------------------------------
  .
  .
  .
-------------------------------------
prefix penalty of POS ID N (2 bytes)
-------------------------------------
suffix penalty of POS ID N (2 bytes)
-------------------------------------

See converter/segmenter.cc for how it's used.
"""

import codecs
import optparse
import re
import struct
import sys


def PatternToRegexp(pattern):
  return '^' + pattern.replace('*', '[^,]+')


def LoadPatterns(file):
  prefix = []
  suffix = []
  for line in codecs.open(file, 'r', encoding='utf-8'):
    if len(line) <= 1 or line[0] == '#':
      continue
    fields = line.split()
    label = fields[0]
    feature = fields[1]
    cost = int(fields[2])
    if cost < 0 or cost > 0xFFFF:
      sys.exit(-1)
    if label == 'PREFIX':
      prefix.append([re.compile(PatternToRegexp(feature)), cost])
    elif label == 'SUFFIX':
      suffix.append([re.compile(PatternToRegexp(feature)), cost])
    else:
      print('format error %s' % (line))
      sys.exit(0)
  return (prefix, suffix)


def GetCost(patterns, feature):
  for p in patterns:
    pat = p[0]
    cost = p[1]
    if pat.match(feature):
      return cost
  return 0


def LoadFeatures(filename):
  features = []
  for line in codecs.open(filename, 'r', encoding='utf-8'):
    fields = line.split()
    features.append(fields[1])
  return features


def CountSpecialPos(filename):
  count = 0
  for line in codecs.open(filename, 'r', encoding='utf-8'):
    line = line.rstrip()
    if not line or line[0] == '#':
      continue
    count += 1
  return count


def ParseOptions():
  parser = optparse.OptionParser()
  parser.add_option(
      '--boundary_def', dest='boundary_def', help='Boundary definition file'
  )
  parser.add_option('--id_def', dest='id_def', help='Boundary definition file')
  parser.add_option(
      '--special_pos', dest='special_pos', help='Special POS definition file'
  )
  parser.add_option('--output', dest='output', help='Output binary file')
  return parser.parse_args()[0]


def main():
  opts = ParseOptions()

  prefix, suffix = LoadPatterns(opts.boundary_def)
  features = LoadFeatures(opts.id_def)
  num_special_pos = CountSpecialPos(opts.special_pos)

  with open(opts.output, 'wb') as f:
    for feature in features:
      f.write(struct.pack('<H', GetCost(prefix, feature)))
      f.write(struct.pack('<H', GetCost(suffix, feature)))

    for _ in range(num_special_pos):
      f.write(struct.pack('<H', 0))
      f.write(struct.pack('<H', 0))


if __name__ == '__main__':
  main()
