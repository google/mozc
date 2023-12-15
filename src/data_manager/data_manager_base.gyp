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
  'variables': {
    'relative_dir': 'data_manager',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  'targets': [
    {
      'target_name': 'data_manager',
      'type': 'static_library',
      'toolsets': [ 'target', 'host' ],
      'sources': [
        'data_manager.cc',
      ],
      'dependencies': [
        '<(mozc_src_dir)/base/absl.gyp:absl_status',
        '<(mozc_src_dir)/base/absl.gyp:absl_strings',
        '<(mozc_src_dir)/base/base.gyp:base',
        '<(mozc_src_dir)/base/base.gyp:serialized_string_array',
        '<(mozc_src_dir)/base/base.gyp:version',
        '<(mozc_src_dir)/protocol/protocol.gyp:segmenter_data_proto',
        'dataset_reader',
        'serialized_dictionary',
      ],
    },
    {
      'target_name': 'genproto_dataset_proto',
      'type': 'none',
      'toolsets': ['host'],
      'sources': [
        'dataset.proto',
      ],
      'includes': [
        '../protobuf/genproto.gypi',
      ],
    },
    {
      'target_name': 'dataset_proto',
      'type': 'static_library',
      'toolsets': [ 'target', 'host' ],
      'hard_dependency': 1,
      'sources': [
        '<(proto_out_dir)/<(relative_dir)/dataset.pb.cc',
      ],
      'dependencies': [
        '<(mozc_src_dir)/protobuf/protobuf.gyp:protobuf',
        'genproto_dataset_proto#host',
      ],
      'export_dependent_settings': [
        'genproto_dataset_proto#host',
      ],
    },
    {
      'target_name': 'dataset_writer',
      'type': 'static_library',
      'toolsets': [ 'target', 'host' ],
      'sources': [
        'dataset_writer.cc',
      ],
      'dependencies': [
        '<(mozc_src_dir)/base/absl.gyp:absl_strings',
        '<(mozc_src_dir)/base/base.gyp:base',
        '<(mozc_src_dir)/base/base.gyp:obfuscator_support',
        'dataset_proto',
      ],
    },
    {
      'target_name': 'dataset_reader',
      'type': 'static_library',
      'toolsets': [ 'target', 'host' ],
      'sources': [
        'dataset_reader.cc',
      ],
      'dependencies': [
        '<(mozc_src_dir)/base/absl.gyp:absl_strings',
        '<(mozc_src_dir)/base/base.gyp:base',
        '<(mozc_src_dir)/base/base.gyp:obfuscator_support',
        'dataset_proto',
      ],
    },
    {
      'target_name': 'dataset_writer_main',
      'type': 'executable',
      'toolsets': [ 'host' ],
      'sources': [
        'dataset_writer_main.cc',
      ],
      'dependencies': [
        '<(mozc_src_dir)/base/base.gyp:base',
        '<(mozc_src_dir)/base/base.gyp:base_core',
        '<(mozc_src_dir)/base/base.gyp:number_util',
        'dataset_proto',
        'dataset_writer',
      ],
    },
    {
      'target_name': 'serialized_dictionary',
      'type': 'static_library',
      'toolsets': ['target', 'host'],
      'sources': [
        'serialized_dictionary.cc',
      ],
      'dependencies': [
        '<(mozc_src_dir)/base/absl.gyp:absl_strings',
        '<(mozc_src_dir)/base/base.gyp:base',
        '<(mozc_src_dir)/base/base.gyp:number_util',
        '<(mozc_src_dir)/base/base.gyp:serialized_string_array',
      ],
    },
  ],
}
