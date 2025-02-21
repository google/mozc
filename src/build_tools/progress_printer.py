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

"""A console utility to show progress."""

import os
import sys
import time


class ProgressPrinter:
  """A utility to print progress message with carriage return and trancatoin."""

  def __enter__(self):
    if not sys.stdout.isatty():

      class NoOpImpl:
        """A no-op implementation in case stdout is not attached to concole."""

        def print_line(self, msg: str) -> None:
          """No-op implementation.

          Args:
            msg: Unused.
          """
          del msg  # Unused
          return

        def cleanup(self) -> None:
          pass

      self.impl = NoOpImpl()
      return self

    class Impl:
      """A real implementation in case stdout is attached to concole."""

      last_output_time_ns = time.time_ns()

      def print_line(self, msg: str) -> None:
        """Print the given message with carriage return and trancatoin.

        Args:
          msg: Message to be printed.
        """
        colmuns = os.get_terminal_size().columns
        now = time.time_ns()
        if (now - self.last_output_time_ns) < 25000000:
          return
        msg = msg + ' ' * max(colmuns - len(msg), 0)
        msg = msg[0:(colmuns)] + '\r'
        sys.stdout.write(msg)
        sys.stdout.flush()
        self.last_output_time_ns = now

      def cleanup(self) -> None:
        colmuns = os.get_terminal_size().columns
        sys.stdout.write(' ' * colmuns + '\r')
        sys.stdout.flush()

    self.impl = Impl()
    return self

  def print_line(self, msg: str) -> None:
    self.impl.print_line(msg)

  def __exit__(self, *exc):
    self.impl.cleanup()
