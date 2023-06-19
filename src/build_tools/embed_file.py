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

"""Generate a C++ array definition for file embedding."""
import argparse
import os


def _ParseArgs():
  """Parse command line options."""
  parser = argparse.ArgumentParser()
  parser.add_argument('--input')
  parser.add_argument('--name')
  parser.add_argument(
      '--output', type=argparse.FileType(mode='w', encoding='utf-8')
  )
  return parser.parse_args()


def _FormatAsUint64LittleEndian(s):
  """Formats a string as uint64_t value in little endian order."""
  for _ in range(len(s), 8):
    s += b'\0'
  s = s[::-1]  # Reverse the string
  return '0x' + s.hex()


def main():
  args = _ParseArgs()
  args.output.write(
      '#ifdef MOZC_EMBEDDED_FILE_%(name)s\n'
      '#error "%(name)s was already included or defined elsewhere"\n'
      '#else\n'
      '#define MOZC_EMBEDDED_FILE_%(name)s\n'
      'constexpr uint64_t %(name)s_data[] = {'
      % {'name': args.name}
  )

  with open(args.input, 'rb') as infile:
    i = 0
    while True:
      chunk = infile.read(8)
      if not chunk:
        break
      if i % 4 == 0:
        args.output.write('\n  ')
      else:
        args.output.write(' ')
      args.output.write(_FormatAsUint64LittleEndian(chunk))
      args.output.write(',')
      i = i + 1

  args.output.write(
      '\n};\n'
      'constexpr EmbeddedFile %(name)s = {\n'
      '  %(name)s_data,\n'
      '  %(size)d,\n'
      '};\n'
      '#endif  // MOZC_EMBEDDED_FILE_%(name)s\n'
      % {'name': args.name, 'size': os.stat(args.input).st_size}
  )


if __name__ == '__main__':
  main()
