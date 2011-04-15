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

"""A script to generate a C++ header file for the POS conversion map.
"""

__author__ = "keni"

import sys

BODY = """// Copyright 2009 Google Inc. All Rights Reserved.
// Author: keni

#ifndef MOZC_DICTIONARY_POS_MAP_H_
#define MOZC_DICTIONARY_POS_MAP_H_

// POS conversion rules
const POSMap kPOSMap[] = {
%s};

#endif  // MOZC_DICTIONARY_POS_MAP_H_
"""

def Escape(str):
  result = []
  length = len(str)
  x = 0
  for c in str:
    result.append("\\x%2X" % ord(c))
  return "".join(result)

def GenPOSMap(pos_map_file, user_pos_file):
  user_pos = {}
  outputs = []
  dup = {}

  # target POS must be found in user_pos map
  for line in open(user_pos_file, "r"):
    fields = line.split()
    user_pos[fields[0]] = True

  # read all POS mapping
  for line in open(pos_map_file, "r"):
    line = line.lstrip("\n");
    if line == '' or line[0] == '#':
      continue

    fields = line.split()
    num_fields = len(fields)

    assert(num_fields >= 1 and num_fields <= 3)

    if (num_fields >= 2):
      assert(user_pos.has_key(fields[1]))

    output = ''
    if len(fields) == 1:
      output = '  { "%s", NULL }, ' % Escape(fields[0])
    elif len(fields) >= 2:
      output = '  { "%s", "%s" }, ' % (Escape(fields[0]),
                                       Escape(fields[1]))

    # For example, when ATOK has POS "FOO", and MS-IME has the
    # same POS "FOO", we assume that these two POSes can be
    # translated into the same Mozc POS.
    # We allow duplicate source POSes in the pos_map, but target pos
    # for these POSes must be the same.
    if dup.has_key(fields[0]):
      assert(dup[fields[0]] == output)
      continue

    outputs.append(output)
    dup[fields[0]] = output

  # Make a Mozc to Mozc mapping rule.
  for line in open(user_pos_file, "r"):
    fields = line.split()
    output = ''
    if not dup.has_key(fields[0]):
      outputs.append('  { "%s", "%s" }, ' % (Escape(fields[0]),
                                             Escape(fields[0])))

  outputs.sort()
  return "\n".join(outputs) + "\n"

def main():
  user_pos_file = sys.argv[1]
  pos_map_file = sys.argv[2]
  pos_map = GenPOSMap(pos_map_file, user_pos_file)
  print BODY % pos_map

if __name__ == '__main__':
  main()
