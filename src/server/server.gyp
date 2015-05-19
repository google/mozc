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

{
  'variables': {
    'relative_dir': 'server',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
    'conditions': [
      ['branding=="GoogleJapaneseInput"', {
        'converter_product_name_win': 'GoogleIMEJaConverter',
        'cache_service_product_name_win': 'GoogleIMEJaCacheService',
      }, {  # else
        'converter_product_name_win': 'mozc_server',
        'cache_service_product_name_win': 'mozc_cache_service',
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'mozc_server',
      'type': 'executable',
      'sources': [
        'server_main.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../engine/engine.gyp:engine_factory',
        '../session/session.gyp:session',
        'mozc_server_lib',
      ],
      'conditions': [
        ['OS=="mac"', {
          'product_name': '<(branding)Converter',
          'sources': [
            '../data/mac/mozc_server_info',
          ],
          'dependencies': [
            'gen_mozc_server_info_plist',
          ],
          'mac_bundle': 1,
          'mac_bundle_resources': [
            '../data/images/mac/product_icon.icns'
          ],
          'xcode_settings': {
            # Currently metadata in the Info.plist file like version
            # info go away because the generated xcodeproj don't know
            # version info.
            # TODO(mukai): write a script to expand those variables
            # and use that script instead of this INFOPLIST_FILE.
            'INFOPLIST_FILE': '<(gen_out_dir)/mozc_server_info',
          },
          'variables': {
            # This product name is used in postbuilds_mac.gypi.
            'product_name': '<(branding)Converter',
          },
          'includes': [
            '../gyp/postbuilds_mac.gypi',
          ],
        }],
        ['OS=="win"', {
          'product_name': '<(converter_product_name_win)',
          'includes': [
            '../gyp/postbuilds_win.gypi',
          ],
          'sources': [
            'mozc_server.exe.manifest',
            '<(gen_out_dir)/mozc_server_autogen.rc',
          ],
          'dependencies': [
            'gen_mozc_server_resource_header',
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'DelayLoadDLLs': [
                'version.dll',
                'wininet.dll',
              ],
            },
          }
        }],
      ]
    },
    {
      'target_name': 'mozc_server_lib',
      'type': 'static_library',
      'sources': [
        'mozc_server.cc',
      ],
      'dependencies': [
        '../session/session.gyp:session_server',
        '../usage_stats/usage_stats.gyp:usage_stats',
      ],
    },
    {
      'target_name': 'mozc_rpc_server_main',
      'type': 'executable',
      'sources': [
        'mozc_rpc_server_main.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../engine/engine.gyp:engine_factory',
        '../session/session.gyp:session',
        '../session/session.gyp:session_server',
        '../session/session.gyp:random_keyevents_generator',
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'genproto_server',
          'type': 'none',
          'sources': [
            'win32_service_state.proto',
          ],
          'includes': [
            '../protobuf/genproto.gypi',
          ],
        },
        {
          'target_name': 'server_protocol',
          'type': 'static_library',
          'hard_dependency': 1,
          'sources': [
            '<(proto_out_dir)/<(relative_dir)/win32_service_state.pb.cc',
          ],
          'dependencies': [
            '../protobuf/protobuf.gyp:protobuf',
            'genproto_server',
          ],
          'export_dependent_settings': [
            'genproto_server',
          ],
        },
        {
          'target_name': 'gen_mozc_server_resource_header',
          'variables': {
            'gen_resource_proj_name': 'mozc_server',
            'gen_main_resource_path': 'server/mozc_server.rc',
            'gen_output_resource_path':
                '<(gen_out_dir)/mozc_server_autogen.rc',
          },
          'includes': [
            '../gyp/gen_win32_resource_header.gypi',
          ],
        },
        {
          'target_name': 'gen_mozc_cache_service_resource_header',
          'variables': {
            'gen_resource_proj_name': 'mozc_cache_service',
            'gen_main_resource_path': 'server/mozc_cache_service.rc',
            'gen_output_resource_path':
                '<(gen_out_dir)/mozc_cache_service_autogen.rc',
          },
          'includes': [
            '../gyp/gen_win32_resource_header.gypi',
          ],
        },
        {
          'target_name': 'cache_service_manager',
          'type': 'static_library',
          'sources': [
            'cache_service_manager.cc',
          ],
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:encryptor',
            'server_protocol',
          ],
        },
        {
          'target_name': 'mozc_cache_service',
          'product_name': '<(cache_service_product_name_win)',
          'type': 'executable',
          'includes': [
            '../gyp/postbuilds_win.gypi',
          ],
          'sources': [
            'mozc_cache_service.cc',
            'mozc_cache_service.exe.manifest',
            '<(gen_out_dir)/mozc_cache_service_autogen.rc',
          ],
          'dependencies': [
            'cache_service_manager',
            'gen_mozc_cache_service_resource_header',
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'AdditionalDependencies': [
                'crypt32.lib',  # used in 'mozc_cache_service.cc'
                'shlwapi.lib',
              ],
              'DelayLoadDLLs': [
                'shlwapi.dll',
              ],
            },
          },
        },
      ],
    }],
    ['OS=="mac"', {
      'targets': [
        {
          'target_name': 'gen_mozc_server_info_plist',
          'type': 'none',
          'actions': [
            {
              'action_name': 'generate',
              'inputs': [
                '../data/mac/mozc_server_info',
              ],
              'outputs': [
                '<(gen_out_dir)/mozc_server_info',
              ],
              'action': [
                'python', '../build_tools/tweak_info_plist.py',
                '--output', '<(gen_out_dir)/mozc_server_info',
                '--input', '../data/mac/mozc_server_info',
                '--version_file', '../mozc_version.txt',
                '--branding', '<(branding)',
              ],
            },
          ],
        },
      ],
    }],
  ],
}
