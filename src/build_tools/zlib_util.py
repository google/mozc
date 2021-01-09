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

"""Simple zlib utility."""

from __future__ import absolute_import
from __future__ import print_function
import sys
import zlib


def _Read(filename):
  """Reads a file in binary mode."""
  with open(filename, 'rb') as f:
    return f.read()


def _Write(buf, filename):
  """Writes a buffer to a file in binary mode."""
  with open(filename, 'wb') as f:
    f.write(buf)


def Compress(input_filename, output_filename):
  """Compresses a single file by zlib's compression algorithm."""
  _Write(zlib.compress(_Read(input_filename)), output_filename)


def Decompress(input_filename, output_filename):
  """Decompresses a single deflated file."""
  _Write(zlib.decompress(_Read(input_filename)), output_filename)


def main():
  if len(sys.argv) != 4:
    print('Invalid arguments', file=sys.stderr)
    return
  if sys.argv[1] == 'compress':
    Compress(sys.argv[2], sys.argv[3])
    return
  if sys.argv[1] == 'decompress':
    Decompress(sys.argv[2], sys.argv[3])
    return
  print('Unknown command:', sys.argv[1], file=sys.stderr)


if __name__ == '__main__':
  main()
