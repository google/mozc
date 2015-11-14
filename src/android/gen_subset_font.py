# -*- coding: utf-8 -*-
# Copyright 2010-2015, Google Inc.
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

"""Generates subset font.

1. Corrects characters from .svg files in given directory.
  Only text nodes in <text> element are taken into account.
2. Make subset font which contains only the characters corrected above.
"""

import logging
import optparse
import os
import shutil
import subprocess
import sys
import tempfile
from xml.etree import cElementTree as ElementTree

from build_tools import util


def ExtractCharactersFromSvgFile(svg_file):
  result = set()
  for text in ElementTree.parse(svg_file).findall(
      './/{http://www.w3.org/2000/svg}text'):
    if text.text:
      result.update(tuple(text.text))  # Split into characters.
  logging.debug('%s: %s', svg_file, result)
  return result


def ExtractCharactersFromDirectory(svg_paths):
  result = set()
  for dirpath, _, filenames in util.WalkFileContainers(svg_paths):
    for filename in filenames:
      if os.path.splitext(filename)[1] != '.svg':
        # Do nothing for files other than svg.
        continue
      result.update(ExtractCharactersFromSvgFile(
          os.path.join(dirpath, filename)))
  return list(result)


def MakeSubsetFont(fonttools_path, unicodes, input_font, output_font):
  """Makes subset font using fontTools toolchain.

  Args:
    fonttools_path: Path to fontTools library.
    unicodes: A list of unicode characters of which glyph should be in
              the output.
    input_font: Path to input font.
    output_font: Path to output font.
  """
  if not unicodes:
    # subset.py requires at least one unicode character.
    logging.debug('No unicode character is specified. Use "A" as stub.')
    unicodes = [u'A']

  tempdir = tempfile.mkdtemp()
  try:
    # fontTools's output file name is fixed (input file path + '.subset').
    # To get result with specified file name, copy the input font
    # to temporary directory and copy the result with renaming.
    shutil.copy(input_font, tempdir)
    temp_input_font = os.path.join(tempdir, os.path.basename(input_font))
    python_cmd = 'python'
    commands = [python_cmd, os.path.join(fonttools_path, 'subset.py'),
                temp_input_font]
    commands.extend(['U+%08x' % ord(char) for char in unicodes])
    env = os.environ.copy()
    # In fontTools toolchain, "from fontTools import XXXX" is executed.
    # In order for successfull import, add parent directory of the toolchain
    # into PYTHONPATH.
    env['PYTHONPATH'] += ':%s' % os.path.join(fonttools_path, '..')
    if subprocess.call(commands, env=env) != 0:
      sys.exit(1)
    shutil.copyfile('%s.subset' % temp_input_font, output_font)
  finally:
    shutil.rmtree(tempdir)


def ParseOptions():
  """Parses options."""
  parser = optparse.OptionParser()
  parser.add_option('--svg_paths', dest='svg_paths',
                    help='Comma separated paths to a directory or'
                    ' a .zip file containing .svg files.')
  parser.add_option('--input_font', dest='input_font',
                    help='Path to the input font.')
  parser.add_option('--output_font', dest='output_font',
                    help='Path to the output font.')
  parser.add_option('--fonttools_path', dest='fonttools_path',
                    help='Path to fontTools toolchain.')
  parser.add_option('--verbose', dest='verbose',
                    help='Verbosity')
  return parser.parse_args()[0]


def main():
  logging.basicConfig(level=logging.INFO, format='%(levelname)s: %(message)s')

  options = ParseOptions()
  if options.verbose:
    logging.getLogger().setLevel(logging.DEBUG)
  unicodes = ExtractCharactersFromDirectory(options.svg_paths)
  MakeSubsetFont(options.fonttools_path, unicodes,
                 options.input_font, options.output_font)


if __name__ == '__main__':
  main()
