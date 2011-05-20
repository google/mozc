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

{
  'variables': {
    'variables': {
      # SYSROOT directory path used for Chrome OS build
      'sysroot%': '',
    },
    'sysroot%': '<(sysroot)',
    'pkg_config_libs': [
      'scim',
      'gtk+-2.0',
    ],
    'scim_dependencies': [
      '../../client/client.gyp:client',
      '../../session/session.gyp:session',
    ],
    'scim_defines': [
      'SCIM_ICONDIR="<!@(pkg-config --variable=icondir scim)"',
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
      'cflags': [
        '<!@(pkg-config --cflags <@(pkg_config_libs))',
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
      'cflags': [
        '<!@(pkg-config --cflags <@(pkg_config_libs))',
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
      'cflags': [
        '<!@(pkg-config --cflags <@(pkg_config_libs))',
      ],
      'libraries': [
        '<!@(pkg-config --libs-only-l <@(pkg_config_libs))',
      ],
      'ldflags': [
        '<!@(pkg-config --libs-only-L <@(pkg_config_libs))',
        # Add a library path to the directory of libscim_mozc.so.
        '-Wl,-rpath,<(PRODUCT_DIR)/lib.target',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
  ],
}
