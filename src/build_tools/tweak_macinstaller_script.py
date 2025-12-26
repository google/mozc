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

"""Fix @@@...@@@ format of variables in the installer script templates for Mac.

% python tweak_macinstaller_script.py --output=out.txt --input=in.txt
  [--version_file=version.txt] [--build_type=dev]
"""

import argparse
import os

from build_tools import mozc_version


def _ReplaceVariables(data, rules):
  """Replace all occurrence of the variable in data by the value.

  Args:
    data: the original data string
    rules: an iterable of (variable name, its value) pairs

  Returns:
    the data string which replaces the variables by the value.
  """
  result = data
  for k, v in rules:
    result = result.replace(k, v)
  return result


def ParseOptions():
  """Parse command line options.

  Returns:
    An options data.
  """
  parser = argparse.ArgumentParser()
  parser.add_argument('--version_file', dest='version_file')
  parser.add_argument('--output', dest='output', required=True)
  parser.add_argument('--input', dest='input', required=True)
  parser.add_argument('--build_type', dest='build_type', default='stable')
  parser.add_argument(
      '--use_mozc_version_env',
      dest='use_mozc_version_env',
      action='store_true',
      default=False,
      help=(
          'If true and MOZC_VERSION environment variable is set, the variable'
          ' is used as the version string. Otherwise the template file is used.'
          ' The value is a four-digit number (e.g. 2.31.5840.0) or a single'
          ' build number (e.g. 5840).'
      ),
  )

  return parser.parse_args()


def main():
  """The main function."""
  options = ParseOptions()

  mozc_version_env = os.getenv('MOZC_VERSION')
  version_placeholder = '@@@MOZC_VERSION@@@'

  if options.use_mozc_version_env and mozc_version_env:
    version = mozc_version_env
  elif options.version_file:
    version = mozc_version.MozcVersion(options.version_file).GetVersionString()
  else:
    version = version_placeholder

  if options.build_type == 'dev':
    omaha_tag = 'external-dev'
  else:
    omaha_tag = 'external-stable'

  variables = [
      (
          '@@@MOZC_APPLICATIONS_DIR@@@',
          '/Applications/GoogleJapaneseInput.localized',
      ),
      ('@@@MOZC_APP_PATH@@@', '/Library/Input Methods/GoogleJapaneseInput.app'),
      ('@@@MOZC_OMAHA_TAG@@@', omaha_tag),
      ('@@@MOZC_PACKAGE_NAME@@@', 'GoogleJapaneseInput.pkg'),
      ('@@@MOZC_PRODUCT_ID@@@', 'com.google.JapaneseIME'),
      ('@@@MOZC_VERSION@@@', version),
  ]

  replaced = _ReplaceVariables(open(options.input).read(), variables)
  if version_placeholder in replaced:
    raise ValueError(
        f'Version placeholder {version_placeholder} is not replaced'
    )

  with open(options.output, 'w') as f:
    f.write(_ReplaceVariables(open(options.input).read(), variables))


if __name__ == '__main__':
  main()
