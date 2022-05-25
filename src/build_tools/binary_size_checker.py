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

"""A checker tool to make sure if the binary size is too huge.

Usage:
  % python binary_size_checker.py --target_directory=out_linux/Debug
"""

import logging
import optparse
import os
import sys


# The expected maximum size of each package and binary.  Each size is
# in Megabytes.
EXPECTED_MAXIMUM_SIZES = {
    # Distribution package files:
    'GoogleJapaneseInput.dmg': 70,
    'GoogleJapaneseInput32.msi': 65,
    'GoogleJapaneseInput64.msi': 65,
    }


def CheckFileSize(filename):
  """Check the size of filename.

  Args:
    filename: the file name to be scanned.

  Returns:
    True is returned when filename is not target or does not exceed the limit.
  """
  basename = os.path.basename(filename)
  if basename not in EXPECTED_MAXIMUM_SIZES:
    return True

  actual_size = os.stat(filename).st_size
  expected_size = EXPECTED_MAXIMUM_SIZES[basename]
  if actual_size < expected_size * 1024 * 1024:
    print('Pass: %s (size: %d) is smaller than expected (%d MB)' % (
        filename, actual_size, expected_size))
    return True
  else:
    print('WARNING: %s (size: %d) is larger than expected (%d MB)' % (
        filename, actual_size, expected_size))
    return False


def ScanAndCheckSize(directory):
  """Scan the specified directory and check the binary/package size.

  Args:
    directory: the directory file name to be scanned.

  Returns:
    True is returned when all criteria is passed.
  """
  result = True
  for filename in os.listdir(directory):
    result &= CheckFileSize(os.path.join(directory, filename))
  return result


def ParseOptions():
  """Parse command line options.

  Returns:
    The options data passed.
  """
  parser = optparse.OptionParser()
  parser.add_option('--target_directory', dest='target_directory',
                    help='The directory which will contain target binaries '
                    'and packages.')
  parser.add_option('--target_filename', dest='target_filename',
                    help='The target filenames (comma separated).')
  (options, unused_args) = parser.parse_args()
  return options


def main():
  """The main function."""
  options = ParseOptions()
  if not (options.target_directory or options.target_filename):
    logging.error('--target_directory or --target_filename is not specified.')
    sys.exit(-1)

  if options.target_directory:
    if not ScanAndCheckSize(options.target_directory):
      logging.error('Files in target_directory exceeds the limit.')
      sys.exit(-1)
  if options.target_filename:
    for filename in options.target_filename.split(','):
      if not CheckFileSize(filename):
        logging.error('The target_filename exceeds the limit.')
        sys.exit(-1)


if __name__ == '__main__':
  main()
