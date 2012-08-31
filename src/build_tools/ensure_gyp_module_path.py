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

"""A script to ensure 'import gyp' loads a gyp module from expected location.

Example:
./ensure_gyp_module_path.py
    --expected=/home/foobar/work/mozc/src/third_party/gyp/pylib/gyp
"""
__author__ = "yukawa"

import optparse
import os
import sys


def ParseOption():
  """Parse command line options."""
  parser = optparse.OptionParser()
  parser.add_option('--expected', dest='expected')

  (options, _) = parser.parse_args()
  if not options.expected:
    print parser.print_help()
    sys.exit(1)

  return options


def main():
  """Script to ensure gyp module location."""
  opt = ParseOption()
  expected_path = os.path.abspath(opt.expected)
  if not os.path.exists(expected_path):
    print '%s does not exist.' % expected_path
    sys.exit(1)

  try:
    import gyp  # NOLINT
  except ImportError as e:
    print 'import gyp failed: %s' % e
    sys.exit(1)

  actual_path = os.path.abspath(gyp.__path__[0])
  if expected_path != actual_path:
    print 'Unexpected gyp module is loaded on this environment.'
    print '  expected: %s' % expected_path
    print '  actual  : %s' % actual_path
    sys.exit(1)

if __name__ == '__main__':
  main()
