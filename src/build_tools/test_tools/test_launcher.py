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

"""Provides parallelization tests.

Usage:

The usage example is as follows.

 Sample Code:
  launcher = TestLauncher(gtest_report_dir='/tmp/my_gtest_report_dir')
  launcher.AddTest(['/path/to/binary/some_test'])
  launcher.AddTest(['/path/to/binary/another_test'])
  ...
  launcher.Execute(8)          # Execute with specified number of processes.
                                 This function blocks main thread until all
                                 task is completed.
TODO(nona): Adds unit test(hard to inject mock code due to multiprocessing)
"""


import errno
import logging
import multiprocessing
import os
import shutil
import stat
import subprocess
import sys
import tempfile
import time

from build_tools import util


# TODO(team): Move this to build_tools.util
def _RmTreeOnError(function, path, info):
  """Callback for handling shutil.rmtree errors."""
  # On Windows, file operation against a newly created or updated file
  # sometimes conflicts with that by security software and/or file indexer.
  time.sleep(0.5)
  if function == os.remove:
    os.chmod(path, stat.S_IREAD | stat.S_IWRITE)
    os.remove(path)
  elif function == os.listdir:
    os.chmod(path, stat.S_IREAD | stat.S_IWRITE | stat.S_IEXEC)
    shutil.rmtree(path, ignore_errors=True)
  elif function == os.rmdir and info[1].errno == errno.ENOTEMPTY:
    # Another race condition? Retry.
    shutil.rmtree(path, ignore_errors=True)


# TODO(team): Move this to build_tools.util
class PathDeleter(object):
  """A deleter to ensure that the given path object is certainly removed."""

  def __init__(self, path):
    """Stores the target path."""
    self._path = path

  def __enter__(self):
    """Does nothing."""
    pass

  def __exit__(self, *unused_exc_info):
    """Recursively removes the files and directories under given path."""
    if not os.path.exists(self._path):
      return
    shutil.rmtree(self._path, onerror=_RmTreeOnError)
    if os.path.exists(self._path):
      # Try again without error handler after 1 sec sleep.
      time.sleep(1)
      try:
        shutil.rmtree(self._path)
      except OSError as e:
        logging.error('Failed to remove %s. error: %s', self._path, e)


def _ExecuteTest(command_and_gtest_report_dir):
  """Executes tests with specified Test command.

  Args:
    command_and_gtest_report_dir: tuple of (command, gtest_report_dir) where
        command is a list of string to be executed.  gtest_report_dir is the
        directory path where gtest XML reports will be stored.

  Returns:
    A dictionary:
      command: An performed command-line string list.
      result: An boolean which represents test result.

  This should be a top-level function due to the restriction of the pickle
  module, which is used in multiprocessing module.
  (http://docs.python.org/library/pickle.html)
  """
  command, gtest_report_dir = command_and_gtest_report_dir
  binary = command[0]
  binary_filename = os.path.basename(binary)
  tmp_dir = tempfile.mkdtemp()
  with PathDeleter(tmp_dir):
    env = os.environ.copy()
    env['TEST_TMPDIR'] = tmp_dir
    tmp_xml_path = os.path.join(tmp_dir, '%s.xml' % binary_filename)
    # Due to incompatibility between the prefixes of internal and external,
    # testing libraries.
    test_command = command + [
        '--gunit_output=xml:%s' % tmp_xml_path,
        '--gtest_output=xml:%s' % tmp_xml_path,
    ]
    try:
      proc = subprocess.Popen(
          test_command,
          env=env,
          stdout=subprocess.PIPE,
          stderr=subprocess.STDOUT,
      )
      (output, _) = proc.communicate()
      result = proc.poll() == 0 and os.path.isfile(tmp_xml_path)
      if os.path.isfile(tmp_xml_path) and gtest_report_dir:
        shutil.copy(tmp_xml_path, gtest_report_dir)
    except OSError:
      logging.fatal('Fail to execute %s', ' '.join(command))
      sys.exit(1)

  if result:
    label = util.ColoredText('[ PASSED ]', logging.INFO)
    logging.info('%s %s', label, binary)
  else:
    label = util.ColoredText('[ FAILED ]', logging.ERROR)
    logging.error('Failed. Detail output:\n%s', output.decode('utf-8'))
    logging.info('%s %s', label, binary)

  return {'command': command, 'result': result}


class TestLauncher(object):
  """This class provieds task parallelization.

  This class accepts command-line strings and executes them in parallel.
  """

  def __init__(self, gtest_report_dir=''):
    """Initializes the object.

    Args:
      gtest_report_dir: Directory path where gtest XML reports will be stored.
    """
    self._test_commands = []
    self._gtest_report_dir = gtest_report_dir

  def Execute(self, num_parallel):
    """Starts to run tests.

    This method watches all test command until all of them are finished.

    Args:
      num_parallel: Allows "num_parallel" tasks at once.
    Returns:
      An failed command list. Each command represents as one-line
      command-line string. If this list is empty, all tests are passed.
    """
    # TODO(nona): Show progress report for debugging.
    try:
      pool = multiprocessing.Pool(processes=num_parallel)
      params = [(command, self._gtest_report_dir)
                for command in self._test_commands]
      # Workaround against http://bugs.python.org/issue8296
      # See also
      # http://stackoverflow.com/questions/1408356/keyboard-interrupts-with-pythons-multiprocessing-pool
      async_results = pool.map_async(_ExecuteTest, params)
      while True:
        try:
          results = async_results.get(1000000)
          break
        except multiprocessing.TimeoutError:
          pass
      pool.close()
      return [' '.join(result['command'])
              for result in results if not result['result']]
    except:
      pool.terminate()
      logging.fatal('Exception occurred.')
      raise

  def AddTestCommand(self, command):
    self._test_commands.append(command)
