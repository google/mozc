# Copyright 2010-2014, Google Inc.
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
    'relative_dir': 'data_manager/packed',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  'targets': [
    {
      'target_name': 'packed_data_manager',
      'type': 'static_library',
      'toolsets': [ 'host', 'target' ],
      'sources': [
        'packed_data_manager.cc',
        'system_dictionary_format_version.h',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        'system_dictionary_data_protocol',
      ],
    },
    {
      'target_name': 'system_dictionary_data_protocol',
      'type': 'static_library',
      'toolsets': [ 'host', 'target' ],
      'hard_dependency': 1,
      'sources': [
        '<(proto_out_dir)/<(relative_dir)/system_dictionary_data.pb.cc',
      ],
      'dependencies': [
        '../../protobuf/protobuf.gyp:protobuf',
        'genproto_system_dictionary_data#host',
      ],
      'export_dependent_settings': [
        'genproto_system_dictionary_data#host',
      ],
    },
    {
      'target_name': 'genproto_system_dictionary_data',
      'type': 'none',
      'toolsets': ['host'],
      'sources': [
        'system_dictionary_data.proto',
      ],
      'includes': [
        '../../protobuf/genproto.gypi',
      ],
    },
    {
      'target_name': 'system_dictionary_data_packer',
      'type': 'static_library',
      'toolsets': [ 'host', 'target' ],
      'sources': [
        'system_dictionary_data_packer.cc',
        'system_dictionary_format_version.h',
      ],
      'dependencies': [
        'system_dictionary_data_protocol',
        '../../base/base.gyp:base',
        '../../dictionary/dictionary_base.gyp:pos_matcher',
      ],
    },
  ],
  'conditions': [
    ['use_packed_dictionary==1', {
      'variables': {
        'dataset_dir': 'testing',
        'dataset_tag': 'mock',
      },
      'includes': [ 'packed_data_manager_base.gypi' ],
    }],
    ['branding=="Mozc"', {
      'variables': {
        'dataset_dir': 'oss',
        'dataset_tag': 'oss',
      },
      'includes': [ 'packed_data_manager_base.gypi' ],
    }],
  ],
}
