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

"""Generate a binary image of SerializedStringArray."""
import io
import struct


def SerializeToFile(strings, filename):
  """Builds a binary image of strings.

  For file format, see base/serialized_string_array.h.

  Args:
    strings: A list of strings to be serialized.
    filename: Output binary file.
  """
  array_size = len(strings)
  str_data = io.BytesIO()

  # Precompute offsets and lengths.
  offsets = []
  lengths = []
  offset = 4 + 8 * array_size  # The start offset of strings chunk
  for data in strings:
    if isinstance(data, str):
      data = data.encode('utf-8')
    offsets.append(offset)
    lengths.append(len(data))
    offset += len(data) + 1  # Include one byte for the trailing '\0'
    str_data.write(data + b'\0')

  with open(filename, 'wb') as f:
    # 4-byte array_size.
    f.write(struct.pack('<I', array_size))

    # Offset and length array of (4 + 4) * array_size bytes.
    for i in range(array_size):
      f.write(struct.pack('<I', offsets[i]))
      f.write(struct.pack('<I', lengths[i]))

    # Strings chunk.
    f.write(str_data.getvalue())
