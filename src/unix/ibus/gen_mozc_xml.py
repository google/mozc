# -*- coding: utf-8 -*-
# Copyright 2010-2011, Google Inc.
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

__author__ = "yusukes"

import optparse
import sys

# Information to generate <component> part of mozc.xml. %s will be replaced with
# a product name, 'Mozc' or 'Google Japanese Input'.
IBUS_COMPONENT_PROPS = {
    'name': 'com.google.IBus.Mozc',
    'description': '%s Component',
    # TODO(yusukes): Support Linux distributions other than Gentoo/ChromeOS.
    # For example, Ubuntu uses /usr/lib/ibus-mozc/.
    'exec': '/usr/libexec/ibus-engine-mozc --ibus',
    # TODO(mazda): Generate the version number.
    'version': '0.0.0.0',
    'author': 'Google Inc.',
    'license': 'New BSD',
    'homepage': 'http://code.google.com/p/mozc/',
    'textdomain': 'ibus-mozc',
}

# Information to generate <engines> part of mozc.xml.
IBUS_ENGINE_COMMON_PROPS = {
    'description': '%s (Japanese Input Method)',
    'language': 'ja',
    'icon': '/usr/share/ibus-mozc/product_icon.png',
    'rank': '80',
}

# A dictionary from --platform to engines that are used in the platform. The
# information is used to generate <engines> part of mozc.xml.
IBUS_ENGINES_PROPS = {
    # On Linux, we provide only one engine for the Japanese keyboard layout.
    'Linux': {
        # DO NOT change the engine name 'mozc-jp'. The names is referenced by
        # unix/ibus/mozc_engine.cc.
        'name': ['mozc-jp'],
        'longname': ['%s'],
        'layout': ['jp'],
    },
    # On Chrome/Chromium OS, we provide three engines.
    'ChromeOS': {
        # DO NOT change the engine name 'mozc-jp'. The names is referenced by
        # unix/ibus/mozc_engine.cc.
        'name': ['mozc-jp', 'mozc', 'mozc-dv'],
        'longname': ['%s (Japanese keyboard layout)', '%s (US keyboard layout)',
                     '%s (US Dvorak keyboard layout)'],
        'layout': ['jp', 'us', 'us(dvorak)'],
    },
}

# A dictionary from --branding to a product name which is embedded into the
# properties above.
PRODUCT_NAMES = {
    'Mozc': 'Mozc',
    'GoogleJapaneseInput': 'Google Japanese Input',
}

CPP_HEADER = """// Copyright 2010 Google Inc. All Rights Reserved.

#ifndef %s
#define %s
namespace {"""

CPP_FOOTER = """}  // namespace
#endif  // %s"""


def EmbedProductName(product_name, format_string):
  if format_string.find('%s') != -1:
    return format_string % product_name
  return format_string


def OutputXmlElement(product_name, element_name, value):
  print '  <%s>%s</%s>' % (element_name, EmbedProductName(product_name, value),
                           element_name)


def OutputXml(product_name, component, engine_common, engines):
  """Outputs a XML data for ibus-daemon.

  Args:
    product_name: 'Mozc' or 'Google Japanese Input'
    component: A dictionary from a property name to a property value of the
        ibus-mozc component. For example, {'name': 'com.google.IBus.Mozc'}.
    engine_common: A dictionary from a property name to a property value that
        are commonly used in all engines. For example, {'language': 'ja'}.
    engines: A dictionary from a property name to a list of property values of
        engines. For example, {'name': ['mozc-jp', 'mozc', 'mozc-dv']}.
  """
  print '<component>'
  for key in component:
    OutputXmlElement(product_name, key, component[key])
  print '<engines>'
  for i in range(len(engines['name'])):
    print '<engine>'
    for key in engine_common:
      OutputXmlElement(product_name, key, engine_common[key])
    for key in engines:
      OutputXmlElement(product_name, key, engines[key][i])
    print '</engine>'
  print '</engines>'
  print '</component>'


def OutputCppVariable(product_name, prefix, variable_name, value):
  print 'const char k%s%s[] = "%s";' % (prefix, variable_name.capitalize(),
                                        EmbedProductName(product_name, value))


def OutputCpp(product_name, component, engine_common, engines):
  """Outputs a C++ header file for mozc/unix/ibus/main.cc.

  Args:
    product_name: see OutputXml.
    component: ditto.
    engine_common: ditto.
    engines: ditto.
  """
  guard_name = 'MOZC_UNIX_IBUS_MAIN_H_'
  print CPP_HEADER % (guard_name, guard_name)
  for key in component:
    OutputCppVariable(product_name, 'Component', key, component[key])
  for key in engine_common:
    OutputCppVariable(product_name, 'Engine', key, engine_common[key])
  for key in engines:
    print 'const char* kEngine%sArray[] = {' % key.capitalize()
    for i in range(len(engines[key])):
      print '"%s",' % EmbedProductName(product_name, engines[key][i])
    print '};'
  print 'const size_t kEngineArrayLen = %s;' % len(engines['name'])
  print CPP_FOOTER % guard_name


def main():
  """The main function."""
  parser = optparse.OptionParser(usage='Usage: %prog [options]')
  parser.add_option('--output_cpp', action='store_true',
                    dest='output_cpp', default=False,
                    help='If specified, output a C++ header. Otherwise, output '
                    'XML.')
  parser.add_option('--platform', dest='platform', default=None,
                    help='Platform where ibus-mozc runs. Currently, only two '
                    'platforms are supported: ChromeOS, Linux.')
  parser.add_option('--branding', dest='branding', default=None,
                    help='GoogleJapaneseInput for the ChromeOS official build. '
                    'Otherwise, Mozc.')
  (options, unused_args) = parser.parse_args()

  if options.output_cpp:
    OutputCpp(PRODUCT_NAMES[options.branding], IBUS_COMPONENT_PROPS,
              IBUS_ENGINE_COMMON_PROPS,
              IBUS_ENGINES_PROPS[options.platform])
  else:
    OutputXml(PRODUCT_NAMES[options.branding], IBUS_COMPONENT_PROPS,
              IBUS_ENGINE_COMMON_PROPS,
              IBUS_ENGINES_PROPS[options.platform])
  return 0

if __name__ == '__main__':
  sys.exit(main())
