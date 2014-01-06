# -*- coding: utf-8 -*-
# Copyright 2010-2014, Google Inc.
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

"""Splits the specified apk file into each ABIs.

"Fat APK" contains multipul shared objects in order to run on all the ABIs.
But this means such APK is larger than "Thin" APK.
This script creates Thin APKs from Fat APK.

Note:
The format of Android Binary XML is not documented.
This script is based on below source code.
${ANDROID_SOURCE_TREE}/frameworks/base/include/utils/ResourceTypes.h
All the digits are in little endian.

Android Binary XML consists of file headr and chunks.
File header block consists of following 8 bytes.
0x00: Magic number in int32 (0x00080003)
0x04: File size

Chunks follow the header.
- String chunk
- Resource ID chunk
- Start Namespace chunk
- End Namespace chunk
- Start Element chunk
- End Element chunk

A chunk starts with following 8 bytes.
0x00: Magic number in int32.
0x04: Size of this block in int32.

Next chunk is located at (the head of a chunk + (value of 0x04)).

-- String chunk
0x00: Magic number in int32 (0x001C0001)
0x04: Size of this block in int32
0x08: Number of the entries in the string table in int32
0x0C: Number of style in int32 (unused). Expected to be 0.
0x10: Flags in int32 (unused)
0x14: String data offset in int32 (unused)
0x18: Style data offset in int32 (unused). Expected to be 0.
0x1C: Offset table of the string table, consists of int16.
      The size of this block is (value of 0x08 * 2).
0x1C + (value of 0x08 * 2): Strings (repeated).
      Each strings start with size value (int16)
      and the body (UTF-16LE).

-- Resource ID chunk
0x00: Magic number in int32 (0x00080180)
0x04: Size of this block in int32
(Omit the descirption of the content. We simply ignore them)

-- Namespace chunk
0x00: Magic number in int32 (0x00100100)
0x04: Size of this block in int32
(Omit the descirption of the content. We simply ignore them)

-- Element chunk
0x00: Start-element header in int32 (0x00100102)
0x04: Size of this block in int32
0x08: Line number (unused)
0x1C: XML Comment or 0xFFFFFFFF if none.
0x10: Namespace name index.
      Reference to string table in int32, or 0xFFFFFFFF for default NS.
0x14: Element name index. Reference to string table in int32.
0x18: Attribute start offset from 0x10 in uint16. Always 0x14.
0x1A: Attribute size in uint16. Always 0x14.
0x1C: Attribute count in uint16
0x1E: Index of 'id' attribute in uint16
0x20: Index of 'class' attribute in uint16
0x22: Index of 'style' attribute in uint16
0x24: Attributes.

Each Attribute (=5*4bytes) consists of
0x00: Namespace name.
      Reference to string table in int32, or 0xFFFFFFFF for default NS.
0x04: Attribute name. Reference to string table in int32.
0x08: Attribute value.
0x0C: Value type. This script handles only int constant, 0x10000008.
0x10: Value
"""

__author__ = "matsuzakit"

import logging
import struct


# Magic numbers
START_FILE_TAG = 0x00080003

CHUNK_TAG_STRING = 0x001C0001
CHUNK_TAG_RESOURCE_ID = 0x00080180
CHUNK_TAG_START_NAMESPACE = 0x00100100
CHUNK_TAG_END_NAMESPACE = 0x00100101
CHUNK_TAG_START_ELEMENT = 0x00100102
CHUNK_TAG_END_ELEMENT = 0x00100103

ATTRIBUTE_START_OFFSET = 0x14
ATTRIBUTE_STRUCT_SIZE = 0x14

# Struct formats
FILE_HEADER_FORMAT = '<II'
CHUNK_HEADER_FORMAT = '<II'
STRING_CHUNK_CONTENT_FORMAT = '<IIIIIII'
STRING_ENTRY_SIZE_FORMAT = '<H'
START_ELEMENT_FORMAT = '<IIIIIIHHHHHH'
END_ELEMENT_FORMAT = '<IIIIII'
ATTRIBUTE_FORMAT = '<IIIII'

DEFAULT_NS_INDEX = 0xFFFFFFFF


class Error(Exception):
  """Base exception class for this module."""
  pass


class UnexpectedFormatException(Error):
  """If the XML file's format does not meet our expectation."""
  pass


class IllegalArgumentException(Error):
  """If passed argument is illegal."""
  pass


def _Unpack(content, offset, format_string):
  """Unpack the content at the offset.

  Args:
    content: String to be unpacked.
    offset: Offset from the beginning of the content.
    format_string: Format string of struct module.
  Returns:
    See struct.unpack.
  """
  size = struct.calcsize(format_string)
  result = struct.unpack(format_string, content[offset : offset + size])
  return result


def _Pack(content, offset, format_string, values):
  """Pack values to the content at the offset.

  Args:
    content: String to be packed.
    offset: Offset from the beginning of the file.
    format_string: Format string of struct module.
    values: Values to struct.pack.
  Returns:
    Updated content.
  """
  size = struct.calcsize(format_string)
  return ''.join([content[:offset],
                  struct.pack(format_string, *values),
                  content[offset + size:]])


class Serializable(object):
  """Serializable object."""

  def __init__(self, content):
    self._content = content

  def Serialize(self, output_stream):
    """Serializes the content (self._content)."""
    output_stream.write(self._content)


class Chunk(Serializable):
  """Generic chunk."""

  def __init__(self, content):
    Serializable.__init__(self, content)

  def SetStringChunk(self, string_chunk):
    """Sets string chunk.

    On initialization phase string chunk might not be available
    so after parsing invoke this to set it.

    Args:
      string_chunk: StringChunk to be set.
    """
    self._string_chunk = string_chunk


class StringChunk(Chunk):
  """String chunk.

  Only one String chunks should be in the file.

  Attributes:
    string_entries: List of string entries.
      Other chunks would access the entries via the list's index.
  """

  def __init__(self, content):
    Chunk.__init__(self, content)
    logging.debug('-- String chunk')

    (chunk_tag,
     _,  # chunk_size
     string_entry_size,
     style_size,
     _,  # flag
     _,  # string_data_offset
     style_data_offset) = _Unpack(content, 0, STRING_CHUNK_CONTENT_FORMAT)
    if chunk_tag != CHUNK_TAG_STRING:
      raise UnexpectedFormatException('CHUNK_TAG_STRING is not found')

    logging.debug('String entry size: %d', string_entry_size)
    if style_size != 0 or style_data_offset != 0:
      raise UnexpectedFormatException(
          'Unexpected style data is found.')

    # Parse string offset table block.
    string_offset_format = '<%s' % ('I' * string_entry_size)
    string_offset_table = _Unpack(content,
                                  struct.calcsize(STRING_CHUNK_CONTENT_FORMAT),
                                  string_offset_format)

    # Parse string table block.
    string_entries_offset = (struct.calcsize(STRING_CHUNK_CONTENT_FORMAT)
                             + struct.calcsize(string_offset_format))
    self.string_entries = []
    for i in xrange(0, string_entry_size):
      entry_start_offset = string_offset_table[i] + string_entries_offset
      # The size of the string, in character length of UTF-16.
      entry_size = _Unpack(content, entry_start_offset,
                           STRING_ENTRY_SIZE_FORMAT)[0]
      string_body_offset = (entry_start_offset
                            + struct.calcsize(STRING_ENTRY_SIZE_FORMAT))
      # Each UTF-16 character consumes 2 bytes.
      entry_in_utf16 = content[string_body_offset :
                               string_body_offset + entry_size * 2]
      entry = unicode(entry_in_utf16, 'utf-16le').encode('utf-8')
      logging.debug('String entry #%d (0x%X): %s', i, entry_start_offset, entry)
      self.string_entries.append(entry)

  def GetIndexByString(self, string):
    return self.string_entries.index(string)


class Attribute(Serializable):
  """Attribute in StartElementChunk.

  Attributes:
    namespace_name_index: Index in String Chunk of namespace name.
    attribute_name_index: Index in String Chunk of attribute name.
  """

  def __init__(self, content):
    Serializable.__init__(self, content)
    (self.namespace_name_index,
     self.attribute_name_index,
     _,
     _,  # value_type
     _) = _Unpack(content, 0, ATTRIBUTE_FORMAT)
    logging.debug('attr %s:%s',
                  self.namespace_name_index, self.attribute_name_index)

  def GetIntValue(self):
    """Gets the value as integer.

    The value might be updated by SetIntValue so direct access to the value
    is not expected.
    Returns:
      Integer value of the attribute.
    """
    value = _Unpack(self._content, 0, ATTRIBUTE_FORMAT)[4]
    return value

  def SetIntValue(self, value):
    """Sets the value as integer. This updates the result of serialize."""
    values = _Unpack(self._content, 0, ATTRIBUTE_FORMAT)
    new_values = list(values)
    new_values[4] = value
    self._content = _Pack(self._content, 0, ATTRIBUTE_FORMAT, new_values)


class StartElementChunk(Chunk):
  """Start element chunk.

  This chunk contains all the informations about an element.
  Attributes:
    namespace_name_index: Index in String Chunk of namespace name.
    element_name_index: Index in String Chunk of element name.
  """

  def __init__(self, content):
    Chunk.__init__(self, content[:struct.calcsize(START_ELEMENT_FORMAT)])
    logging.debug('-- Start Element chunk')
    (chunk_tag,
     _,  # chunk_size
     _,  # line_number
     _,  # comment_index
     self.namespace_name_index,
     self.element_name_index,
     attribute_start,
     attribute_struct_size,
     attribute_size,
     _,
     _,
     _) = _Unpack(content, 0, START_ELEMENT_FORMAT)
    if chunk_tag != CHUNK_TAG_START_ELEMENT:
      raise UnexpectedFormatException('CHUNK_TAG_START_ELEMENT is not found')
    logging.debug('<%d:%d>',
                  self.namespace_name_index, self.element_name_index)
    if attribute_start != ATTRIBUTE_START_OFFSET:
      raise UnexpectedFormatException(
          'attribute_start must be %d but met %d.'
          % (ATTRIBUTE_START_OFFSET, attribute_start))
    if attribute_struct_size != ATTRIBUTE_STRUCT_SIZE:
      raise UnexpectedFormatException(
          'attribute_struct_size must be %d but met %d.'
          % (ATTRIBUTE_STRUCT_SIZE, attribute_start))

    self._attributes = []
    for i in xrange(0, attribute_size):
      offset = struct.calcsize(START_ELEMENT_FORMAT) + i * ATTRIBUTE_STRUCT_SIZE
      self._attributes.append(
          Attribute(content[offset : offset + ATTRIBUTE_STRUCT_SIZE]))

  def GetAttribute(self, namespace_name, attribute_name):
    """Gets an attribute if exists.

    Args:
      namespace_name: Namespace name or None for default NS.
      attribute_name: Attribute name.
    Returns:
      An attribute or None if not exists.
    Raises:
      IllegalArgumentException: Given attribute_name is not in string chunk.
    """
    if namespace_name:
      namespace_index = self._string_chunk.GetIndexByString(namespace_name)
    else:
      namespace_index = DEFAULT_NS_INDEX
    attribute_index = self._string_chunk.GetIndexByString(attribute_name)
    if attribute_index is None:
      raise IllegalArgumentException('Attribute %s is not found',
                                     attribute_name)
    for attribute in self._attributes:
      if (attribute.namespace_name_index == namespace_index and
          attribute.attribute_name_index == attribute_index):
        return attribute
    return None

  def Serialize(self, output_stream):
    super(StartElementChunk, self).Serialize(output_stream)
    for attr in self._attributes:
      attr.Serialize(output_stream)


class EndElementChunk(Chunk):
  def __init__(self, content):
    Chunk.__init__(self, content)
    logging.debug('-- End Element chunk')
    (chunk_tag,
     _,  # chunk_size
     _,  # line_number
     _,
     namespace_name_index,
     element_name_index) = _Unpack(content, 0, END_ELEMENT_FORMAT)
    if chunk_tag != CHUNK_TAG_END_ELEMENT:
      raise UnexpectedFormatException('CHUNK_TAG_END_ELEMENT is not found')
    logging.debug('</%d:%d>',
                  namespace_name_index, element_name_index)


class ResourceIdChunk(Chunk):
  def __init__(self, content):
    Chunk.__init__(self, content)
    logging.debug('-- Resource ID chunk')
    (chunk_tag, _) = _Unpack(content, 0, CHUNK_HEADER_FORMAT)
    if chunk_tag != CHUNK_TAG_RESOURCE_ID:
      raise UnexpectedFormatException('CHUNK_TAG_RESOURCE_ID is not found')


class StartNamespaceChunk(Chunk):
  def __init__(self, content):
    Chunk.__init__(self, content)
    logging.debug('-- Start Namespace chunk')
    (chunk_tag, _) = _Unpack(content, 0, CHUNK_HEADER_FORMAT)
    if chunk_tag != CHUNK_TAG_START_NAMESPACE:
      raise UnexpectedFormatException('CHUNK_TAG_START_NAMESPACE is not found')


class EndNamespaceChunk(Chunk):
  def __init__(self, content):
    Chunk.__init__(self, content)
    logging.debug('-- End Namespace chunk')
    (chunk_tag, _) = _Unpack(content, 0, CHUNK_HEADER_FORMAT)
    if chunk_tag != CHUNK_TAG_END_NAMESPACE:
      raise UnexpectedFormatException('CHUNK_TAG_END_NAMESPACE is not found')


TAG_TO_CHUNK = {
    CHUNK_TAG_STRING: StringChunk,
    CHUNK_TAG_RESOURCE_ID: ResourceIdChunk,
    CHUNK_TAG_START_NAMESPACE: StartNamespaceChunk,
    CHUNK_TAG_END_NAMESPACE: EndNamespaceChunk,
    CHUNK_TAG_START_ELEMENT: StartElementChunk,
    CHUNK_TAG_END_ELEMENT: EndElementChunk,
}


class AndroidBinaryXml(object):
  """Compiled XML file."""

  def __init__(self, xml_path):
    """Initialize an object.

    Args:
      xml_path: A path to an Android Binary XML file.
    Raises:
      UnexpectedFormatException:
        If the file format doesn't meet this script's expectation.
    """
    with open(xml_path) as xml_file:
      self._content = xml_file.read()
    logging.info('Read from %s', xml_path)
    self._chunks = self._GetChunks()

    # Inject string chunk.
    string_chunk = self._GetStringChunk()
    for chunk in self._chunks:
      chunk.SetStringChunk(string_chunk)

  def _GetChunks(self):
    (file_tag, file_size) = _Unpack(self._content, 0, FILE_HEADER_FORMAT)
    if file_tag != START_FILE_TAG:
      raise UnexpectedFormatException('START_FILE_TAG is not found.')
    if file_size != len(self._content):
      raise UnexpectedFormatException('File size is incorrect.')
    offset = struct.calcsize(FILE_HEADER_FORMAT)
    chunks = []
    while len(self._content) > offset:
      chunk_tag, chunk_size = _Unpack(self._content, offset,
                                      CHUNK_HEADER_FORMAT)
      partial_content = self._content[offset : offset + chunk_size]
      chunk_type = TAG_TO_CHUNK.get(chunk_tag)
      if chunk_type is None:
        raise UnexpectedFormatException('Unexpected tag 0x%X at 0x%X'
                                        % (chunk_tag, offset))
      chunks.append(chunk_type(partial_content))
      offset += chunk_size
    return chunks

  def _GetStringChunk(self):
    string_chunks = [c for c in self._chunks if isinstance(c, StringChunk)]
    if len(string_chunks) != 1:
      raise UnexpectedFormatException(
          'Found %d string chunks but only one is expected.')
    return string_chunks[0]

  def FindElements(self, namespace_name, element_name):
    """Finds elements which match given namespace_name and element_name.

    Args:
      namespace_name: Namespace name or None for default NS.
      element_name: Element name.
    Returns:
      Element list.
    Raises:
      IllegalArgumentException: If passed argument is illegal.
    """
    string_chunk = self._GetStringChunk()
    if namespace_name:
      namespace_index = string_chunk.GetIndexByString(namespace_name)
    else:
      namespace_index = DEFAULT_NS_INDEX
    if element_name is None:
      raise IllegalArgumentException('Element name is mandatory.')
    element_index = string_chunk.GetIndexByString(element_name)
    if element_index is None:
      raise IllegalArgumentException('Element %s is not found.', element_name)
    return [chunk for chunk in self._chunks
            if isinstance(chunk, StartElementChunk)
            and chunk.namespace_name_index == namespace_index
            and chunk.element_name_index == element_index]

  def Write(self, out_path):
    with open(out_path, 'w') as output_stream:
      output_stream.write(
          self._content[0 : struct.calcsize(FILE_HEADER_FORMAT)])
      for chunk in self._chunks:
        chunk.Serialize(output_stream)


if __name__ == '__main__':
  logging.info('This script is intended to be imported from other script.')
