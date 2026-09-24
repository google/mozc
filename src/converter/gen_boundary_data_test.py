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

import struct

from absl.testing import absltest

from converter import gen_boundary_data


class GenBoundaryDataTest(absltest.TestCase):

  def test_pattern_to_regexp(self) -> None:
    # Test cases for PatternToRegexp
    self.assertEqual(
        gen_boundary_data.PatternToRegexp('名詞,接尾'), '^名詞,接尾(?:,|$)'
    )
    self.assertEqual(
        gen_boundary_data.PatternToRegexp('名詞,形容動詞語幹,'),
        '^名詞,形容動詞語幹,',
    )
    self.assertEqual(
        gen_boundary_data.PatternToRegexp('連体詞,*,*,*,*,*,*'),
        '^連体詞,[^,]+,[^,]+,[^,]+,[^,]+,[^,]+,[^,]+(?:,|$)',
    )
    self.assertEqual(
        gen_boundary_data.PatternToRegexp('助詞,(格助詞|連体化)'),
        '^助詞,(格助詞|連体化)(?:,|$)',
    )

  def test_get_cost(self) -> None:
    # Setup patterns
    prefix_patterns = [
        (
            gen_boundary_data.re.compile(
                gen_boundary_data.PatternToRegexp('名詞,接尾')
            ),
            100,
        ),
        (
            gen_boundary_data.re.compile(
                gen_boundary_data.PatternToRegexp('名詞,形容動詞語幹,')
            ),
            300,
        ),
    ]

    # Test exact matches
    self.assertEqual(
        gen_boundary_data.GetCost(prefix_patterns, '名詞,接尾,*,*,*'), 100
    )
    self.assertEqual(
        gen_boundary_data.GetCost(prefix_patterns, '名詞,形容動詞語幹,*,*,*'),
        300,
    )

    # Test false positive prevention (these should NOT match)
    self.assertEqual(
        gen_boundary_data.GetCost(prefix_patterns, '名詞,接尾可能,*,*,*'), 0
    )
    self.assertEqual(
        gen_boundary_data.GetCost(
            prefix_patterns, '名詞,形容動詞語幹特殊,*,*,*'
        ),
        0,
    )

  def test_gen_boundary_data(self) -> None:
    mock_boundary_def = [
        'PREFIX\t名詞,接尾\t100\n',
        'SUFFIX\t名詞,形容動詞語幹,\t300\n',
    ]
    mock_id_def = [
        '0\t名詞,接尾,*,*,*\n',
        '1\t名詞,接尾可能,*,*,*\n',
        '2\t名詞,形容動詞語幹,*,*,*\n',
        '3\t名詞,形容動詞語幹特殊,*,*,*\n',
    ]
    mock_special_pos = [
        'SPECIAL_POS_1\n',
    ]

    actual_data = gen_boundary_data.GenerateBoundaryData(
        mock_boundary_def, mock_id_def, mock_special_pos
    )

    # Verify output binary
    # Expected:
    # ID 0 (名詞,接尾,*,*,*): prefix=100, suffix=0
    # ID 1 (名詞,接尾可能,*,*,*): prefix=0, suffix=0  (Not matched due to fix)
    # ID 2 (名詞,形容動詞語幹,*,*,*): prefix=0, suffix=300
    # ID 3 (名詞,形容動詞語幹特殊,*,*,*): prefix=0, suffix=0 (Not matched due to fix)
    # Special POS 1: prefix=0, suffix=0
    expected_data = struct.pack(
        '<HHHHHHHHHH',
        100,
        0,  # ID 0
        0,
        0,  # ID 1
        0,
        300,  # ID 2
        0,
        0,  # ID 3
        0,
        0,  # Special POS 1
    )

    self.assertEqual(actual_data, expected_data)


if __name__ == '__main__':
  absltest.main()
