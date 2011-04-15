# -*- coding: utf-8 -*-
# Copyright 2010-2011, Google Inc.
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

  % python tweak_macinstaller_script.py --output=out.txt --input=in.txt \
    --version_file=version.txt [--build_type=dev] \
"""

__author__ = "mukai"

import logging
import optparse

import mozc_version


def _ReplaceVariables(data, environment):
  """Replace all occurrence of the variable in data by the value.

  Args:
    data: the original data string
    environment: an iterable of (variable name, its value) pairs

  Returns:
    the data string which replaces the variables by the value.
  """
  result = data
  for (k, v) in environment:
    result = result.replace(k, v)
  return result


def ParseOptions():
  """Parse command line options.

  Returns:
    An options data.
  """
  parser = optparse.OptionParser()
  parser.add_option("--version_file", dest="version_file")
  parser.add_option("--output", dest="output")
  parser.add_option("--input", dest="input")
  parser.add_option("--build_type", dest="build_type")

  (options, unused_args) = parser.parse_args()
  return options


def main():
  """The main function."""
  options = ParseOptions()
  required_flags = ["version_file", "output", "input"]
  for flag in required_flags:
    if getattr(options, flag) is None:
      logging.error("--%s is not specified." % flag)
      exit(-1)

  version = mozc_version.MozcVersion(options.version_file, expand_daily=False)

  if options.build_type == "dev":
    omaha_tag = "external-dev"
  else:
    omaha_tag = "external-stable"

  # This definition is copied from tools/scons/script.py
  variables = [
      ("@@@MOZC_VERSION@@@", version.GetVersionString()),
      ("@@@MOZC_PRODUCT_ID@@@", "com.google.JapaneseIME"),
      ("@@@MOZC_APP_PATH@@@", "/Library/Input Methods/GoogleJapaneseInput.app"),
      ("@@@MOZC_APPLICATIONS_DIR@@@",
       "/Application/GoogleJapaneseInput.localized"),
      ("@@@MOZC_OMAHA_TAG@@@", omaha_tag),
      ("@@@MOZC_PACKAGE_NAME@@@", "GoogleJapaneseInput.mpkg"),
      ]

  open(options.output, 'w').write(
      _ReplaceVariables(open(options.input).read(), variables))

if __name__ == '__main__':
  main()
