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

"""Capture stdout output from a program and save it to a file.

  % python redirect.py out.txt echo foo
  % cat out.txt
  foo

This is essentially identical to 'echo foo > out.txt' but the script
can be used in platforms without a Unix shell (i.e. windows).
"""

__author__ = "satorux"

import subprocess
import sys


def main():
  """The main function."""
  # Delete the self program name (i.e. redirect.py).
  sys.argv.pop(0)
  # The first parameter is the output file name.
  output_file_name = sys.argv.pop(0)
  # Rest are the target program name and the parameters, but we special
  # case if the target program name ends with '.py'
  if sys.argv[0].endswith('.py'):
    sys.argv.insert(0, sys.executable)  # Inject the python interpreter path.
  try:
    process = subprocess.Popen(sys.argv, stdout=subprocess.PIPE,
                               universal_newlines=True)
  except:
    print '=========='
    print ' ERROR: %s' % ' '.join(sys.argv)
    print '=========='
    raise
  (stdout_content, _) = process.communicate()
  # Write the stdout content to the output file.
  output_file = open(output_file_name, 'w')
  output_file.write(stdout_content)
  return process.wait()

if __name__ == '__main__':
  sys.exit(main())
