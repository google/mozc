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

"""Generate binary data for zero query suggestion.

For output format, see zero_query_dict.h.
"""

from __future__ import absolute_import
from __future__ import print_function

import os
import struct

import six

from build_tools import code_generator_util as cgu
from build_tools import serialized_string_array_builder


ZERO_QUERY_TYPE_NONE = 0
ZERO_QUERY_TYPE_NUMBER_SUFFIX = 1
ZERO_QUERY_TYPE_EMOTICON = 2
ZERO_QUERY_TYPE_EMOJI = 3

# bit fields
# These are standing for command::Request::EmojiCarrierType
EMOJI_TYPE_NONE = 0
EMOJI_TYPE_UNICODE = 1
EMOJI_TYPE_DOCOMO = 2
EMOJI_TYPE_SOFTBANK = 4
EMOJI_TYPE_KDDI = 8


class ZeroQueryEntry(object):

  def __init__(self, entry_type, value, emoji_type, emoji_android_pua):
    self.entry_type = entry_type
    self.value = value
    self.emoji_type = emoji_type
    self.emoji_android_pua = emoji_android_pua


def WriteZeroQueryData(zero_query_dict, output_token_array,
                       output_string_array):
  # Collect all the strings and assign index in ascending order
  string_index = {}
  for key, entry_list in six.iteritems(zero_query_dict):
    string_index[key] = 0
    for entry in entry_list:
      string_index[entry.value] = 0
  sorted_strings = sorted(string_index)
  for i, s in enumerate(sorted_strings):
    string_index[s] = i

  with open(output_token_array, 'wb') as f:
    for key in sorted(zero_query_dict):
      for entry in zero_query_dict[key]:
        f.write(struct.pack('<I', string_index[key]))
        f.write(struct.pack('<I', string_index[entry.value]))
        f.write(struct.pack('<H', entry.entry_type))
        f.write(struct.pack('<H', entry.emoji_type))
        f.write(struct.pack('<I', entry.emoji_android_pua))

  serialized_string_array_builder.SerializeToFile(sorted_strings,
                                                  output_string_array)
