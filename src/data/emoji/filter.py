# -*- coding: utf-8 -*-
# Copyright 2010-2020, Google Inc.
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

"""Filter emoji data file by specified conditions and print specified fields.

Usage:
$ python filter.py [conditions...] --print FIELD < emoji_data.tsv > output.tsv

Conditions are a combination of:
* --has FIELD: requies the emoji to have FIELD.
* --not_have FIELD: requires the emoji not to have FIELD.
* --category CATEGORY: requies the emoji to be in CATEGORY.
"""

__author__ = "kyatoh"

import optparse
import sys

from build_tools import code_generator_util


# The order corresponds to the scheme of emoji_data.tsv.
_FIELDS = [
    'unicode_code_point',
    'actual_data',
    'android_code_point',
    'docomo_code_point',
    'softbank_code_point',
    'kddi_code_point',
    'yomi',
    'unicode_name',
    'japanese_name',
    'docomo_name',
    'softbank_name',
    'kddi_name',
    'category_name_index',
]
_FIELD_TO_INDEX = dict((field, index) for index, field in enumerate(_FIELDS))
_CATEGORY_FIELD_INDEX = len(_FIELDS) - 1


def _CreateOptionParser():
  """Create a OptionParser to parse options for this script."""

  description = ('Filters emoji data file by specified conditions and '
                 'print specified fields. '
                 'FIELD options can be specified either by a field name in '
                 'emoji_data.tsv (e.g. unicode_code_point) or a field index '
                 '(0-origin).')
  parser = optparse.OptionParser(description=description)
  parser.add_option(
      '--has', action='append', dest='required_fields', default=[],
      help='specify field that must exist. '
      'Repeat this option to set multiple constraints.',
      metavar='FIELD')
  parser.add_option(
      '--not_have', action='append', dest='forbidden_fields', default=[],
      help='specify field that must NOT exist. '
      'Repeat this option to set multiple constraints.',
      metavar='FIELD')
  parser.add_option('--category', dest='category', default=[],
                    help='specify category that emojis must have',
                    metavar='CATEGORY')
  parser.add_option(
      '--print', action='append', dest='out_fields', default=[],
      help='specify field that will be printed. '
      'Repeat this option to print multiple fields. '
      'Fields will be separated by a tab.',
      metavar='FIELD')
  return parser


def _ToIndex(field):
  assert field in _FIELD_TO_INDEX or field.isdigit(), (
      'FIELD must be a field name or a field index.')
  if field in _FIELD_TO_INDEX:
    return _FIELD_TO_INDEX[field]
  else:
    assert int(field) < len(_FIELDS), 'FIELD index is too large.'
    return int(field)


def _ShouldPrint(entry, options):
  for field in options.required_fields:
    if not entry[_ToIndex(field)]:
      return False
  for field in options.forbidden_fields:
    if entry[_ToIndex(field)]:
      return False
  if options.category:
    if entry[_CATEGORY_FIELD_INDEX].split('-')[0] != options.category:
      return False
  return True


def _Print(entry, options, output_stream):
  indices = [_ToIndex(field) for field in options.out_fields]
  output_stream.write('\t'.join([entry[i] for i in indices]) + '\n')


def main():
  options = _CreateOptionParser().parse_args()[0]
  stream = code_generator_util.SkipLineComment(sys.stdin)
  stream = code_generator_util.ParseColumnStream(stream, delimiter='\t')
  for entry in stream:
    assert len(entry) == len(_FIELDS), 'Invalid TSV row.'
    if _ShouldPrint(entry, options):
      _Print(entry, options, sys.stdout)


if __name__ == '__main__':
  main()
