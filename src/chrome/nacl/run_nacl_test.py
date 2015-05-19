# -*- coding: utf-8 -*-
# Copyright 2010-2013, Google Inc.
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

"""A script to run unittests using sel_ldr (NaCl Simple/Secure ELF Loader).

This file searches all tests under --test_bin_dir (the default is the current
directory), and run them using sel_ldr (NaCl Simple/Secure ELF Loader).
"""

import logging
import optparse
import os
import subprocess


def ParseArgs():
  """Parses command line options and returns them."""
  parser = optparse.OptionParser()
  parser.add_option('--nacl_sdk_root', dest='nacl_sdk_root', default='.',
                    help='A path to the root directory of Native Client SDK.')
  parser.add_option('--test_bin_dir', dest='test_bin_dir', default='.',
                    help='The directory which contains test binaries.')
  return parser.parse_args()[0]


def FindTestBinaries(test_bin_dir):
  """Returns the file names of tests."""
  logging.info('Gathering binaries in %s.', os.path.abspath(test_bin_dir))
  result = []
  for f in os.listdir(test_bin_dir):
    test_bin_path = os.path.join(test_bin_dir, f)
    if (os.access(test_bin_path, os.R_OK | os.X_OK) and
        os.path.isfile(test_bin_path) and
        f.endswith('_test_x86_64.nexe')):
      result.append(test_bin_path)
  return result


def RunCommand(options, command):
  """Run a command on android via sel_ldr."""
  args = [os.path.join(options.nacl_sdk_root, 'tools', 'sel_ldr_x86_64'),
          '-B',
          os.path.join(options.nacl_sdk_root, 'tools', 'irt_core_x86_64.nexe')]
  args.extend(command)
  process = subprocess.Popen(args, stdout=subprocess.PIPE)
  output, _ = process.communicate()
  logging.info(output)
  if process.returncode != 0:
    raise StandardError('Failed to run the command: ' + ' '.join(args))
  return output


def main():
  # Enable logging.info.
  logging.getLogger().setLevel(logging.INFO)
  options = ParseArgs()
  test_binaries = FindTestBinaries(options.test_bin_dir)
  for test in test_binaries:
    RunCommand(options, [test])


if __name__ == '__main__':
  main()
