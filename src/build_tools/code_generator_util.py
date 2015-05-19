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

"""Utilities to generate source codes."""

__author__ = "hidehiko"

CPP_STRING_LITERAL_EXCEPTION = ['\\', '"']


def _CppCharEscape(c):
  code = ord(c)
  if 0x20 <= code <= 0x7E and c not in CPP_STRING_LITERAL_EXCEPTION:
    return c
  return r'\x%02X' % code


def ToCppStringLiteral(s):
  """Returns C-style string literal, or NULL if given s is None."""
  if s is None:
    return 'NULL'
  return '"%s"' % ''.join(_CppCharEscape(c) for c in s)


def SkipLineComment(stream, comment_prefix='#'):
  """Skips line comments from stream."""
  for line in stream:
    line = line.strip()
    if line and not line.startswith(comment_prefix):
      yield line


def ParseColumnStream(stream, num_column=None):
  """Returns parsed columns read from stream."""
  if num_column is None:
    for line in stream:
      yield line.split()
  else:
    for line in stream:
      yield line.split()[:num_column]
