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

"""Tests for build_tools.code_generator_util."""

import io

from absl.testing import absltest

from build_tools import code_generator_util


class CodeGeneratorUtilTest(absltest.TestCase):

  def testToCppStringLiteral(self):
    self.assertEqual(code_generator_util.ToCppStringLiteral(None), 'nullptr')
    self.assertEqual(code_generator_util.ToCppStringLiteral('abcde'), '"abcde"')
    self.assertEqual(
        code_generator_util.ToCppStringLiteral('あいうえお'), '"あいうえお"'
    )
    self.assertEqual(
        code_generator_util.ToCppStringLiteral('あいうえお'), '"あいうえお"'
    )
    self.assertEqual(
        code_generator_util.ToCppStringLiteral(r'abcde"\fghij'),
        r'"abcde\"\\fghij"',
    )
    self.assertEqual(
        code_generator_util.ToCppStringLiteral('あegEF'), '"あegEF"'
    )

  def testFormatWithCppEscape(self):
    self.assertEqual(
        code_generator_util.FormatWithCppEscape('%s, %s, %s', 'a', None, 'c'),
        '"a", nullptr, "c"',
    )
    self.assertEqual(
        code_generator_util.FormatWithCppEscape(
            '%s, %d, %1.2f', 'あ', 1.5, 1.5
        ),
        '"あ", 1, 1.50',
    )

  def testWriteCppDataArray(self):
    data = ('\x01\x23\x45\x67\x89\xAB\xCD\xEF' * 8 + '\x01\x23\x45\x67')

    stream = io.StringIO()
    code_generator_util.WriteCppDataArray(data, 'VariableName', stream)
    self.assertEqual(
        stream.getvalue(),
        r"""const char kVariableName_data[] =
"\x01\x23\x45\x67\x89\xAB\xCD\xEF\x01\x23\x45\x67\x89\xAB\xCD\xEF"
"\x01\x23\x45\x67\x89\xAB\xCD\xEF\x01\x23\x45\x67\x89\xAB\xCD\xEF"
"\x01\x23\x45\x67\x89\xAB\xCD\xEF\x01\x23\x45\x67\x89\xAB\xCD\xEF"
"\x01\x23\x45\x67\x89\xAB\xCD\xEF\x01\x23\x45\x67\x89\xAB\xCD\xEF"
"\x01\x23\x45\x67"
;
const size_t kVariableName_size = 68;
""",
    )

  def testToJavaStringLiteral(self):
    # None
    self.assertEqual(code_generator_util.ToJavaStringLiteral(None), 'null')
    # Empty list
    self.assertEqual(code_generator_util.ToJavaStringLiteral([]), 'null')
    # SNOWMAN WITHOUT SNOW emoji (NOT a surrogate pair emoji)
    self.assertEqual(
        code_generator_util.ToJavaStringLiteral(0x26C4),
        r'"\u26C4"')
    # CRECENT MOON emoji (A surrogate pair emoji)
    self.assertEqual(
        code_generator_util.ToJavaStringLiteral(0x1F319),
        r'"\uD83C\uDF19"')
    # SNOWMAN WITHOUT SNOW emoji (NOT a surrogate pair emoji, tuple)
    self.assertEqual(
        code_generator_util.ToJavaStringLiteral((0x26C4,)),
        r'"\u26C4"')
    # '0' with COMBINING ENCLOSING KEYCAP (Combination emoji, tuple)
    self.assertEqual(
        code_generator_util.ToJavaStringLiteral((0x30, 0xFE0F, 0x20E3)),
        r'"\u0030\uFE0F\u20E3"')

  def testSkipLineComment(self):
    stream = io.StringIO('abcdefg\n'
                         '# this comment should be removed.\n'
                         'this line should be kept.\n'
                         '  # this comment should be removed, too.\n'
                         '\n'  # An empty line also should be skipped.
                         'this line should be kept, too.\n')
    result = [line for line in code_generator_util.SkipLineComment(stream)]
    self.assertEqual(
        result,
        ['abcdefg',
         'this line should be kept.',
         'this line should be kept, too.'])

  def testParseColumnStream(self):
    stream = io.StringIO('field1-1\tfield1-2\tfield1-3\n'
                         'field2-1\n'
                         'field3-1\tfield3-2\n')
    result = [field_list
              for field_list in code_generator_util.ParseColumnStream(stream)]
    self.assertEqual(
        result,
        [['field1-1', 'field1-2', 'field1-3'],
         ['field2-1',],
         ['field3-1', 'field3-2']])

    stream = io.StringIO('field1-1\tfield1-2\tfield1-3\n'
                         'field2-1\tfield2-2\tfield2-3\tfield2-4\n'
                         'field3-1\tfield3-2\tfield3-3\tfield3-4\tfield3-5\n'
                         'field4-1\tfield4-2\n')
    stream = code_generator_util.ParseColumnStream(stream, num_column=2)
    result = [field_list for field_list in stream]
    self.assertEqual(
        result,
        [['field1-1', 'field1-2'],
         ['field2-1', 'field2-2'],
         ['field3-1', 'field3-2'],
         ['field4-1', 'field4-2']])

  def testSelectColumn(self):
    stream = [('abc1', 'def1', 'ghi1'),
              ('abc2', 'def2', 'ghi2')]
    result = [columns
              for columns in code_generator_util.SelectColumn(stream, [1, 2])]
    self.assertEqual(result, [('def1', 'ghi1'), ('def2', 'ghi2')])


if __name__ == '__main__':
  absltest.main()
