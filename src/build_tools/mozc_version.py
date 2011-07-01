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

"""A library to get the version info from version definition file.

Here "mozc_version.txt" file has to be the following format:
MAJOR=0
MINOR=9
BUILD=248
REVISION=0
"""

import datetime
import logging
import os
import re
import sys


def IsWindows():
  """Returns true if the platform is Windows."""
  return os.name == 'nt'


def IsMac():
  """Returns true if the platform is Mac."""
  return os.name == 'posix' and os.uname()[0] == 'Darwin'


def IsLinux():
  """Returns true if the platform is Linux."""
  return os.name == 'posix' and os.uname()[0] == 'Linux'


def CalculateRevisionForPlatform(revision, target_platform):
  """Returns the revision for the current platform."""
  if not revision:
    return revision
  if target_platform == 'ChromeOS':
    last_digit = '4'
  elif IsWindows():
    last_digit = '0'
  elif IsMac():
    last_digit = '1'
  elif IsLinux():
    last_digit = '2'
  if last_digit:
    return revision[0:-1] + last_digit

  # If not supported, just use the specified version.
  return revision


class MozcVersion(object):
  """A class to parse and maintain the version definition data."""

  def __init__(self, path, expand_daily=False, target_platform=None):
    self.ParseVersionFile(path, expand_daily, target_platform)

  def ParseVersionFile(self, path, expand_daily, target_platform):
    """Parses a version definition file.

    Args:
      path: A filename which has the version definition.
      expand_daily: True if 'daily' is expanded to the build number.
      target_platform: The target platform on which the programs run.
    """
    self._dict = {}
    for line in open(path):
      matchobj = re.match(r'(\w+)=(.*)', line.strip())
      if matchobj:
        var = matchobj.group(1)
        val = matchobj.group(2)
        if var not in self._dict:
          self._dict[var] = val

    # Set the actual version from the parsed data.
    self._major = self._dict['MAJOR']
    self._minor = self._dict['MINOR']
    self._build = self._dict['BUILD']
    self._revision = CalculateRevisionForPlatform(self._dict['REVISION'],
                                                  target_platform)
    self._flag = self._dict.get('FLAG', 'RELEASE')
    if expand_daily:
      zero_day = datetime.date(2009, 5, 24)
      num_of_days = datetime.date.today().toordinal() - zero_day.toordinal()
      if self._build == 'daily':
        self._build = str(num_of_days)
        if 'FLAG' not in self._dict:
          self._flag = 'CONTINUOUS'

  def IsDevChannel(self):
    """Returns true if the parsed version is dev-channel."""
    if len(self._revision) >= 3 and self._revision[-3] == '1':
      return True

    return False

  def GetVersionString(self):
    """Returns the normal version info string.

    Returns:
      a string in format of "MAJOR.MINOR.BUILD.REVISION"
    """
    return self.GetVersionInFormat('@MAJOR@.@MINOR@.@BUILD@.@REVISION@')

  def GetVersionInFormat(self, format):
    """Returns the version string based on the specified format.

    format can contains @MAJOR@, @MINOR@, @BUILD@ and @REVISION@ which are
    replaced by self._major, self._minor, self._build, and self._revision
    respectively.

    Args:
      format: a string which contains version patterns.

    Returns:
      Return the version string in the format of format.
    """
    replacements = [('@MAJOR@', self._major), ('@MINOR@', self._minor),
                    ('@BUILD@', self._build), ('@REVISION@', self._revision),
                    ('@FLAG@', self._flag)]
    result = format
    for r in replacements:
      result = result.replace(r[0], r[1])
    return result


def main():
  """A sample main function."""
  if len(sys.argv) < 2:
    logging.error('The number of argument is not enough')
    exit(1)

  version = MozcVersion(sys.argv[1], expand_daily=True)
  print version.GetVersionString()

if __name__ == '__main__':
  main()
