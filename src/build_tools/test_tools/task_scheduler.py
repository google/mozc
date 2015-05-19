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

"""Provides parallelization tasks.

Usage:

The simplest usage example is as follows. The command should be represented as
array.

 Sample Code:
  command = ['ls', '-al']      # command line options should be array
  scheduler = TaskScheduler()
  scheduler.AddTask(command)
  scheduler.Execute(8)          # Execute with specified number of threads. This
                                  function blocks main thread until all task is
                                  compoeted.

If you want to do something after task is executed, set call-back functions as
follows. In this case, you can provide some information via user_data.

 Sample Code:
  def OnSuccess(cmd, output, user_data):
    print output

  def OnFail(cmd, output, return_code, user_data):
    print user_data['error_msg']
    print output
    sys.exit(1)

  command = ['ls', '-al']
  scheduler = TaskScheduler()
  scheduler.AddTask(command, on_success=OnSuccess, on_fail=OnFail,
                    user_data={'error_msg': 'Error Message')
  scheduler.Execute(8)

TODO(nona): replace multiprocessing instead of threading
"""

__author__ = "nona"

import logging
import Queue
import subprocess
import sys
import threading


class Task(object):
  """Task class provides a single command line operation."""

  def __init__(self, command, on_success=None, on_fail=None, on_finish=None,
               user_data=None):
    self._command = command
    self._output = ''
    self._on_success = on_success
    self._on_fail = on_fail
    self._on_finish = on_finish
    self._user_data = user_data

  def Execute(self):
    """Performs command and saves its output string to buffer.

    This method is called by threading module in other thread. This method
    executes given command and save it's stdout and stderr to same buffer. After
    it is executed, if executed command's return code is 0, call on_success
    function specified in constructor, otherwise, call on_fail function.
    """

    try:
      self._process = subprocess.Popen(self._command, stdout=subprocess.PIPE,
                                      stderr=subprocess.STDOUT)
      (self._output, _) = self._process.communicate()
    except OSError:
      logging.fatal('Fail to execute %s', ' '.join(self._command))
      sys.exit(1)

    if self._process.poll() == 0:
      if self._on_success:
        self._on_success(self._command, self._output, self._user_data)
    else:
      if self._on_fail:
        self._on_fail(self._command, self._output, self._process.returncode,
                      self._user_data)
    if self._on_finish:
      self._on_finish(self._command, self._output, self._user_data)


class TaskScheduler(object):
  """This class provieds task parallelization.

  This class accepts command-line strings and executes them with parallel.
  """

  class _WorkerThread(threading.Thread):
    def __init__(self, task_queue):
      threading.Thread.__init__(self)
      self.task_queue = task_queue

    def run(self):
      while not self.task_queue.empty():
        task = self.task_queue.get()
        task.Execute()
        self.task_queue.task_done()

  def __init__(self):
    self._task = []

  def Execute(self, num_parallel):
    """This method watches all task objects.

    This method watches all task objects and wait until all of them are
    finished.
    Args:
      num_parallel: Allows "num_parallel" tasks at once.
    """
    task_queue = Queue.Queue()
    for task in self._task:
      task_queue.put(task)

    thread_list = [self._WorkerThread(task_queue) for _ in xrange(num_parallel)]
    for thread in thread_list:
      thread.start()

    task_queue.join()

  def AddTask(self, command, on_success=None, on_fail=None, on_finish=None,
              user_data=None):
    self._task.append(Task(command, on_success, on_fail, on_finish, user_data))
