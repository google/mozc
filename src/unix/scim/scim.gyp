# Copyright 2010, Google Inc.
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

{
  'variables': {
    'variables': {
      # SYSROOT directory path used for Chrome OS build
      'sysroot%': '',
    },
    'sysroot%': '<(sysroot)',
    'scim_dependencies': [
      '../../client/client.gyp:client',
      '../../session/session.gyp:genproto_session',
      '../../session/session.gyp:session',
    ],
    # These directories come from "pkg-config --cflags-only-I gtk+-2.0 scim"
    # on Ubuntu Hardy. Until it becomes necessary, we will not invoke
    # pkg-config here.
    'scim_include_dirs': [
      '/usr/include/atk-1.0',
      '/usr/include/cairo',
      '/usr/include/freetype2',
      '/usr/include/glib-2.0',
      '/usr/include/gtk-2.0',
      '/usr/include/libpng12',
      '/usr/include/pango-1.0',
      '/usr/include/pixman-1',
      '/usr/include/scim-1.0',
      '/usr/lib/glib-2.0/include',
      '/usr/lib/gtk-2.0/include',
    ],
    # The libraries come from "pkg-config --libs-only-l scim gtk+-2.0".
    'scim_libraries': [
      '-latk-1.0',
      '-lcairo',
      '-lgdk-x11-2.0',
      '-lgdk_pixbuf-2.0',
      '-lglib-2.0',
      '-lgmodule-2.0',
      '-lgobject-2.0',
      '-lgtk-x11-2.0',
      '-lm',
      '-lpango-1.0',
      '-lpangocairo-1.0',
      '-lscim-1.0',
    ],
    'scim_cflags': [
      '-fPIC',
    ],
    'scim_defines': [
      'SCIM_ICONDIR="/usr/share/scim/icons"',
    ]
  },
  'targets': [
    {
      'target_name': 'scim_mozc',
      'type': 'shared_library',
      'sources': [
        'imengine_factory.cc',
        'mozc_connection.cc',
        'mozc_lookup_table.cc',
        'mozc_response_parser.cc',
        'scim_key_translator.cc',
        'scim_mozc.cc',
        'scim_mozc_entry.cc',
      ],
      'dependencies': [
        '<@(scim_dependencies)',
      ],
      'include_dirs': [
        '<@(scim_include_dirs)',
      ],
      'defines': [
        '<@(scim_defines)',
      ],
    },
    {
      'target_name': 'scim_mozc_setup',
      'type': 'shared_library',
      'sources': [
        'scim_mozc_setup.cc',
      ],
      'dependencies': [
        '<@(scim_dependencies)',
      ],
      'include_dirs': [
        '<@(scim_include_dirs)',
      ],
      'defines': [
        '<@(scim_defines)',
      ],
    },
    {
      'target_name': 'scim_mozc_tests',
      'type': 'executable',
      'sources': [
        'mozc_response_parser_test.cc',
        'mozc_so_link_test.cc',
        'scim_key_translator_test.cc',
      ],
      'dependencies': [
        '<@(scim_dependencies)',
        '../../testing/testing.gyp:gtest_main',
        'scim_mozc',
      ],
      'include_dirs': [
        '<@(scim_include_dirs)',
      ],
      'libraries': [
        '<@(scim_libraries)',
      ],
      'cflags': [
        '<@(scim_cflags)',
      ],
      'ldflags': [
        # Add a library path to the directory of libscim_mozc.so.
        '-Wl,-rpath,<(PRODUCT_DIR)/lib.target',
      ],
    },
  ],
}
