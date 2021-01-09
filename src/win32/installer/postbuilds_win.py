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

"""Script post-prosessing executables for Windows.

postbuilds_win.py --targetpath=my_binary.exe
"""

from __future__ import print_function

__author__ = "yukawa"

import optparse
import os
import re
import subprocess
import sys


def IsWindows():
  """Returns true if the platform is Windows."""
  return os.name == 'nt'


def ParseOption():
  """Parse command line options."""
  parser = optparse.OptionParser()
  parser.add_option('--targetpath', dest='targetpath')
  parser.add_option('--pdbpath', dest='pdbpath', default='')

  (opts, _) = parser.parse_args()

  return opts


  # Touch the timestamp file.
  if os.path.exists(abs_touch_file_path):
    os.utime(abs_touch_file_path, None)
  else:
    open(abs_touch_file_path, 'w').close()


class RunOrDieError(Exception):
  """The exception class for RunOrDie."""

  def __init__(self, message):
    Exception.__init__(self, message)


def RunOrDie(argv):
  """Run the command, or die if it failed."""

  # Rest are the target program name and the parameters, but we special
  # case if the target program name ends with '.py'
  if argv[0].endswith('.py'):
    argv.insert(0, sys.executable)  # Inject the python interpreter path.
  # We don't capture stdout and stderr from Popen. The output will just
  # be emitted to a terminal or console.
  process = subprocess.Popen(argv, stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE)
  out, err = process.communicate()
  if process.wait() != 0:
    raise RunOrDieError('\n'.join(['',
                                   '==========',
                                   ' ERROR: %s' % ' '.join(argv),
                                   ' Stdout', out,
                                   ' Stderr', err,
                                   '==========']))


def PrintErrorAndExit(error_message):
  """Prints the error message and exists."""
  print(error_message)
  sys.exit(1)


def ShowHelpAndExit():
  """Shows the help message."""
  print('Usage: postbuilds_win.py [ARGS]')
  print('This script only supports Windows')
  print('See also the comment in the script for typical usage.')
  sys.exit(1)


def main():
  opts = ParseOption()

  if not opts.targetpath:
    print('--targetpath is not specified')
    sys.exit(1)

  if IsWindows():
    PostProcessOnWindows(opts)
  else:
    ShowHelpAndExit()

if __name__ == '__main__':
  main()
