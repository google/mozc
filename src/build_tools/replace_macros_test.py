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

"""Tests for replace_macros.py."""

from absl.testing import absltest
from build_tools import replace_macros


class ReplaceMacrosTest(absltest.TestCase):

  def testParseVariableDefinitions(self):
    var_defs = (
        'enable_var11',
        'enable_var12=true',
        'enable_var13=false',
        'enable_var14=1',
        'enable_var15=0',
        'disable_var21',
        'disable_var22=true',
        'disable_var23=false',
        'disable_var24=1',
        'disable_var25=0',
        'squoted_var31=foo bar',
        'squoted_var32=\'foo\'s bar\'',
        'squoted_var33="foo\'s bar"',
        'squoted_var34=0',
        'squoted_var35=123',
        'dquoted_var41=foo bar',
        'dquoted_var42=\'foo\'s bar\'',
        'dquoted_var43="foo\'s bar"',
        'dquoted_var44=0',
        'dquoted_var45=123',
        'var51=0',
        'var52=123',
        'var53=3.14',
        'var54=foo bar',
        'var55=foo\'s bar',
        )
    expected_list = (
        ('enable_', 'var11', True),
        ('enable_', 'var12', True),
        ('enable_', 'var13', False),
        ('enable_', 'var14', True),
        ('enable_', 'var15', False),
        ('enable_', 'var21', False),
        ('enable_', 'var22', False),
        ('enable_', 'var23', True),
        ('enable_', 'var24', False),
        ('enable_', 'var25', True),
        ('squoted_', 'var31', '\'foo bar\''),
        ('squoted_', 'var32', '\'\\\'foo\\\'s bar\\\'\''),
        ('squoted_', 'var33', '\'"foo\\\'s bar"\''),
        ('squoted_', 'var34', '\'0\''),
        ('squoted_', 'var35', '\'123\''),
        ('dquoted_', 'var41', '"foo bar"'),
        ('dquoted_', 'var42', '"\'foo\'s bar\'"'),
        ('dquoted_', 'var43', '"\\"foo\'s bar\\""'),
        ('dquoted_', 'var44', '"0"'),
        ('dquoted_', 'var45', '"123"'),
        (None, 'var51', 0),
        (None, 'var52', 123),
        (None, 'var53', '3.14'),
        (None, 'var54', 'foo bar'),
        (None, 'var55', 'foo\'s bar'),
        )
    result_list = replace_macros.ParseVariableDefinitions(var_defs)
    self.assertEqual(len(expected_list), len(result_list))
    for expected, result in zip(expected_list, result_list):
      self.assertEqual(expected, result)

  def testTransformValuesToCStyle(self):
    variables = (
        (None, 'var1', True),
        (None, 'var2', False),
        (None, 'var3', 0),
        (None, 'var4', 123),
        (None, 'var5', 'hello, world'),
        )
    expected_list = (
        (None, 'var1', 'true'),
        (None, 'var2', 'false'),
        (None, 'var3', 0),
        (None, 'var4', 123),
        (None, 'var5', 'hello, world'),
        )
    result_list = replace_macros.TransformValuesToCStyle(variables)
    self.assertEqual(len(expected_list), len(result_list))
    for expected, result in zip(expected_list, result_list):
      self.assertEqual(expected, result)

  def testReplaceVariables(self):
    variables = (
        (None, 'var1', True),
        (None, 'var2', False),
        (None, 'var3', 0),
        (None, 'var4', 123),
        (None, 'var5', 'hello, world'),
        (None, 'var6', '"hello, world"'),
        )
    text = '\n'.join((
        '@var1@ @VAR1@',
        '@VAR2@@Var2@',
        '@VAR3@VAR3@',
        '@VAR4@@VAR4@',
        'VAR5@VAR5@@VAR5',
        '@VAR6@',
        ))
    expected = '\n'.join((
        '@var1@ True',
        'False@Var2@',
        '0VAR3@',
        '123123',
        'VAR5hello, world@VAR5',
        '"hello, world"',
        ))
    result = replace_macros.ReplaceVariables(text, variables)
    self.assertEqual(expected, result)


if __name__ == '__main__':
  absltest.main()
