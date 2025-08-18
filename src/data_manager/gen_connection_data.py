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

"""Generator script for connection data."""

import codecs
import io
import logging
import optparse
import os
import struct
import sys

from build_tools import code_generator_util

INVALID_COST = 30000
INVALID_1BYTE_COST = 255
RESOLUTION_FOR_1BYTE = 64
FILE_MAGIC = b'\xAB\xCD'

FALSE_VALUES = ['f', 'false', '0']
TRUE_VALUES = ['t', 'true', '1']


def ParseBoolFlag(value):
  if value is None:
    return False

  value = value.lower()
  if value in TRUE_VALUES:
    return True
  if value in FALSE_VALUES:
    return False

  # Unknown value.
  logging.critical('Unknown boolean flag: %s', value)
  sys.exit(1)


def GetPosSize(filepath):
  # The pos-size should be equal to the number of lines.
  # TODO(hidehiko): Merge this method with pos_util in dictionary.
  with codecs.open(filepath, 'r', encoding='utf-8') as stream:
    stream = code_generator_util.SkipLineComment(stream)
    # Count the number of lines.
    return sum(1 for _ in stream)


def ParseConnectionFile(text_connection_file, pos_size, special_pos_size):
  # The result is a square matrix.
  mat_size = pos_size + special_pos_size

  matrix = [[0] * mat_size for _ in range(mat_size)]
  with codecs.open(text_connection_file, encoding='utf-8') as stream:
    stream = code_generator_util.SkipLineComment(stream)
    # The first line contains the matrix column/row size.
    size = next(stream).rstrip()
    assert int(size) == pos_size, '%s != %d' % (size, pos_size)

    for array_index, cost in enumerate(stream):
      cost = int(cost.rstrip())
      rid = array_index // pos_size
      lid = array_index % pos_size
      if rid == 0 and lid == 0:
        cost = 0
      matrix[rid][lid] = cost

  # Fill INVALID_COST in matrix elements for special POS.
  for rid in range(pos_size, mat_size):
    for lid in range(1, mat_size):  # Skip EOS
      matrix[rid][lid] = INVALID_COST

  for lid in range(pos_size, mat_size):
    for rid in range(1, mat_size):  # Skip BOS
      matrix[rid][lid] = INVALID_COST

  return matrix


def CreateModeValueList(matrix):
  """Create a list of modes of each row."""
  result = []
  for row in matrix:
    m = {}
    for cost in row:
      if cost == INVALID_COST:
        # Heuristically, we do not compress INVALID_COST.
        continue
      m[cost] = m.get(cost, 0) + 1
    mode_value = max(m.items(), key=lambda __count: __count[1])[0]
    result.append(mode_value)
  return result


def CompressMatrixByModeValue(matrix, mode_value_list):
  # To compress the data size, we hold mode values for each row in a separate
  # list, and fill None into the matrix if it equals to the corresponding
  # mode value.
  assert len(matrix) == len(mode_value_list)
  for row, mode_value in zip(matrix, mode_value_list):
    for index in range(len(row)):
      if row[index] == mode_value:
        row[index] = None


def OutputBitList(bit_list, stream):
  # Make sure that the bit list is aligned to the byte boundary.
  assert len(bit_list) % 8 == 0
  for bits in code_generator_util.SplitChunk(bit_list, 8):
    byte = 0
    for bit_index, bit in enumerate(bits):
      if bit:
        # Fill in LSB to MSB order.
        byte |= 1 << bit_index
    stream.write(struct.pack('B', byte))


def BuildBinaryData(matrix, mode_value_list, use_1byte_cost):
  # To compress the connection data, we use two-level succinct bit vector.
  #
  # The basic idea to compress the rid-lid matrix is compressing each row as
  # follows:
  # find the mode value of the row, and set the cells containins the value
  # empty, thus we get a sparse array.
  # We can compress sparse array by using succinct bit vector.
  # (Please see also storage/louds/simple_succinct_bit_vector_index and
  # storage/louds/bit_vector_based_array.)
  # In addition, we compress the bit vector, too. Fortunately the distribution
  # of bits is biased, so we group consecutive 8-bits and create another
  # bit vector, named chunk-bits;
  # - if no bits are 1, the corresponding bit is 0, otherwise 1.
  # By using the bit vector, we can compact the original bit vector by skipping
  # consecutive eight 0-bits. We can calculate the actual bit position in
  # the compact bit vector by using Rank1 operation on chunk-bits.
  #
  # The file format is as follows:
  # FILE_MAGIC (\xAB\xCD): 2bytes
  # Resolution: 2bytes
  # Num rids: 2bytes
  # Num lids: 2bytes
  # A list of mode values: 2bytes * rids (aligned to 32bits)
  # A list of row data.
  #
  # The row data format is as follows:
  # The size of compact bits in bytes: 2bytes
  # The size of values in bytes: 2bytes
  # chunk_bits, compact_bits, followed by values.

  if use_1byte_cost:
    resolution = RESOLUTION_FOR_1BYTE
  else:
    resolution = 1
  stream = io.BytesIO()

  # Output header.
  stream.write(FILE_MAGIC)
  matrix_size = len(matrix)
  assert 0 <= matrix_size <= 65535
  stream.write(struct.pack('<HHH', resolution, matrix_size, matrix_size))

  # Output mode value list.
  for value in mode_value_list:
    assert 0 <= value <= 65536
    stream.write(struct.pack('<H', value))

  # 4 bytes alignment.
  if len(mode_value_list) % 2:
    stream.write(b'\x00\x00')

  # Process each row:
  for row in matrix:
    chunk_bits = []
    compact_bits = []
    values = []

    for chunk in code_generator_util.SplitChunk(row, 8):
      if all(cost is None for cost in chunk):
        # All bits are 0, so output 0-chunk bit.
        chunk_bits.append(False)
        continue

      chunk_bits.append(True)
      for cost in chunk:
        if cost is None:
          compact_bits.append(False)
        else:
          compact_bits.append(True)
          if use_1byte_cost:
            if cost == INVALID_COST:
              cost = INVALID_1BYTE_COST
            else:
              cost //= resolution
              assert cost != INVALID_1BYTE_COST
          values.append(cost)

    # 4 bytes alignment.
    while len(chunk_bits) % 32:
      chunk_bits.append(False)
    while len(compact_bits) % 32:
      compact_bits.append(False)
    if use_1byte_cost:
      while len(values) % 4:
        values.append(0)
      values_size = len(values)
    else:
      while len(values) % 2:
        values.append(0)
      values_size = len(values) * 2

    # Output the bits for a row.
    stream.write(struct.pack('<HH', len(compact_bits) // 8, values_size))
    OutputBitList(chunk_bits, stream)
    OutputBitList(compact_bits, stream)
    if use_1byte_cost:
      for value in values:
        assert 0 <= value <= 255
        stream.write(struct.pack('<B', value))
    else:
      for value in values:
        assert 0 <= value <= 65535
        stream.write(struct.pack('<H', value))

  return stream.getvalue()


def ParseOptions():
  parser = optparse.OptionParser()
  parser.add_option('--text_connection_file', dest='text_connection_file')
  parser.add_option('--id_file', dest='id_file')
  parser.add_option('--special_pos_file', dest='special_pos_file')
  parser.add_option('--use_1byte_cost', dest='use_1byte_cost')
  parser.add_option('--binary_output_file', dest='binary_output_file')
  parser.add_option('--header_output_file', dest='header_output_file')
  return parser.parse_args()[0]


def main():
  options = ParseOptions()

  pos_size = GetPosSize(options.id_file)
  special_pos_size = GetPosSize(options.special_pos_file)
  matrix = ParseConnectionFile(
      options.text_connection_file, pos_size, special_pos_size
  )
  mode_value_list = CreateModeValueList(matrix)
  CompressMatrixByModeValue(matrix, mode_value_list)
  binary = BuildBinaryData(
      matrix, mode_value_list, ParseBoolFlag(options.use_1byte_cost)
  )

  if options.binary_output_file:
    dirpath = os.path.dirname(options.binary_output_file)
    if not os.path.exists(dirpath):
      os.makedirs(dirpath)
    with open(options.binary_output_file, 'wb') as stream:
      stream.write(binary)

  if options.header_output_file:
    dirpath = os.path.dirname(options.header_output_file)
    if not os.path.exists(dirpath):
      os.makedirs(dirpath)
    with open(options.header_output_file, 'wb') as stream:
      code_generator_util.WriteCppDataArray(binary, 'ConnectionData', stream)


if __name__ == '__main__':
  main()
