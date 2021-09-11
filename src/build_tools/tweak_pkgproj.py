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

"""Fix all version info in the input Info.plist file.

  % python tweak_info_plist.py --output=out.txt --input=in.txt
      --build_dir=/path/to/xcodebuild --gen_out_dir=/path/to/gen_out_dir
      --keystone_dir=/path/to/KeyStone --version_file=version.txt

See mozc_version.py for the detailed information for version.txt.
"""

__author__ = "mukai"

import datetime
import logging
import optparse
import os
import re

from build_tools import mozc_version

_COPYRIGHT_YEAR = datetime.date.today().year


def _ReplaceVariables(data, environment):
  """Replace all occurrence of the variable in data by the value.

  Args:
    data: the original data string
    environment: a dict which maps from variable names to their values

  Returns:
    the data string which replaces the variables by the value.
  """

  def Replacer(matchobj):
    """The replace function to expand variables.

    Args:
      matchobj: the match object

    Returns:
      The value for the variable if the variable is defined in the
      environment.  Otherwise original string is returned.
    """
    if matchobj.group(1) in environment:
      return environment[matchobj.group(1)]
    return matchobj.group(0)

  return re.sub(r'\$\{(\w+)\}', Replacer, data)


def _RemoveDevOnlyLines(data, build_type):
  """Remove dev-only lines.

  Args:
    data: the original data string
    build_type: build type ("dev" or "stable")

  Returns:
    if build_type == "dev"
      the data string including dev-only lines.
    else:
      the data string excluding dev-only lines.
  """
  pat = re.compile('<!--DEV_ONLY_START-->\n(.*)<!--DEV_ONLY_END-->\n',
                   re.DOTALL)
  if build_type == 'dev':
    return re.sub(pat, r'\1', data)
  else:
    return re.sub(pat, '', data)


def ParseOptions():
  """Parse command line options.

  Returns:
    An options data.
  """
  parser = optparse.OptionParser()
  parser.add_option('--version_file', dest='version_file')
  parser.add_option('--output', dest='output')
  parser.add_option('--input', dest='input')
  parser.add_option('--build_dir', dest='build_dir')
  parser.add_option('--gen_out_dir', dest='gen_out_dir')
  parser.add_option('--auto_updater_dir', dest='auto_updater_dir')
  parser.add_option('--mozc_dir', dest='mozc_dir')
  parser.add_option('--launch_agent_dir', dest='launch_agent_dir')
  parser.add_option('--build_type', dest='build_type')

  (options, unused_args) = parser.parse_args()
  return options


def main():
  """The main function."""
  options = ParseOptions()
  required_flags = [
      'version_file', 'output', 'input', 'build_dir', 'gen_out_dir',
      'auto_updater_dir', 'build_type', 'launch_agent_dir',
  ]
  for flag in required_flags:
    if getattr(options, flag) is None:
      logging.error('--%s is not specified.', flag)
      exit(-1)

  version = mozc_version.MozcVersion(options.version_file)

  copyright_message = '© %d Google Inc.' % _COPYRIGHT_YEAR
  long_version = version.GetVersionString()
  short_version = version.GetVersionInFormat('@MAJOR@.@MINOR@.@BUILD@')
  if options.mozc_dir:
    mozc_dir = options.mozc_dir
  else:
    mozc_dir = os.path.abspath(os.path.join(os.getcwd(), '..'))

  variables = {
      'MOZC_VERSIONINFO_MAJOR':
          version.GetVersionInFormat('@MAJOR@'),
      'MOZC_VERSIONINFO_MINOR':
          version.GetVersionInFormat('@MINOR@'),
      'MOZC_VERSIONINFO_LONG':
          long_version,
      'MOZC_VERSIONINFO_SHORT':
          short_version,
      'MOZC_VERSIONINFO_FINDER':
          'Google Japanese Input %s, %s' % (long_version, copyright_message),
      'GEN_OUT_DIR':
          os.path.abspath(options.gen_out_dir),
      'BUILD_DIR':
          os.path.abspath(options.build_dir),
      'AUTO_UPDATER_DIR':
          os.path.abspath(options.auto_updater_dir),
      'MOZC_DIR':
          mozc_dir,
      'LAUNCH_AGENT_DIR':
          os.path.abspath(options.launch_agent_dir),
  }

  with open(options.input, encoding='utf-8') as input_file:
    with open(options.output, 'w', encoding='utf-8') as output_file:
      output_file.write(
          _RemoveDevOnlyLines(_ReplaceVariables(input_file.read(), variables),
                              options.build_type))


if __name__ == '__main__':
  main()
