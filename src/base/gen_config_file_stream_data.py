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

"""Script to generate config_file_stream_data.h."""

import argparse
import os.path


def ParseArguments():
  """Parses options from given argument."""
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--output',
      type=argparse.FileType(mode='w', encoding='utf-8'),
      help='Output file path',
  )
  parser.add_argument('input', nargs='+', help='Input file paths')
  return parser.parse_args()


def OutputConfigFileStreamData(path_list, output):
  r"""Outputs the generated FileData entries for path_list.

  Prints the config_file_stream_data.h file, which contains FileData entries
  of files in given path_list, to the output stream.
  The generated code looks like:

  constexpr FileData kFileData[] = {
    {"filename1", "\x00\x01\x02\x03"},
    {"filename2", "\x10\x11\x12\x13\x14\x15\x16\x17"},
        :
        :
  };

  Args:
    path_list: a list of file path which should be included in the
      config_file_stream_data as FileData entries.
    output: a stream of the output data.
  """
  output.write('constexpr FileData kFileData[] = {\n')
  for path in path_list:
    output.write('  {"%s", "' % os.path.basename(path))
    with open(path, 'rb') as stream:
      while byte := stream.read(1):
        output.write(r'\x' + byte.hex())
    output.write('"},\n')
  output.write('};\n')


def main():
  args = ParseArguments()

  OutputConfigFileStreamData(args.input, args.output)


if __name__ == '__main__':
  main()
