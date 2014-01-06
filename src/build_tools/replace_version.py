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

"""Replace version definitions written in the specified file.

  % python replace_version.py --output=out.txt --input=in.txt \
      --version_file=version.txt

See mozc_version.py for the detailed information for version.txt.
"""

__author__ = "mukai"

import logging
import optparse

from build_tools import mozc_version


def ParseOptions():
  """Parse command line options.

  Returns:
    An options data.
  """
  parser = optparse.OptionParser()
  parser.add_option('--version_file', dest='version_file')
  parser.add_option('--output', dest='output')
  parser.add_option('--input', dest='input')
  parser.add_option('--branding', dest='branding')

  (options, unused_args) = parser.parse_args()
  return options


def GetBrandingName(version, branding):
  """Returns the branding name dictionary."""
  branding_name = {
      'APP_NAME_JA': 'Mozc',
      'APP_NAME_EN': 'Mozc',
      'BRANDING': 'Mozc',
      'DEV_CHANNEL': 'false',
      'OFFICIAL_BUILD': 'false',
  }
  if branding == 'GoogleJapaneseInput':
    branding_name['BRANDING'] = 'GoogleJapaneseInput'
    branding_name['OFFICIAL_BUILD'] = 'true'
    if version.IsDevChannel():
      branding_name['APP_NAME_JA'] = 'Google 日本語入力 開発版'
      branding_name['APP_NAME_EN'] = 'Google Japanese Input Dev Channel'
      branding_name['DEV_CHANNEL'] = 'true'
    else:
      branding_name['APP_NAME_JA'] = 'Google 日本語入力'
      branding_name['APP_NAME_EN'] = 'Google Japanese Input'
  return branding_name


def main():
  """The main function."""
  options = ParseOptions()
  if options.version_file is None:
    logging.error('--version_file is not specified.')
    exit(-1)
  if options.output is None:
    logging.error('--output is not specified.')
    exit(-1)
  if options.input is None:
    logging.error('--input is not specified.')
    exit(-1)
  if options.branding is None:
    logging.error('--branding is not specified.')
    exit(-1)

  version = mozc_version.MozcVersion(options.version_file)
  branding_name = GetBrandingName(version, options.branding)
  with open(options.input) as f:
    result = f.read()
  result = version.GetVersionInFormat(result)
  for (key, value) in branding_name.items():
    result = result.replace('@%s@' % key, value)

  with open(options.output, 'w') as f:
    f.write(result)


if __name__ == '__main__':
  main()
