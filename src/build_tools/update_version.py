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

"""A tool to update the version number in a version file.

python3 update_version.py --version_file=mozc_version_template.bzl
    --version=2.31.5851.100
"""

import argparse
import re
import sys


def parse_args() -> argparse.Namespace:
  """Parse command line options."""
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--version_file',
      help='version file (e.g. mozc_version_template.bzl)',
  )
  parser.add_argument(
      '--version',
      help=(
          'The version number in "MAJOR.MINOR.BUILD.REVISION" format'
          ' (e.g., "2.31.5851.100"). This is used to update the MAJOR, MINOR,'
          ' BUILD,and REVISION variables.'
      ),
  )
  parser.add_argument(
      '--output',
      help='output file. The default is the same as version_file.',
  )
  return parser.parse_args()


def replace_version_number(content: str, variable: str, number: str) -> str:
  # '\b' is a word boundary.
  return re.sub(fr'\b{variable} = \d+', f'{variable} = {number}', content)


def update_version_string(content: str, version: str) -> str:
  """Update the version number in the string."""
  major, minor, build, revision = version.split('.')
  content = replace_version_number(content, 'MAJOR', major)
  content = replace_version_number(content, 'MINOR', minor)
  content = replace_version_number(content, 'BUILD', build)
  content = replace_version_number(content, 'REVISION', revision)
  return content.replace('MOZC_VERSION', version)


def update_version_file(version_file: str, version: str, output: str) -> None:
  """Update the version number in the version file."""
  with open(version_file, 'r') as f:
    content = f.read()
  content = update_version_string(content, version)
  with open(output, 'w') as f:
    f.write(content)


def main():
  args = parse_args()
  if not args.version:
    print('No version is specified')
    # Exit with 0 as no version is an expected behavior.
    sys.exit(0)

  output = args.output if args.output else args.version_file
  update_version_file(args.version_file, args.version, output)


if __name__ == '__main__':
  main()
