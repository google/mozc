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

"""Fix all version info in the input Info.plist file.

  % python tweak_info_plist.py --output=out.txt --input=in.txt \
      --branding=Mozc --version_file=version.txt

See mozc_version.py for the detailed information for version.txt.
"""

__author__ = "mukai"

import logging
import optparse
import re
import mozc_version

_COPYRIGHT_YEAR = 2010

def _ReplaceVariables(data, environment):
  """Replace all occurrence of the variable in data by the value.

  Args:
    data: the original data string
    environment: a dict which maps from variable names to their values

  Returns:
    the data string which replaces the variables by the value.
  """
  def Replacer(matchobj):
    """The replace function to expand variables

    Args:
      matchobj: the match object

    Returns:
      The value for the variable if the variable is defined in the
      environment.  Otherwise original string is returned.
    """
    if environment.has_key(matchobj.group(1)):
      return environment[matchobj.group(1)]
    return matchobj.group(0)

  return re.sub(r'\$\{(\w+)\}', Replacer, data)


def ParseOptions():
  """Parse command line options.

  Returns:
    An options data.
  """
  parser = optparse.OptionParser()
  parser.add_option("--version_file", dest="version_file")
  parser.add_option("--output", dest="output")
  parser.add_option("--input", dest="input")
  parser.add_option("--branding", dest="branding")

  (options, unused_args) = parser.parse_args()
  return options


def main():
  """The main function."""
  options = ParseOptions()
  if options.version_file is None:
    logging.error("--version_file is not specified.")
    exit(-1)
  if options.output is None:
    logging.error("--output is not specified.")
    exit(-1)
  if options.input is None:
    logging.error("--input is not specified.")
    exit(-1)
  if options.branding is None:
    logging.error("--branding is not specified.")
    exit(-1)

  version = mozc_version.MozcVersion(options.version_file, expand_daily=False)

  # \xC2\xA9 is the copyright mark in UTF-8
  copyright_message = '\xC2\xA9 %d Google Inc.' % _COPYRIGHT_YEAR
  long_version = version.GetVersionString()
  short_version = version.GetVersionInFormat('@MAJOR@.@MINOR@.@BUILD@')
  domain_prefix = 'org.mozc'
  if options.branding == 'GoogleJapaneseInput':
    domain_prefix = 'com.google'
  variables = {
      'GOOGLE_VERSIONINFO_LONG': long_version,
      'GOOGLE_VERSIONINFO_SHORT': short_version,
      'GOOGLE_VERSIONINFO_ABOUT': copyright_message,
      'GOOGLE_VERSIONINFO_FINDER':
        'Google Japanese Input %s, %s' % (long_version, copyright_message),
      'BRANDING': options.branding,
      'DOMAIN_PREFIX': domain_prefix,
      }

  open(options.output, 'w').write(
      _ReplaceVariables(open(options.input).read(), variables))

if __name__ == '__main__':
    main()
