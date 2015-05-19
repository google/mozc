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

"""Utilitis for build_mozc script."""
#TODO(nona): implement unittests.

__author__ = "nona"

import logging
import multiprocessing
import os
import shutil
import stat
import subprocess
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


def GetNumberOfProcessors():
  """Returns the number of CPU cores available.

  Returns:
    An integer which represents the number of CPU cores available on
    the host environment. Returns 1 if something fails.
  TODO(nona): Support colorized error message
  """
  try:
    return multiprocessing.cpu_count()
  except NotImplementedError as e:
    logging.warning('Failed to retrieve the CPU count. Error: %s.', e)
  return 1


class RunOrDieError(StandardError):
  """The exception class for RunOrDie."""

  def __init__(self, message):
    StandardError.__init__(self, message)


def RunOrDie(argv):
  """Run the command, or die if it failed."""
  # Rest are the target program name and the parameters, but we special
  # case if the target program name ends with '.py'
  if argv[0].endswith('.py'):
    argv.insert(0, sys.executable)  # Inject the python interpreter path.
  # We don't capture stdout and stderr from Popen. The output will just
  # be emitted to a terminal or console.
  # TODO(nona): Support colorized error message
  print 'Running: ' + ' '.join(argv)
  process = subprocess.Popen(argv)

  if process.wait() != 0:
    raise RunOrDieError('\n'.join(['',
                                   '==========',
                                   ' ERROR: %s' % ' '.join(argv),
                                   '==========']))


def RemoveFile(file_name):
  """Removes the specified file."""
  if not os.path.isfile(file_name):
    return  # Do nothing if not exist.
  if IsWindows():
    # Read-only files cannot be deleted on Windows.
    os.chmod(file_name, 0700)
  print 'Removing file: %s' % file_name
  os.unlink(file_name)


def CopyFile(source, destination):
  """Copies a file to the destination. Remove an old version if needed."""
  if os.path.isfile(destination):  # Remove the old one if exists.
    RemoveFile(destination)
  dest_dirname = os.path.dirname(destination)
  if not os.path.isdir(dest_dirname):
    os.makedirs(dest_dirname)
  # TODO(nona): Support colorized error message
  print 'Copying file to: %s' % destination
  shutil.copy(source, destination)


def RemoveDirectoryRecursively(directory):
  """Removes the specified directory recursively."""
  if os.path.isdir(directory):
    # TODO(nona): Support colorized error message
    print 'Removing directory: %s' % directory
    if IsWindows():
      # Use RD because shutil.rmtree fails when the directory is readonly.
      RunOrDie(['CMD.exe', '/C', 'RD', '/S', '/Q',
                os.path.normpath(directory)])
    else:
      shutil.rmtree(directory, ignore_errors=True)


def MakeFileWritableRecursively(path):
  """Make files (including directories) writable recursively."""
  for root, dirs, files in os.walk(path):
    for name in dirs + files:
      path = os.path.join(root, name)
      os.chmod(path, os.lstat(path).st_mode | stat.S_IWRITE)


def PrintErrorAndExit(error_message):
  """Prints the error message and exists."""
  # TODO(nona): Support colorized error message
  print '\n==========\n ERROR: %s\n==========\n' % error_message
  sys.exit(1)


def GetRelPath(path, start):
  """Return a relative path to |path| from |start|."""
  # NOTE: Python 2.6 provides os.path.relpath, which has almost the same
  # functionality as this function. Since Python 2.6 is not the internal
  # official version, we reimplement it.
  path_list = os.path.abspath(os.path.normpath(path)).split(os.sep)
  start_list = os.path.abspath(os.path.normpath(start)).split(os.sep)

  common_prefix_count = 0
  for i in range(0, min(len(path_list), len(start_list))):
    if path_list[i] != start_list[i]:
      break
    common_prefix_count += 1

  return os.sep.join(['..'] * (len(start_list) - common_prefix_count) +
                     path_list[common_prefix_count:])


def CheckFileOrDie(file_name):
  """Check the file exists or dies if not."""
  if not os.path.isfile(file_name):
    PrintErrorAndExit('No such file: ' + file_name)
