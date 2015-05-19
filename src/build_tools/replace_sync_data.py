# -*- coding: utf-8 -*-
# Copyright 2010-2012, Google Inc.
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

"""Replace sync client data written in the specified file.

  % python replace_sync_data.py --output=out.txt --input=in.txt \
      --sync_data_file=data.txt
"""

__author__ = "mukai"

import logging
import optparse
import re

import tweak_data


def ParseSyncData(filename):
  """Parse the sync data file and create variables mappings.

  Args:
    filename: The filename to be read.

  Returns:
    A dict which holds the mappings from variable names to their values.
  """
  result = {}
  for line in open(filename):
    matchobj = re.match(r'(\w+)=(.*)', line.strip())
    if matchobj:
      var = matchobj.group(1)
      val = matchobj.group(2)
      if var not in result:
        result[var] = val
  return result


def ParseOptions():
  """Parse command line options.

  Returns:
    An options data.
  """
  parser = optparse.OptionParser()
  parser.add_option("--sync_data_file", dest="sync_data_file")
  parser.add_option("--output", dest="output")
  parser.add_option("--input", dest="input")
  parser.add_option("--target_platform", dest="target_platform")

  (options, unused_args) = parser.parse_args()
  return options


def main():
  """The main function."""
  options = ParseOptions()
  if options.sync_data_file is None:
    logging.error("--version_file is not specified.")
    exit(-1)
  if options.output is None:
    logging.error("--output is not specified.")
    exit(-1)
  if options.input is None:
    logging.error("--input is not specified.")
    exit(-1)

  variables = ParseSyncData(options.sync_data_file)
  open(options.output, 'w').write(
      tweak_data.ReplaceVariables(
          open(options.input).read(), variables))


if __name__ == '__main__':
  main()
