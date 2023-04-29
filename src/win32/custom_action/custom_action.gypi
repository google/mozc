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

{
  'type': 'shared_library',
  'sources': [
    'custom_action.cc',
    'custom_action.def',
    '<(gen_out_dir)/custom_action_autogen.rc',
  ],
  'dependencies': [
    '../../base/base.gyp:base',
    '../../base/base.gyp:url',
    '../../client/client.gyp:client',
    '../../config/config.gyp:stats_config_util',
    '../../renderer/renderer.gyp:renderer_client',
    '../base/win32_base.gyp:ime_base',
    '../base/win32_base.gyp:imframework_util',
    '../base/win32_base.gyp:input_dll_import_lib',
    '../cache_service/cache_service.gyp:cache_service_manager',
    'gen_custom_action_resource_header',
  ],
  'msvs_settings': {
    'VCLinkerTool': {
      'AdditionalDependencies': [
        'crypt32.lib',  # used in 'custom_action.cc'
        'msi.lib',      # used in 'custom_action.cc'
      ],
    },
  },
}
