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
    'relative_dir': 'ipc',
  },
  'targets': [
    {
      'target_name': 'ipc',
      'type': 'static_library',
      'sources': [
        'ipc.cc',
        'ipc_mock.cc',
        'ipc_path_manager.cc',
        'mach_ipc.cc',
        'named_event.cc',
        'process_watch_dog.cc',
        'unix_ipc.cc',
        'win32_ipc.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'ipc_protocol',
      ],
    },
    {
      'target_name': 'ipc_protocol',
      'type': 'static_library',
      'hard_dependency': 1,
      'sources': [
        '<(proto_out_dir)/<(relative_dir)/ipc.pb.cc',
      ],
      'dependencies': [
        '../protobuf/protobuf.gyp:protobuf',
        'genproto_ipc#host',
      ],
      'export_dependent_settings': [
        'genproto_ipc#host',
      ],
    },
    {
      'target_name': 'genproto_ipc',
      'type': 'none',
      'toolsets': ['host'],
      'sources': [
        'ipc.proto',
      ],
      'includes': [
        '../protobuf/genproto.gypi',
      ],
    },
    {
      'target_name': 'window_info_protocol',
      'type': 'static_library',
      'hard_dependency': 1,
      'sources': [
        '<(proto_out_dir)/<(relative_dir)/window_info.pb.cc',
      ],
      'dependencies': [
        '../protobuf/protobuf.gyp:protobuf',
        'genproto_window_info#host',
      ],
      'export_dependent_settings': [
        'genproto_window_info#host',
      ],
    },
    {
      'target_name': 'genproto_window_info',
      'type': 'none',
      'toolsets': ['host'],
      'sources': [
        'window_info.proto',
      ],
      'includes': [
        '../protobuf/genproto.gypi',
      ],
    },
    {
      'target_name': 'ipc_test_util',
      'type': 'static_library',
      'sources': [
        'ipc_test_util.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'ipc',
      ],
    },
    {
      'target_name': 'ipc_test',
      'type': 'executable',
      'sources': [
        'ipc_path_manager_test.cc',
        'ipc_test.cc',
        'named_event_test.cc',
        'process_watch_dog_test.cc',
      ],
      'dependencies': [
        '../testing/testing.gyp:gtest_main',
        'ipc',
        'ipc_test_util',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    # Test cases meta target: this target is referred from gyp/tests.gyp
    {
      'target_name': 'ipc_all_test',
      'type': 'none',
      'dependencies': [
        'ipc_test',
      ],
    },
  ],
}
