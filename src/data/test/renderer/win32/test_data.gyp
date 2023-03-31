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
    'relative_dir': 'data/test/renderer/win32',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'genproto_test_spec_proto',
          'type': 'none',
          'toolsets': ['host'],
          'sources': [
            'test_spec.proto',
          ],
          'includes': [
            '../../../../protobuf/genproto.gypi',
          ],
        },
        {
          'target_name': 'test_spec_proto',
          'type': 'static_library',
          'hard_dependency': 1,
          'sources': [
            '<(proto_out_dir)/<(relative_dir)/test_spec.pb.cc',
          ],
          'dependencies': [
            '../../../../protobuf/protobuf.gyp:protobuf',
            'genproto_test_spec_proto#host',
          ],
          'export_dependent_settings': [
            'genproto_test_spec_proto#host',
          ],
        },
        {
          'target_name': 'install_test_data',
          'type': 'none',
          'variables': {
            'test_data': [
              'balloon_blur_alpha_-1.png',
              'balloon_blur_alpha_-1.png.textproto',
              'balloon_blur_alpha_0.png',
              'balloon_blur_alpha_0.png.textproto',
              'balloon_blur_alpha_10.png',
              'balloon_blur_alpha_10.png.textproto',
              'balloon_blur_color_32_64_128.png',
              'balloon_blur_color_32_64_128.png.textproto',
              'balloon_blur_offset_-20_-10.png',
              'balloon_blur_offset_-20_-10.png.textproto',
              'balloon_blur_offset_0_0.png',
              'balloon_blur_offset_0_0.png.textproto',
              'balloon_blur_offset_20_5.png',
              'balloon_blur_offset_20_5.png.textproto',
              'balloon_blur_sigma_0.0.png',
              'balloon_blur_sigma_0.0.png.textproto',
              'balloon_blur_sigma_0.5.png',
              'balloon_blur_sigma_0.5.png.textproto',
              'balloon_blur_sigma_1.0.png',
              'balloon_blur_sigma_1.0.png.textproto',
              'balloon_blur_sigma_2.0.png',
              'balloon_blur_sigma_2.0.png.textproto',
              'balloon_frame_thickness_-1.png',
              'balloon_frame_thickness_-1.png.textproto',
              'balloon_frame_thickness_0.png',
              'balloon_frame_thickness_0.png.textproto',
              'balloon_frame_thickness_1.5.png',
              'balloon_frame_thickness_1.5.png.textproto',
              'balloon_frame_thickness_3.png',
              'balloon_frame_thickness_3.png.textproto',
              'balloon_inside_color_32_64_128.png',
              'balloon_inside_color_32_64_128.png.textproto',
              'balloon_no_label.png',
              'balloon_no_label.png.textproto',
              'balloon_tail_bottom.png',
              'balloon_tail_bottom.png.textproto',
              'balloon_tail_left.png',
              'balloon_tail_left.png.textproto',
              'balloon_tail_right.png',
              'balloon_tail_right.png.textproto',
              'balloon_tail_top.png',
              'balloon_tail_top.png.textproto',
              'balloon_tail_width_height_-10_-10.png',
              'balloon_tail_width_height_-10_-10.png.textproto',
              'balloon_tail_width_height_0_0.png',
              'balloon_tail_width_height_0_0.png.textproto',
              'balloon_tail_width_height_10_20.png',
              'balloon_tail_width_height_10_20.png.textproto',
              'balloon_width_height_40_30.png',
              'balloon_width_height_40_30.png.textproto',
            ],
            'test_data_subdir': 'data/test/renderer/win32',
          },
          'includes': ['../../../../gyp/install_testdata.gypi'],
        },
      ],
    }],
  ],
}
