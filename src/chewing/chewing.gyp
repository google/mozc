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
    'relative_dir': 'chewing',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
    'pkg_config_libs': [
      'glib-2.0',
      'gobject-2.0',
      'ibus-1.0',
      'libcurl',
      'chewing',
    ],
    'ibus_client_dependencies' : [
      '../client/client.gyp:client',
      '../session/session_base.gyp:ime_switch_util',
    ],
    'ibus_standalone_dependencies' : [
      '../net/net.gyp:net',
      '../session/session_base.gyp:genproto_session',
      '../usage_stats/usage_stats.gyp:usage_stats',
    ],
  },
  'targets': [
    {
      'target_name': 'chewing_session',
      'type': 'static_library',
      'sources': [
        'session.cc',
        'dummy_converter.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'chewing_session_server',
      ],
    },
    # This is a copy of session.gyp:session_server but does not depend
    # on session.gyp:session not to link language model/dictionary of
    # Japanese.
    {
      'target_name': 'chewing_session_server',
      'type': 'static_library',
      'sources': [
        '../session/session_handler.cc',
        '../session/session_observer_handler.cc',
        '../session/session_server.cc',
        '../session/session_usage_observer.cc',
        '../session/session_watch_dog.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../client/client.gyp:client',
        '../session/session_base.gyp:config_handler',
        '../session/session_base.gyp:genproto_session',
        '../session/session_base.gyp:ime_switch_util',
        '../session/session_base.gyp:keymap',
        '../session/session_base.gyp:session_protocol',
      ],
    },
  ],
  'conditions': [
    ['OS=="linux"', {
      'targets': [
        {
          'target_name': 'ibus_mozc_chewing_lib',
          'type': 'static_library',
          'sources': [
            '../unix/ibus/engine_registrar.cc',
            '../unix/ibus/key_translator.cc',
            '../unix/ibus/mozc_engine.cc',
            '../unix/ibus/path_util.cc',
            'unix/ibus/mozc_engine_property.cc',
          ],
          'dependencies': [
            '../session/session_base.gyp:ime_switch_util',
          ],
          'conditions': [
            ['chromeos==1', {
             'sources': [
               '../unix/ibus/session.cc',
               '../unix/ibus/config_util.cc',
               'unix/ibus/config_updater.cc',
            ],
            }],
           ],
          'cflags': [
            '<!@(pkg-config --cflags <@(pkg_config_libs))',
          ],
        },
        {
          'target_name': 'ibus_mozc_chewing',
          'type': 'executable',
          'sources': [
            'unix/ibus/main.cc',
          ],
          'dependencies': [
            '../base/base.gyp:base',
            'chewing_session',
            'ibus_mozc_chewing_lib',
          ],
          'conditions': [
            ['chromeos==1', {
             'dependencies+': [
               '<@(ibus_standalone_dependencies)',
             ],
            }, {
             'dependencies+': [
               '<@(ibus_client_dependencies)',
             ],
            }],
           ],
          'cflags': [
            '<!@(pkg-config --cflags <@(pkg_config_libs))',
          ],
          'libraries': [
            '<!@(pkg-config --libs-only-l <@(pkg_config_libs))',
          ],
          'ldflags': [
            '<!@(pkg-config --libs-only-L <@(pkg_config_libs))',
          ],
        },
      ],
    }],
  ],
}
