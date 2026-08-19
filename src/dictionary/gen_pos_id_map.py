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

"""A tool to generate POS ID map binary data from id.def."""

import struct

from absl import app
from absl import flags

_ID_FILE = flags.DEFINE_string('id_file', None, 'Path to id.def', required=True)
_OUTPUT = flags.DEFINE_string(
    'output', None, 'Path to output binary', required=True
)


def ParseIdDef(id_def_path: str) -> dict[int, str]:
  """Parses id.def and returns a mapping from POS ID to POS string."""
  id_map: dict[int, str] = {}
  with open(id_def_path, 'r', encoding='utf-8') as stream:
    for line in stream:
      line = line.strip()
      if not line or line.startswith('#'):
        continue
      parts = line.split(maxsplit=1)
      if len(parts) < 2:
        continue
      pos_id = int(parts[0])
      id_map[pos_id] = parts[1]
  return id_map


def GeneratePosIdMapBinary(id_map: dict[int, str], output_path: str) -> None:
  """Generates pos_id_map binary data."""
  pos_ids = sorted(id_map.keys())
  if not pos_ids:
    raise ValueError('id.def must contain at least one valid POS ID.')

  max_pos_id = pos_ids[-1]
  # Provide +1 to accommodate pos_ids from 0 to max_pos_id
  pos_id_count = max_pos_id + 1
  offsets = [0] * pos_id_count
  string_data: list[bytes] = []

  # The offset is the absolute byte offset from the beginning of this
  # pos_id_map.data file (which becomes a section in mozc.data), NOT from
  # the beginning of the string buffer.
  current_offset = 4 + 4 * pos_id_count
  for pos_id in range(pos_id_count):
    offsets[pos_id] = current_offset
    utf8_str = id_map[pos_id].encode('utf-8') + b'\0'
    string_data.append(utf8_str)
    current_offset += len(utf8_str)

  string_buffer = b''.join(string_data)

  with open(output_path, 'wb') as output:
    output.write(struct.pack('<I', pos_id_count))
    for pos_id in range(pos_id_count):
      output.write(struct.pack('<I', offsets[pos_id]))
    output.write(string_buffer)


def main(argv: list[str]) -> None:
  if len(argv) > 1:
    raise app.UsageError('Too many command-line arguments.')

  id_map = ParseIdDef(_ID_FILE.value)
  GeneratePosIdMapBinary(id_map, _OUTPUT.value)


if __name__ == '__main__':
  app.run(main)
