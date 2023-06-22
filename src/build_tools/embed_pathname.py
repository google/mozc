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

r"""A script to embed the given (relative) path name to C/C++ characters array.

Example:
  ./embed_pathname.py --path_to_be_embedded=d:\data\mozc
      --constant_name=kMozcDataDir --output=mozc_data_dir.h
  Then you will get 'mozc_data_dir.h' which consists of the following line.
      constexpr char kMozcDataDir[] = "d:\\data\\mozc";
"""

import argparse
import os


def ParseArgs():
  """Parse command line options."""
  parser = argparse.ArgumentParser()
  parser.add_argument('--path_to_be_embedded')
  parser.add_argument('--constant_name')
  parser.add_argument('-p', type=int, default=0)
  parser.add_argument(
      '--output', type=argparse.FileType(mode='w', encoding='utf-8')
  )
  return parser.parse_args()


def main():
  args = ParseArgs()
  path = os.path.abspath(args.path_to_be_embedded)
  for _ in range(args.p):
    path = os.path.dirname(path)
  # TODO(yukawa): Consider the case of non-ASCII characters.
  escaped_path = path.replace('\\', r'\\')
  args.output.write(
      'constexpr char %s[] = "%s";\n' % (args.constant_name, escaped_path)
  )


if __name__ == '__main__':
  main()
