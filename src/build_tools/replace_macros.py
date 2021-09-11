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

"""Replaces macros in a text file with given values.

For example,

  % python replace_macros.py --input=in.txt --output=out.txt \
      --define=enable_foo=1 --define=enable_bar=0 --define=baz=str

replaces @FOO@ with true, @BAR@ with false, and @BAZ@ with str.

There are following special prefixes.  Prefixes are not considered as
part of variable name.
  enable_ = The value is treated as a boolean.  No value defaults to true.
  disable_ = The value is treated as a boolean.  No value defaults to false.
  squoted_ = The value is treated as a string and single quotes in the value
      are escaped with backslash.  The value is surrounded by single quotes.
  dquoted_ = The value is treated as a string and double quotes in the value
      are escaped with backslash.  The value is surrounded by double quotes.
"""

__author__ = "yukishiino"


import optparse


def ParseOptions():
  """Parses command line options.

  Returns:
    Parsed options and arguments.
  """
  parser = optparse.OptionParser()
  parser.add_option('--input', dest='input')
  parser.add_option('--output', dest='output')
  parser.add_option('--define', dest='variables', action='append', default=[])

  return parser.parse_args()


def ParseVariableDefinitions(var_defs):
  """Parses variable definitions.

  Args:
    var_defs: A list of strings which define variables in the form of
        "[prefix_]var_name[=value]".

  Returns:
    A list in the form of [(prefix, var_name, value), ...].
  """
  prefixes = ('enable_', 'disable_', 'squoted_', 'dquoted_')

  def _TakeValue(var_value):
    var_value = var_value.split('=', 1)
    if len(var_value) == 1:
      return (var_value[0], None)
    else:
      return (var_value[0], var_value[1])

  def _SplitToPrefixVarNameAndValue(var_def):
    for prefix in prefixes:
      if var_def.startswith(prefix):
        return (prefix,) + _TakeValue(var_def[len(prefix):])
    else:
      return (None,) + _TakeValue(var_def)

  def _ParseVarDef(var_def):
    prefix, var, value = _SplitToPrefixVarNameAndValue(var_def)

    # Interpret a value from the value itself.
    if value is None:
      pass
    elif value.isdigit():
      value = int(value)
    elif value.lower() == 'true':
      value = True
    elif value.lower() == 'false':
      value = False

    # Re-interpret a value from the prefix.
    if prefix == 'enable_':
      value = True if value is None else bool(value)
    elif prefix == 'disable_':
      prefix = 'enable_'
      value = False if value is None else not bool(value)
    elif prefix == 'squoted_' and value is not None:
      value = '\'' + str(value).replace('\'', '\\\'') + '\''
    elif prefix == 'dquoted_' and value is not None:
      value = '"' + str(value).replace('"', '\\"') + '"'

    return (prefix, var, value)

  return [_ParseVarDef(var_def) for var_def in var_defs]


def TransformValuesToCStyle(variables):
  """Returns variables with values replaced with strings suitable for print.

  Args:
    variables: A list in the form of [(prefix, var_name, values), ...].

  Returns:
    The same list as |variables| but some of values are replaced with
    printable strings.
  """

  def _ToCStyle(value):
    if type(value) is bool:
      return 'true' if value else 'false'
    else:
      return value

  return [(prefix, var, _ToCStyle(value)) for prefix, var, value in variables]


def ReplaceVariables(text, variables):
  """Returns the |text| with variables replaced with their values.

  Args:
    text: A string to be replaced.
    variables: A list in the form of [(prefix, var_name, values), ...].

  Returns:
    A replaced string.
  """
  for unused_prefix, var_name, value in variables:
    if var_name:
      text = text.replace('@%s@' % var_name.upper(), str(value))

  return text


def main():
  """The main function."""
  (options, unused_args) = ParseOptions()
  if unused_args:
    raise RuntimeError('too many arguments')

  if options.input is None:
    raise RuntimeError('no --input option')
  if options.output is None:
    raise RuntimeError('no --output option')

  variables = ParseVariableDefinitions(options.variables)

  if options.input.endswith('.py') or options.output.endswith('.py'):
    pass
  else:
    variables = TransformValuesToCStyle(variables)

  open(options.output, 'w').write(
      ReplaceVariables(open(options.input, 'r').read(),
                       variables))


if __name__ == '__main__':
  main()
