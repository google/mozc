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

__author__ = "taku"

import sys

from build_tools import code_generator_util


def main():
  result = []
  with open(sys.argv[1], 'r') as stream:
    for line in stream:
      line = line.rstrip('\r\n')
      fields = line.split('\t')
      key = fields[0]
      lid = int(fields[1])
      rid = int(fields[2])
      cost = int(fields[3])
      value = fields[4]

      if key == value:
        value = None

      result.append((line, (key, value, lid, rid, cost)))

  # Sort entries in the key-incremental order.
  result.sort(key=lambda e: e[1][0])

  print 'const SuffixToken kSuffixTokens[] = {'
  for (line, (key, value, lid, rid, cost)) in result:
    print '// "%s"' % line
    print '{ %s, %s, %d, %d, %d },' % (
        code_generator_util.ToCppStringLiteral(key),
        code_generator_util.ToCppStringLiteral(value),
        lid, rid, cost)
  print '};'


if __name__ == "__main__":
  main()
