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

"""Utilitis for build_mozc script."""

from __future__ import absolute_import
from __future__ import print_function

import logging
import multiprocessing
import os
import shutil
import stat
import subprocess
import sys
import tempfile
import zipfile

from six.moves import range


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
  """
  try:
    return multiprocessing.cpu_count()
  except NotImplementedError as e:
    logging.warning('Failed to retrieve the CPU count. Error: %s.', e)
  return 1


class RunOrDieError(Exception):
  """The exception class for RunOrDie."""

  def __init__(self, message):
    Exception.__init__(self, message)


def RunOrDie(argv):
  """Run the command, or die if it failed."""
  # Rest are the target program name and the parameters, but we special
  # case if the target program name ends with '.py'
  if argv[0].endswith('.py'):
    argv.insert(0, sys.executable)  # Inject the python interpreter path.
  # We don't capture stdout and stderr from Popen. The output will just
  # be emitted to a terminal or console.
  logging.info('Running: %s', ' '.join(argv))
  process = subprocess.Popen(argv)

  if process.wait() != 0:
    error_label = ColoredText('ERROR', logging.ERROR)
    raise RunOrDieError('\n'.join(['',
                                   '==========',
                                   ' %s: %s' % (error_label, ' '.join(argv)),
                                   '==========']))


def RemoveFile(file_name):
  """Removes the specified file."""
  if not (os.path.isfile(file_name) or os.path.islink(file_name)):
    return  # Do nothing if not exist.
  if IsWindows():
    # Read-only files cannot be deleted on Windows.
    os.chmod(file_name, 0o700)
  logging.debug('Removing file: %s', file_name)
  os.unlink(file_name)


def CopyFile(source, destination):
  """Copies a file to the destination. Remove an old version if needed."""
  if os.path.isfile(destination):  # Remove the old one if exists.
    RemoveFile(destination)
  dest_dirname = os.path.dirname(destination)
  if not os.path.isdir(dest_dirname):
    os.makedirs(dest_dirname)
  logging.info('Copying file to: %s', destination)
  shutil.copy(source, destination)


def RemoveDirectoryRecursively(directory):
  """Removes the specified directory recursively."""
  if os.path.isdir(directory):
    logging.debug('Removing directory: %s', directory)
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
  logging.critical('\n==========\n')
  logging.critical(error_message)
  logging.critical('\n==========\n')
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


# ANSI Color sequences
_TERMINAL_COLORS = {
    'CLEAR': '\x1b[0m',
    'BLACK': '\x1b[30m',
    'RED': '\x1b[31m',
    'GREEN': '\x1b[32m',
    'YELLOW': '\x1b[33m',
    'BLUE': '\x1b[34m',
    'MAGENTA': '\x1b[35m',
    'CYAN': '\x1b[36m',
    'WHITE': '\x1b[37m',
}

# Indicates the output stream supports colored text or not.
# It is disabled on windows because cmd.exe doesn't support ANSI color escape
# sequences.
# TODO(team): Considers to use ctypes.windll.kernel32.SetConsoleTextAttribute
#             on Windows. b/6260694
_COLORED_TEXT_SUPPORT = (
    not IsWindows() and sys.stdout.isatty() and sys.stderr.isatty())


def ColoredText(text, level):
  """Generates a colored text according to log level."""
  if not _COLORED_TEXT_SUPPORT:
    return text

  if level <= logging.INFO:
    color = 'GREEN'
  elif level <= logging.WARNING:
    color = 'YELLOW'
  else:
    color = 'RED'

  return _TERMINAL_COLORS[color] + text + _TERMINAL_COLORS['CLEAR']


class ColoredLoggingFilter(logging.Filter):
  """Supports colored text on logging library.

  Sample code.
  logging.getLogger().addFilter(util.ColoredLoggingFilter())
  """

  def filter(self, record):
    level_name = record.levelname
    level_no = record.levelno
    if level_name and level_no:
      record.levelname = ColoredText(level_name, level_no)

    return True


def WalkFileContainers(comma_separated_paths):
  """Walks like os.walk() accepting comma separated directory or zip file paths.

  Args:
    comma_separated_paths: e.g., "directory/path,zip/file/path.zip"
  Yields:
    See os.walk()
  """
  for path in comma_separated_paths.split(','):
    if os.path.isdir(path):
      for dirpath, dirnames, filenames in os.walk(path):
        yield dirpath, dirnames, filenames
    else:
      tempdir = tempfile.mkdtemp()
      with zipfile.ZipFile(path, 'r') as z:
        z.extractall(tempdir)
        for dirpath, dirnames, filenames in os.walk(tempdir):
          yield dirpath, dirnames, filenames

