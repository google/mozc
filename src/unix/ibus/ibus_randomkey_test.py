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

"""ibus_randomkey_test do the random-key comparison over ibus engines.

It generates random key sequences, sends them to two ibus engines, and
compares their results.  If it's different, it will raise warnings.

Sample command line:
python unix/ibus/ibus_randomkey_test.py \
    --side_a_engine=mozc-chewing --side_b_engine=chewing \
    --test_files=path/to/typical_key_sequences.txt --no_check_consumed
"""

__author__ = "mukai"


import itertools
import logging
import optparse
import os
import random
import re
import sys
from ibus import inputcontext
from ibus import keysyms
from ibus import modifier
import ibus.bus


def GetRandomKey(key_type):
  """Create a random key based on the key_type.

  Args:
    key_type: A string to denote the type of key generated.
  Returns:
    A (keycode, modifier) tuple.
  """

  def GenerateRandomModifier():
    """Create a random modifier combination.

    Returns:
      An integer of a modifier combination.
    """
    modifiers = [
        modifier.SHIFT_MASK, modifier.CONTROL_MASK, modifier.ALT_MASK
        ]
    result = random.choice(modifiers)
    if random.random() > 0.5:
      result |= random.choice(modifiers)
      if random.random() > 0.5:
        result |= random.choice(modifiers)
    return result

  special_keys = [
      'BackSpace', 'Tab', 'Return', 'Escape', 'Delete', 'Kanji', 'Muhenkan',
      'Henkan_Mode', 'Romaji', 'Hiragana', 'Katakana', 'Hiragana_Katakana',
      'Zenkaku', 'Hankaku', 'Zenkaku_Hankaku', 'Kana_Shift', 'Eisu_Shift',
      'Eisu_toggle', 'Home', 'Left', 'Up', 'Right', 'Down', 'Prior', 'Page_Up',
      'Page_Down', 'End', 'F1', 'F2', 'F3', 'F4', 'F5', 'F6', 'F7', 'F8', 'F9',
      'F10', 'F11', 'F12'
      # we do not check keypad keys because ibus-mozc does not support them.
      # 'KP_Tab', 'KP_Enter', 'KP_Home', 'KP_Left', 'KP_Up',
      # 'KP_Right', 'KP_Down', 'KP_Page_Up', 'KP_Delete',
      ]
  modifier_keys = [
      'Shift_L', 'Shift_R', 'Control_L', 'Control_R', 'Caps_Lock', 'Shift_Lock',
      'Meta_L', 'Meta_R', 'Alt_L', 'Alt_R'
      ]
  normal_keys = [
      'space', 'exclam', 'quotedbl', 'percent', 'ampersand', 'apostrophe',
      'quoteright', 'parenleft', 'parenright', 'asterisk', 'plus', 'comma',
      'minus', 'period', 'slash', '0', '1', '2', '3', '4', '5', '6', '7', '8',
      '9', 'colon', 'semicolon', 'less', 'equal', 'greater', 'question', 'at',
      'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
      'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'bracketleft',
      'backslash', 'bracketright', 'asciicircum', 'underscore', 'grave',
      'quoteleft', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l',
      'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
      'braceleft', 'asciitilde',
      # we do not check keypad keys because ibus-mozc does not support them.
      # 'KP_Space', 'KP_Equal', 'KP_Multiply', 'KP_Add', 'KP_Separator',
      # 'KP_Subtract', 'KP_Decimal', 'KP_Divide', 'KP_0', 'KP_0', 'KP_1',
      # 'KP_2', 'KP_3', 'KP_4', 'KP_5', 'KP_6', 'KP_7', 'KP_8', 'KP_9',
      ]

  special_keys = map(keysyms.name_to_keycode, special_keys)
  modifier_keys = map(keysyms.name_to_keycode, modifier_keys)
  normal_keys = map(keysyms.name_to_keycode, normal_keys)

  if key_type == 'normal':
    return (random.choice(normal_keys), 0)
  elif key_type == 'modifier':
    return (random.choice(normal_keys), GenerateRandomModifier())
  elif key_type == 'special':
    return (random.choice(special_keys), 0)
  elif key_type == 'special_modifier':
    return (random.choice(special_keys), GenerateRandomModifier())
  elif key_type == 'modifier_only':
    return (random.choice(modifier_keys), 0)
  else:
    raise NotImplementedError


def DebugKeySequence(key_sequence):
  """Creates human-readable representation of the key sequence.

  The output format is as follows:
    - A key event is formatted as (key, modifier)
    - Key is the name of the key code.  See ibus.modifier module.
    - If no modifier keys are pressed, modifier is 'no_mod'.
      Otherwise, modifier is a concatination of 'SHIFT', 'CONTROL', or 'ALT'.
    - and key events are separated by comma ','
  Args:
    key_sequence: A list of (keycode, modifier) tuples.
  Returns:
    A human-readable string which represents the key_sequence.
  """
  elems = []
  for (keycode, mods) in key_sequence:
    if mods == 0:
      mod_str = 'no_mod'
    else:
      mod_str = ''
      if mods & modifier.SHIFT_MASK != 0:
        mod_str += 'SHIFT'
      if mods & modifier.CONTROL_MASK != 0:
        mod_str += 'CONTROL'
      if mods & modifier.ALT_MASK != 0:
        mod_str += 'ALT'
    elems.append('(%s, %s)' % (keysyms.keycode_to_name(keycode), mod_str))
  return ', '.join(elems)


def ParseKeySequence(line):
  """Parse a key sequence.

  The input key sequence format is same as the DebugKeySequence output
  result.  See the docstring above.
  Args:
    line: A string which represents a key sequence.
  Returns:
    A list of (keycode, modifier) tuples.
  """
  result_sequence = []
  for matched in re.finditer(r'\((\w+), (\w+)\)', line):
    keycode = keysyms.name_to_keycode(matched.group(1))
    mod_str = matched.group(2)
    mods = 0
    if mod_str.find('SHIFT') >= 0:
      mods |= modifier.SHIFT_MASK
    if mod_str.find('CONTROL') >= 0:
      mods |= modifier.CONTROL_MASK
    if mod_str.find('ALT') >= 0:
      mods |= modifier.ALT_MASK
    result_sequence.append((keycode, mods))
  return result_sequence


class TestInputContext(object):
  def __init__(self, bus, path_name, engine_name):
    """Constructor.

    Args:
      bus: An ibus.Bus.
      path_name: A string to distinguish this context from others.
      engine_name: The engine name of the context.
    """
    context_path = bus.create_input_context(path_name)
    self._context = inputcontext.InputContext(bus, context_path)
    self._context.connect('commit-text', self.__CommitText)
    self._context.connect('update-preedit-text', self.__UpdatePreeditText)
    self._context.connect('update-auxiliary-text', self.__UpdateAuxiliaryText)
    self._context.connect('update-lookup-table', self.__UpdateLookupTable)
    engines = [engine for engine in bus.list_active_engines()
               if engine.name == engine_name]
    self._engine = engines[0]
    self.ClearContext()

  def __CommitText(self, unused_context, text):
    self._commit_text = text.text

  def __UpdatePreeditText(self, unused_context, text, cursor_pos, visible):
    self._preedit_text = text.text
    self._preedit_visible = visible
    self._preedit_cursor = cursor_pos

  def __UpdateAuxiliaryText(self, unused_context, text, visible):
    self._aux_text = text.text
    self._aux_visible = visible

  def __UpdateLookupTable(self, unused_context, table, visible):
    self._lookup_table_visible = visible
    self._lookup_table = table

  def ClearContext(self):
    """Clears the current input context."""
    self._context.reset()
    self._context.set_engine(self._engine)
    self._commit_text = ''
    self._preedit_text = ''
    self._preedit_visible = False
    self._preedit_cursor = ''
    self._aux_text = ''
    self._aux_visible = ''
    self._lookup_table_visible = False
    self._lookup_table = None
    self._last_keydown_consumed = False
    self._last_keyup_consumed = False

  def ProcessKeyEvent(self, keycode, mods):
    """Process the specified key event for the engine.

    Args:
      keycode: An integer of the key code.
      mods: An integer of the modifiers.
    """
    # keydown
    self._last_keydown_consumed = self._context.process_key_event(
        keycode, keycode, mods)
    # keyup
    self._last_keyup_consumed = self._context.process_key_event(
        keycode, keycode, mods | modifier.RELEASE_MASK)

  def CompareContext(self, that, check_consumed):
    """Compare the current context with that.

    Args:
      that: Another TestInputContext instance for comparison.
      check_consumed: False if it does not check the keyevent consumption.
    Returns:
      True if self and that is in the same context.
    """
    result = True
    check_fields = ['commit_text', 'preedit_visible', 'preedit_cursor',
                    'aux_text', 'aux_visible', 'lookup_table_visible']
    if check_consumed:
      check_fields.extend(['last_keydown_consumed', 'last_keyup_consumed'])
    for field in check_fields:
      self_value = getattr(self, '_' + field)
      that_value = getattr(that, '_' + field)
      if self_value != that_value:
        logging.warning('%s %s %s', field, self_value, that_value)
        result = False

    if not self._lookup_table_visible or not that._lookup_table_visible:
      return result
    self_table = self._lookup_table
    that_table = that._lookup_table
    for method in ['is_cursor_visible', 'get_cursor_pos_in_current_page',
                   'orientation', 'get_candidates_in_current_page']:
      self_value = getattr(self_table, method)()
      that_value = getattr(that_table, method)()
      if self_value != that_value:
        logging.warning('%s %s %s', method, self_value, that_value)
        result = False
    return result


def CompareWithKeySequence(key_sequence, context_a, context_b, check_consumed):
  """Compare context_a and context_b with the key_sequence.

  Args:
    key_sequence: A list of (keycode, modifier) tuples to put to the contexts.
    context_a: A TestInputContext for comparison.
    context_b: The other TextInputContext for comparison.
    check_consumed: False if it does not check the keyevent consumption.
  """
  context_a.ClearContext()
  context_b.ClearContext()
  for (k, (keycode, mods)) in enumerate(key_sequence):
    context_a.ProcessKeyEvent(keycode, mods)
    context_b.ProcessKeyEvent(keycode, mods)
    if not context_a.CompareContext(context_b, check_consumed):
      logging.error('mismatch happens at %d-th element of %s',
                    k, DebugKeySequence(key_sequence))


def CreateOptionParser():
  """Parse command line options for test.

  Returns:
    The option parser for this test script.
  """
  parser = optparse.OptionParser()
  parser.add_option('--side_a_engine', dest='engine_a', default='',
                    help='The engine name to be test as side-A.')
  parser.add_option('--side_b_engine', dest='engine_b', default='',
                    help='The engine name to be test as side-B.')
  parser.add_option('--test_files', dest='test_files', default='',
                    help='Comma-separated file names to test manually created '
                    'key sequences.')
  parser.add_option('--no_random_test', dest='no_random_test',
                    action='store_true', default=False,
                    help='Specify if you don\'t want to test random key '
                    'sequences.')
  parser.add_option('--no_check_consumed', dest='check_consumed',
                    action='store_false', default=True,
                    help='Specify if you don\'t want to check whether engines '
                    'correctly consumed the key event or not.  This flag would '
                    'be useful to test with some buggy engines such like '
                    'ibus-chewing.')
  parser.add_option('--trial', default=2, type='int', dest='trial',
                    help='The number of random key sequences for each key '
                    'combination pattern.')
  parser.add_option('--keyevent_length', default=4, type='int',
                    dest='keyevent_length', help='The number of key events in '
                    'a random key sequence.')
  parser.add_option('--random_seed', default=None, type='int',
                    dest='random_seed', help='Specify the seed of random key '
                    'events to reproduce issues.')
  return parser

def main():
  """main function."""
  key_types = [
      'normal', 'modifier', 'special', 'special_modifier', 'modifier_only']

  optparser = CreateOptionParser()
  (options, unused_args) = optparser.parse_args()
  if not options.engine_a or not options.engine_b:
    parser.error('please specify the engines for testing.')

  bus = ibus.bus.Bus()
  side_a = TestInputContext(
      bus, 'test-side-a-%d' % os.getpid(), options.engine_a)
  side_b = TestInputContext(
      bus, 'test-side-b-%d' % os.getpid(), options.engine_b)
  for keyseq_file in re.split(r'\s*,\s*', options.test_files):
    if not keyseq_file:
      # sometimes keyseq_file can be an empty string.
      continue
    for (line_number, line) in enumerate(file(keyseq_file)):
      logging.info(line)
      key_sequence = ParseKeySequence(line)
      CompareWithKeySequence(
          key_sequence, side_a, side_b, options.check_consumed)

  if options.no_random_test:
    return

  if options.random_seed:
    random.seed(options.random_seed)

  for key_type_seq in itertools.product(
      key_types, repeat=options.keyevent_length):
    print 'types: %s' % repr(key_type_seq)
    for j in range(options.trial):
      logging.info('trial %d', j)
      key_sequence = [GetRandomKey(key_type) for key_type in key_type_seq]
      CompareWithKeySequence(
          key_sequence, side_a, side_b, options.check_consumed)


if __name__ == '__main__':
  main()
