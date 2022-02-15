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

from __future__ import absolute_import
from __future__ import print_function
import binascii
import optparse
import os


def _ParseOption():
  """Parse command line options."""
  parser = optparse.OptionParser()
  parser.add_option('--input', dest='input')
  parser.add_option('--name', dest='name')
  parser.add_option('--output', dest='output')
  return parser.parse_args()[0]


def _FormatAsUint64LittleEndian(s):
  """Formats a string as uint64 value in little endian order."""
  for _ in range(len(s), 8):
    s += b'\0'
  s = s[::-1]  # Reverse the string
  return b'0x%s' % binascii.b2a_hex(s)


def main():
  opts = _ParseOption()
  with open(opts.input, 'rb') as infile:
    with open(opts.output, 'wb') as outfile:
      outfile.write(
          ('#ifdef MOZC_EMBEDDED_FILE_%(name)s\n'
           '#error "%(name)s was already included or defined elsewhere"\n'
           '#else\n'
           '#define MOZC_EMBEDDED_FILE_%(name)s\n'
           'const uint64 %(name)s_data[] = {\n' % {
               'name': opts.name
           }).encode('utf-8'))

      while True:
        chunk = infile.read(8)
        if not chunk:
          break
        outfile.write(b'  ')
        outfile.write(_FormatAsUint64LittleEndian(chunk))
        outfile.write(b',\n')

      outfile.write(('};\n'
                     'const EmbeddedFile %(name)s = {\n'
                     '  %(name)s_data,\n'
                     '  %(size)d,\n'
                     '};\n'
                     '#endif  // MOZC_EMBEDDED_FILE_%(name)s\n' % {
                         'name': opts.name,
                         'size': os.stat(opts.input).st_size
                     }).encode('utf-8'))


if __name__ == '__main__':
  main()
