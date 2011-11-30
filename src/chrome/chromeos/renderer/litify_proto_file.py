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

"""Litify a .proto file.

This program add a line
  "option optimize_for = LITE_RUNTIME;"
to the input .proto file.
"""

import fileinput
import optparse

LITE_OPTIMIZER = 'option optimize_for = LITE_RUNTIME;'


def ParseOption():
  parser = optparse.OptionParser()
  parser.add_option('--in_file_path', dest='in_file_path',
                    help='Specify the input protocol buffer definition file.')
  parser.add_option('--out_file_path', dest='out_file_path',
                    help='Specify the result file name.')
  (options, _) = parser.parse_args()
  return options


def ExecuteLitify(in_file_path, out_file_path):
  output_file = open(out_file_path, 'w')
  for line in fileinput.input(in_file_path):
    output_file.write(line)
  output_file.write('\n%s\n' % LITE_OPTIMIZER)
  output_file.close()


def main():
  options = ParseOption()
  ExecuteLitify(options.in_file_path, options.out_file_path)

if __name__ == '__main__':
  main()
