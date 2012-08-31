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
    'relative_dir': 'dictionary',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  'targets': [
    {
      'target_name': 'suffix_dictionary',
      'type': 'static_library',
      'sources': [
        'suffix_dictionary.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
    },
    {
      'target_name': 'dictionary',
      'type': 'none',
      'dependencies': [
        '../base/base.gyp:base',
        '<(DEPTH)/third_party/rx/rx.gyp:rx',
        'dictionary_base.gyp:dictionary_protocol',
        'dictionary_base.gyp:gen_pos_matcher#host',
        'dictionary_base.gyp:suppression_dictionary',
        'dictionary_base.gyp:user_dictionary',
        'dictionary_impl',
        'suffix_dictionary',
        'system/system_dictionary.gyp:system_dictionary',
        'system/system_dictionary.gyp:value_dictionary',
      ],
    },
    {
      'target_name': 'dictionary_impl',
      'type': 'static_library',
      'sources': [
        'dictionary_impl.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'dictionary_base.gyp:dictionary_protocol',
        'dictionary_base.gyp:suppression_dictionary',
      ],
    },
    {
      'target_name': 'gen_system_dictionary_data_main',
      'type': 'executable',
      'toolsets': ['host'],
      'sources': [
        'gen_system_dictionary_data_main.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'dictionary_base.gyp:gen_pos_matcher',
        'system/system_dictionary.gyp:system_dictionary_builder',
      ],
    },
    {
      'target_name': 'install_gen_system_dictionary_data_main',
      'type': 'none',
      'toolsets': ['host'],
      'variables': {
        'bin_name': 'gen_system_dictionary_data_main'
      },
      'includes' : [
        '../gyp/install_build_tool.gypi'
      ],
    },
    {
      'target_name': 'dictionary_mock',
      'type': 'static_library',
      'sources': [
        'dictionary_mock.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '<(DEPTH)/third_party/rx/rx.gyp:rx',
      ],
    },
  ],
}
