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

"""Generate the Mozc data version string.

Data version consists of three components:

<ENGINE_VERSION>.<DATA_VERSION>.<TAG>

Here, <TAG> is any string to distinguish data set.
"""

import optparse
import re


def _ParseOption():
  """Parse command line options."""
  parser = optparse.OptionParser()
  parser.add_option('--tag', dest='tag')
  parser.add_option('--mozc_version_template', dest='mozc_version_template')
  parser.add_option('--output', dest='output')
  return parser.parse_args()[0]


def main():
  opts = _ParseOption()
  data = {}
  with open(opts.mozc_version_template, 'r') as f:
    for line in f:
      matchobj = re.match(r'(\w+) *= *(.*)', line.strip())
      if matchobj:
        key = matchobj.group(1)
        value = matchobj.group(2)
        data[key] = value

  with open(opts.output, 'w') as f:
    f.write('.'.join((data['ENGINE_VERSION'], data['DATA_VERSION'], opts.tag)))


if __name__ == '__main__':
  main()
