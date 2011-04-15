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

"""
A tool to generate EOS data
"""

__author__ = "taku"

import sys

def ReadBoundary(file):
  data = []
  for line in open(file, "r"):
    fields = line.split()
    data.append([int(fields[0]), int(fields[1])])

  # sort by score field.
  data.sort(lambda x, y: cmp(x[1], y[1]))
  return data


def main():
  data = ReadBoundary(sys.argv[1])
  print "namespace {"
  print "struct BoundaryData {"
  print "  uint16 id;";
  print "  int16  wcost;";
  print "};"
  print "const BoundaryData kBoundaryData[] = {"
  for n in range(len(data)):
    if n != 0:
      print ","
    print  " { %d, %d }" % (data[n][0], data[n][1]),
  print "};"
  print "}  // namespace"

if __name__ == "__main__":
  main()
