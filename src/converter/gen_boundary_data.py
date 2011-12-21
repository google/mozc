# -*- coding: utf-8 -*-
# Copyright 2010-2011, Google Inc.
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

"""A tool to generate boundary data."""

__author__ = "taku"

import re
import sys


def PatternToRegexp(pattern):
  return '^' + pattern.replace('*', '[^,]+')


def LoadPatterns(file):
  prefix = []
  suffix = []
  for line in open(file, 'r'):
    if len(line) <= 1 or line[0] == '#':
      continue
    fields = line.split()
    label = fields[0]
    feature = fields[1]
    cost = int(fields[2])
    if cost < 0 or cost > 0xffff:
      sys.exit(-1)
    if label == 'PREFIX':
      prefix.append([re.compile(PatternToRegexp(feature)), cost])
    elif label == 'SUFFIX':
      suffix.append([re.compile(PatternToRegexp(feature)), cost])
    else:
      print 'format error %s' % (line)
      sys.exit(0)
  return (prefix, suffix)


def GetCost(patterns, feature):
  for p in patterns:
    pat = p[0]
    cost = p[1]
    if pat.match(feature):
      return cost
  return 0


def LoadFeatures(file):
  features = []
  for line in open(file, 'r'):
    fields = line.split()
    features.append(fields[1])
  return features


def main():
  (prefix, suffix) = LoadPatterns(sys.argv[1])
  features = LoadFeatures(sys.argv[2])
  print 'namespace {'
  print 'const BoundaryData kBoundaryData[] = {'

  for n in range(len(features)):
    print ' { %d, %d }, // "%s"' % \
        (GetCost(prefix, features[n]),
         GetCost(suffix, features[n]),
         features[n])

  # TODO(team): assume that we have up to 10 special POSes.
  for n in range(10):
    print ' { 0, 0 },'

  print '};'
  print '}  // namespace'


if __name__ == '__main__':
  main()
