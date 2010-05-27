# -*- coding: utf-8 -*-
# Copyright 2010, Google Inc.
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

"""Script to run an arbitrary program after changing the current directory.

  % python run_after_chdir.py / ls
  auto emul home lib  media proc srv var
  ...

The example above is to run ls command in / directory.
"""

__author__ = "satorux"

import os
import subprocess
import sys


def main():
  """The main function."""
  # Delete the self program name (i.e. run_after_chdir.py).
  sys.argv.pop(0)
  # The first parameter is the destination directory.
  destination_directory_name = sys.argv.pop(0)
  os.chdir(destination_directory_name)
  # Rest are the target program name and the parameters, but we special
  # case if the target program name ends with '.py'
  if sys.argv[0].endswith('.py'):
    sys.argv.insert(0, sys.executable)  # Inject the python interpreter path.
  # We don't capture stdout and stderr from Popen. The output will just
  # be emitted to a terminal or console.
  print sys.argv
  process = subprocess.Popen(sys.argv)

if __name__ == '__main__':
  main()
