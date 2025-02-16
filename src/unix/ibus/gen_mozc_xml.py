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

"""Pre-generate XML data of ibus-mozc engines.

We don't support --xml command line option in the ibus-engines-mozc command
since it could make start-up time of ibus-daemon slow when XML cache of the
daemon in ~/.cache/ibus/bus/ is not ready or is expired.
"""

import optparse
import os
import sys

# A dictionary from --branding to a product name which is embedded into the
# properties above.
PRODUCT_NAMES = {
    'Mozc': 'Mozc',
    'GoogleJapaneseInput': 'Google Japanese Input',
}

CPP_HEADER = """// Copyright 2010 Google Inc. All Rights Reserved.

#ifndef %s
#define %s

#include <cstddef>

namespace {"""

CPP_FOOTER = """}  // namespace
#endif  // %s"""

XDG_CURRENT_DESKTOP_URL = 'https://specifications.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html#recognized-keys'


def GetXmlElement(key, value):
  if key == 'composition_mode':
    return '  <!-- %s : %s -->' % (key, value)
  return '  <%s>%s</%s>' % (key, value, key)


def GetTextProtoElement(key, value):
  if isinstance(value, int) or key == 'composition_mode':
    return '  %s : %s' % (key, value)
  return '  %s : "%s"' % (key, value)


def GetEnginesXml(engine_common, engines):
  """Outputs a XML data for ibus-daemon.

  Args:
    engine_common: A dictionary from a property name to a property value that
        are commonly used in all engines. For example, {'language': 'ja'}.
    engines: A dictionary from a property name to a list of property values of
        engines. For example, {'name': ['mozc-jp', 'mozc', 'mozc-dv']}.
  Returns:
    output string in XML.
  """
  output = ['<engines>']
  for engine in engines:
    output.append('<engine>')
    for key, value in engine_common.items():
      output.append(GetXmlElement(key, value))
    for key, value in engine.items():
      output.append(GetXmlElement(key, value))
    output.append('</engine>')
  output.append('</engines>')
  return '\n'.join(output)


def GetIbusConfigTextProto(engines):
  """Outputs a TextProto data for iBus config.

  Args:
    engines: A dictionary from a property name to a list of property values of
        engines. For example, {'name': ['mozc-jp', 'mozc', 'mozc-dv']}.
  Returns:
    output string in TextProto.
  """
  output = [
      '# `ibus write-cache; ibus restart` might be necessary to apply changes.'
  ]
  for engine in engines:
    output.append('engines {')
    for key, value in engine.items():
      output.append(GetTextProtoElement(key, value))
    output.append('}')
  output.append('active_on_launch: False')
  output.append('mozc_renderer {')
  output.append("  # Set 'False' to use IBus' candidate window.")
  output.append('  enabled : True')
  output.append("  # For Wayland sessions, 'mozc_renderer' will be used if and "
                "only if any value")
  output.append('  # set in this field (e.g. "GNOME", "KDE") is found in '
                '$XDG_CURRENT_DESKTOP.')
  output.append(f'  # {XDG_CURRENT_DESKTOP_URL}')
  output.append('  compatible_wayland_desktop_names : ["GNOME"]')
  output.append('}')
  return '\n'.join(output)


def OutputXml(component, ibus_mozc_path):
  """Outputs a XML data for ibus-daemon.

  Args:
    component: A dictionary from a property name to a property value of the
        ibus-mozc component. For example, {'name': 'com.google.IBus.Mozc'}.
    ibus_mozc_path: A path to ibus-engine-mozc.
  """
  print('<component>')
  for key, value in component.items():
    print(GetXmlElement(key, value))
  print('  <engines exec="%s --xml" />' % ibus_mozc_path)
  print('</component>')

  print('''
<!-- Settings of <engines> and <layout> are stored in ibus_config.textproto -->
<!-- under the user configuration directory, which is either of: -->
<!-- * $XDG_CONFIG_HOME/mozc/ibus_config.textproto -->
<!-- * $HOME/.config/mozc/ibus_config.textproto -->
<!-- * $HOME/.mozc/ibus_config.textproto -->
<!-- `ibus write-cache; ibus restart` might be necessary to apply changes. -->
''')


def OutputCppVariable(prefix, key, value):
  print('const char k%s%s[] = "%s";' % (prefix, key.capitalize(), value))


def OutputCpp(component, engine_common, engines):
  """Outputs a C++ header file for mozc/unix/ibus/main.cc.

  Args:
    component: A dictionary from a property name to a property value of the
        ibus-mozc component. For example, {'name': 'com.google.IBus.Mozc'}.
    engine_common: A dictionary from a property name to a property value that
        are commonly used in all engines. For example, {'language': 'ja'}.
    engines: A dictionary from a property name to a list of property values of
        engines. For example, [{'name': 'mozc-jp',...}, {'name': 'mozc'},...]
  """
  guard_name = 'MOZC_UNIX_IBUS_MAIN_H_'
  print(CPP_HEADER % (guard_name, guard_name))
  for key, value in component.items():
    OutputCppVariable('Component', key, value)
  for key, value in engine_common.items():
    OutputCppVariable('Engine', key, value)
  print('const size_t kEngineArrayLen = %s;' % len(engines))
  print('const char kEnginesXml[] = R"#(', end='')
  print(GetEnginesXml(engine_common, engines))
  print(')#";')
  print('const char kIbusConfigTextProto[] = R"#(', end='')
  print(GetIbusConfigTextProto(engines))
  print(')#";')
  print(CPP_FOOTER % guard_name)


def main():
  """The main function."""
  parser = optparse.OptionParser(usage='Usage: %prog [options]')
  parser.add_option('--output_cpp', action='store_true',
                    dest='output_cpp', default=False,
                    help='If specified, output a C++ header. Otherwise, output '
                    'XML.')
  parser.add_option('--branding', dest='branding', default=None,
                    help='GoogleJapaneseInput for the official build. '
                    'Otherwise, Mozc.')
  parser.add_option('--ibus_mozc_path', dest='ibus_mozc_path', default='',
                    help='The absolute path of ibus_mozc executable.')
  parser.add_option('--ibus_mozc_icon_path', dest='ibus_mozc_icon_path',
                    default='', help='The absolute path of ibus_mozc icon.')
  parser.add_option('--server_dir', dest='server_dir', default='',
                    help='The absolute directory path to be installed the '
                    'server executable.')
  (options, unused_args) = parser.parse_args()

  product_name = PRODUCT_NAMES[options.branding]
  ibus_mozc_path = options.ibus_mozc_path
  ibus_mozc_icon_path = options.ibus_mozc_icon_path
  setup_path = os.path.join(options.server_dir, 'mozc_tool')

  # Information to generate <component> part of mozc.xml.
  component = {
      'name': 'com.google.IBus.Mozc',
      'description': product_name + ' Component',
      'exec': ibus_mozc_path + ' --ibus',
      # TODO(mazda): Generate the version number.
      'version': '0.0.0.0',
      'author': 'Google LLC',
      'license': 'New BSD',
      'homepage': 'https://github.com/google/mozc',
      'textdomain': 'ibus-mozc',
  }

  engine_common = {
      'description': product_name + ' (Japanese Input Method)',
      'language': 'ja',
      'icon': ibus_mozc_icon_path,
      # Make sure that the property key 'InputMode' matches to the property name
      # specified to |ibus_property_new| in unix/ibus/property_handler.cc
      'icon_prop_key': 'InputMode',
      'setup': setup_path + ' --mode=config_dialog',
  }

  # DO NOT change the engine name 'mozc-jp'. The names is referenced by
  # unix/ibus/mozc_engine.cc.
  engines = [{
      'name': 'mozc-jp',
      'longname': product_name,
      'layout': 'default',
      'layout_variant': '',
      'layout_option': '',
      'rank': 80,
      'symbol': 'あ',
  }, {
      'name': 'mozc-on',
      'longname': product_name + u':あ',
      'layout': 'default',
      'layout_variant': '',
      'layout_option': '',
      'rank': 99,
      'symbol': 'あ',
      'composition_mode': 'HIRAGANA',
  }, {
      'name': 'mozc-off',
      'longname': product_name + ':A_',
      'layout': 'default',
      'layout_variant': '',
      'layout_option': '',
      'rank': 99,
      'symbol': 'A',
      'composition_mode': 'DIRECT',
  }]

  if options.output_cpp:
    OutputCpp(component, engine_common, engines)
  else:
    OutputXml(component, ibus_mozc_path)
  return 0

if __name__ == '__main__':
  sys.exit(main())
