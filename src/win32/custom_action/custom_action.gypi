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
  'type': 'shared_library',
  'sources': [
    'custom_action.cc',
    '<(gen_out_dir)/custom_action_autogen.rc',
  ],
  'dependencies': [
    '../../base/base.gyp:base',
    '../../client/client.gyp:client',
    '../../config/config.gyp:stats_config_util',
    '../../languages/japanese/japanese.gyp:language_dependent_spec_japanese',
    '../../languages/languages.gyp:global_language_spec',
    '../../renderer/renderer.gyp:renderer',
    '../../server/server.gyp:cache_service_manager',
    '../base/win32_base.gyp:ime_base',
    'gen_custom_action_resource_header',
  ],
  'msvs_settings': {
    'VCLinkerTool': {
      'ModuleDefinitionFile': '<(gen_out_dir)/custom_actions.def',
    },
  },
  'actions': [
    {
      'action_name': 'gen_customaction_deffile',
      'variables': {
        'input_file': 'custom_action_template.def',
        'output_file': '<(gen_out_dir)/custom_actions.def',
      },
      'inputs': [
        '<(input_file)',
      ],
      'outputs': [
        '<(output_file)',
      ],
      'action': [
        'python', '../../build_tools/redirect.py',
        '<(output_file)',
        'cl.exe',
        '/EP',
        '/TP',
        '/nologo',
        '/DINTERNALONLY_Failure__=',
        '/DINTERNALONLY_DisableErrorReporting__=',
        '<(input_file)',
      ],
      'message': 'Generating deffile from <(input_file)',
    },
  ],
}
