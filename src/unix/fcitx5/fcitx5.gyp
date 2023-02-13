#
# Copyright (c) 2010-2017 fcitx Project http://github.com/fcitx/
#
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of authors nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

{
  'variables': {
    'use_fcitx5%': 'YES',
    'relative_dir': 'unix/fcitx5',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
    'fcitx_dependencies': [
        '../../base/base.gyp:base',
        '../../client/client.gyp:client',
        '../../ipc/ipc.gyp:ipc',
        '../../protocol/protocol.gyp:commands_proto',
    ],
  },
  'conditions': [['use_fcitx5=="YES"', {
  'targets': [
    {
      # Meta target to set up build environment for fcitx5. Required 'cflags'
      # and 'link_settings' will be automatically injected into any target
      # which directly or indirectly depends on this target.
      'target_name': 'fcitx5_build_environment',
      'type': 'none',
      'variables': {
        'target_libs': [
          'Fcitx5Core',
          'Fcitx5Config',
          'Fcitx5Utils',
          'Fcitx5Module',
        ],
      },
      'all_dependent_settings': {
        'cflags': [
          '<!@(pkg-config --cflags <@(target_libs))',
        ],
        'link_settings': {
          'libraries': [
            '<!@(pkg-config --libs-only-l <@(target_libs))',
          ],
          'ldflags': [
            '<!@(pkg-config --libs-only-L <@(target_libs))',
          ],
        },
      },
    },
    {
      'target_name': 'fcitx5-mozc',
      'product_prefix': '',
      'type': 'loadable_module',
      'sources': [
        'fcitx_key_translator.cc',
        'fcitx_key_event_handler.cc',
        'surrounding_text_util.cc',
        'mozc_connection.cc',
        'mozc_response_parser.cc',
        'mozc_engine.cc',
        'mozc_engine_factory.cc',
        'mozc_state.cc',
      ],
      'dependencies': [
        '<@(fcitx_dependencies)',
        'fcitx5_build_environment',
      ],
      'cflags_cc': [
        '-std=c++17',
      ],
      'cflags_cc!': [
        '-std=gnu++0x'
      ],
      'cflags!': [
        '-fno-exceptions',
      ],
      'ldflags': [
        '-Wl,--no-undefined',
        '-Wl,--as-needed',
      ],
      'defines': [
        'FCITX_GETTEXT_DOMAIN="fcitx5-mozc"',
      ],
    },
  ],
  }, {
  'targets': [
    {
      'target_name': 'no_fcitx5_dummy',
      'type': 'none',
    }
  ]}
  ]],
}
