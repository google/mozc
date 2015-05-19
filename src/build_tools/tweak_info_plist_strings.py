# -*- coding: utf-8 -*-
# Copyright 2010-2014, Google Inc.
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

"""Replace variables in InfoPlist.strings with file.

  % python tweak_info_plist_strings.py --output=out.txt --input=in.txt \
      --branding=Mozc
"""

__author__ = "komatsu"

import datetime
import logging
import optparse
import sys
import tweak_data

_COPYRIGHT_YEAR = datetime.date.today().year

def ParseOptions():
  """Parse command line options.

  Returns:
    An options data.
  """
  parser = optparse.OptionParser()
  parser.add_option("--output", dest="output")
  parser.add_option("--input", dest="input")
  parser.add_option("--branding", dest="branding")

  (options, unused_args) = parser.parse_args()
  return options


def main():
  """The main function."""
  options = ParseOptions()
  if options.output is None:
    logging.error("--output is not specified.")
    sys.exit(-1)
  if options.input is None:
    logging.error("--input is not specified.")
    sys.exit(-1)
  if options.branding is None:
    logging.error("--branding is not specified.")
    sys.exit(-1)

  # \xC2\xA9 is the copyright mark in UTF-8
  copyright_message = '\xC2\xA9 %d Google Inc.' % _COPYRIGHT_YEAR
  if options.branding == 'GoogleJapaneseInput':
    variables = {
        'CF_BUNDLE_NAME_EN': 'Google Japanese Input',
        'CF_BUNDLE_NAME_JA': u'Google 日本語入力'.encode('utf-8'),
        'NS_HUMAN_READABLE_COPYRIGHT': copyright_message,
        'INPUT_MODE_ANNOTATION': 'Google',
        }
  else:
    variables = {
        'CF_BUNDLE_NAME_EN': 'Mozc',
        'CF_BUNDLE_NAME_JA': 'Mozc',
        'NS_HUMAN_READABLE_COPYRIGHT': copyright_message,
        'INPUT_MODE_ANNOTATION': 'Mozc',
        }

  open(options.output, 'w').write(
      tweak_data.ReplaceVariables(open(options.input).read(), variables))

if __name__ == '__main__':
    main()
