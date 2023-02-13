#
# Copyright (c) 2010-2012 fcitx Project http://code.google.com/p/fcitx/
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
    'use_fcitx%': 'YES',
    'relative_dir': 'unix/fcitx',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
    'pkg_config_libs': [
      'fcitx',
      'fcitx-config',
      'fcitx-utils',
    ],
    'fcitx_dep_include_dirs': [
    ],
    'fcitx_dependencies': [
        '../../base/base.gyp:base',
        '../../client/client.gyp:client',
        '../../ipc/ipc.gyp:ipc',
        '../../protocol/protocol.gyp:commands_proto',
    ],
  },
  'conditions': [['use_fcitx=="YES"', {
  'targets': [
    {
      'target_name': 'gen_fcitx_mozc_i18n',
      'type': 'none',
      'actions': [
        {
          'action_name': 'gen_fcitx_mozc_i18n',
          'inputs': [
            './gen_fcitx_mozc_i18n.sh'
          ],
          'outputs': [
            '<(gen_out_dir)/po/zh_CN.mo',
            '<(gen_out_dir)/po/zh_TW.mo',
            '<(gen_out_dir)/po/ja.mo',
            '<(gen_out_dir)/po/de.mo',
          ],
          'action': [
            'sh',
            './gen_fcitx_mozc_i18n.sh',
            '<(gen_out_dir)/po',
          ],
        }],
    },
    {
      'target_name': 'fcitx-mozc',
      'product_prefix': '',
      'type': 'loadable_module',
      'sources': [
        'fcitx_mozc.cc',
        'fcitx_key_translator.cc',
        'fcitx_key_event_handler.cc',
        'mozc_connection.cc',
        'mozc_response_parser.cc',
        'surrounding_text_util.cc',
        'eim.cc',
      ],
      'dependencies': [
        '<@(fcitx_dependencies)',
        'gen_fcitx_mozc_i18n',
      ],
      'cflags': [
        '<!@(pkg-config --cflags <@(pkg_config_libs))',
      ],
      'include_dirs': [
        '<@(fcitx_dep_include_dirs)',
      ],
      'defines': [
        'LOCALEDIR="<!@(fcitx4-config --prefix)/share/locale/"',
      ],
    },
  ],
  }, {
  'targets': [
    {
      'target_name': 'no_fcitx_dummy',
      'type': 'none',
    }
  ]}
  ]],
}
