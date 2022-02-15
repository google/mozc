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
    # We accept following warnings come from protobuf.
    # This list should be revised when protobuf is updated.
    'msvc_disabled_warnings_for_protoc': [
      # switch statement contains 'default' but no 'case' labels.
      # https://msdn.microsoft.com/en-us/library/aa748818.aspx
      '4065',
      # unary minus operator applied to unsigned type, result still unsigned.
      # http://msdn.microsoft.com/en-us/library/4kh09110.aspx
      '4146',
      # 'this' : used in base member initializer list
      # http://msdn.microsoft.com/en-us/library/3c594ae3.aspx
      '4355',
      # no definition for inline function.
      # https://msdn.microsoft.com/en-us/library/aa733865.aspx
      '4506',
      # 'type' : forcing value to bool 'true' or 'false'
      # (performance warning)
      # http://msdn.microsoft.com/en-us/library/b6801kcy.aspx
      '4800',
    ],

    # We accept following warnings come from protobuf header files.
    # This list should be revised when protobuf is updated.
    'msvc_disabled_warnings_for_proto_headers': [
      # unary minus operator applied to unsigned type, result still unsigned.
      # http://msdn.microsoft.com/en-us/library/4kh09110.aspx
      '4146',
      # 'type' : forcing value to bool 'true' or 'false'
      # (performance warning)
      # http://msdn.microsoft.com/en-us/library/b6801kcy.aspx
      '4800',
    ],

    'protobuf_cpp_root': '<(protobuf_root)/src/google/protobuf',
    # Sources for Proto3.
    'protobuf_sources': [
      '<(protobuf_cpp_root)/any.cc',
      '<(protobuf_cpp_root)/any_lite.cc',
      '<(protobuf_cpp_root)/arena.cc',
      '<(protobuf_cpp_root)/arenastring.cc',
      '<(protobuf_cpp_root)/compiler/importer.cc',
      '<(protobuf_cpp_root)/compiler/parser.cc',
      '<(protobuf_cpp_root)/descriptor.cc',
      '<(protobuf_cpp_root)/descriptor.pb.cc',
      '<(protobuf_cpp_root)/descriptor_database.cc',
      '<(protobuf_cpp_root)/dynamic_message.cc',
      '<(protobuf_cpp_root)/empty.pb.cc',
      '<(protobuf_cpp_root)/extension_set.cc',
      '<(protobuf_cpp_root)/extension_set_heavy.cc',
      '<(protobuf_cpp_root)/generated_message_bases.cc',
      '<(protobuf_cpp_root)/generated_message_reflection.cc',
      '<(protobuf_cpp_root)/generated_message_util.cc',
      '<(protobuf_cpp_root)/implicit_weak_message.cc',
      '<(protobuf_cpp_root)/io/coded_stream.cc',
      '<(protobuf_cpp_root)/io/gzip_stream.cc',
      '<(protobuf_cpp_root)/io/io_win32.cc',
      '<(protobuf_cpp_root)/io/printer.cc',
      '<(protobuf_cpp_root)/io/strtod.cc',
      '<(protobuf_cpp_root)/io/tokenizer.cc',
      '<(protobuf_cpp_root)/io/zero_copy_stream.cc',
      '<(protobuf_cpp_root)/io/zero_copy_stream_impl.cc',
      '<(protobuf_cpp_root)/io/zero_copy_stream_impl_lite.cc',
      '<(protobuf_cpp_root)/map.cc',
      '<(protobuf_cpp_root)/map_field.cc',
      '<(protobuf_cpp_root)/message.cc',
      '<(protobuf_cpp_root)/message_lite.cc',
      '<(protobuf_cpp_root)/parse_context.cc',
      '<(protobuf_cpp_root)/reflection_ops.cc',
      '<(protobuf_cpp_root)/repeated_field.cc',
      '<(protobuf_cpp_root)/repeated_ptr_field.cc',
      '<(protobuf_cpp_root)/service.cc',
      '<(protobuf_cpp_root)/stubs/common.cc',
      '<(protobuf_cpp_root)/stubs/int128.cc',
      '<(protobuf_cpp_root)/stubs/status.cc',
      '<(protobuf_cpp_root)/stubs/stringpiece.cc',
      '<(protobuf_cpp_root)/stubs/stringprintf.cc',
      '<(protobuf_cpp_root)/stubs/structurally_valid.cc',
      '<(protobuf_cpp_root)/stubs/strutil.cc',
      '<(protobuf_cpp_root)/stubs/substitute.cc',
      '<(protobuf_cpp_root)/text_format.cc',
      '<(protobuf_cpp_root)/unknown_field_set.cc',
      '<(protobuf_cpp_root)/wire_format.cc',
      '<(protobuf_cpp_root)/wire_format_lite.cc',
    ],
    # Sources for protoc (common part and C++ generator only).
    'protoc_sources': [
      '<(protobuf_cpp_root)/any.cc',
      '<(protobuf_cpp_root)/any_lite.cc',
      '<(protobuf_cpp_root)/arena.cc',
      '<(protobuf_cpp_root)/arenastring.cc',
      '<(protobuf_cpp_root)/compiler/code_generator.cc',
      '<(protobuf_cpp_root)/compiler/command_line_interface.cc',
      '<(protobuf_cpp_root)/compiler/cpp/cpp_enum.cc',
      '<(protobuf_cpp_root)/compiler/cpp/cpp_enum_field.cc',
      '<(protobuf_cpp_root)/compiler/cpp/cpp_extension.cc',
      '<(protobuf_cpp_root)/compiler/cpp/cpp_field.cc',
      '<(protobuf_cpp_root)/compiler/cpp/cpp_file.cc',
      '<(protobuf_cpp_root)/compiler/cpp/cpp_generator.cc',
      '<(protobuf_cpp_root)/compiler/cpp/cpp_helpers.cc',
      '<(protobuf_cpp_root)/compiler/cpp/cpp_map_field.cc',
      '<(protobuf_cpp_root)/compiler/cpp/cpp_message.cc',
      '<(protobuf_cpp_root)/compiler/cpp/cpp_message_field.cc',
      '<(protobuf_cpp_root)/compiler/cpp/cpp_padding_optimizer.cc',
      '<(protobuf_cpp_root)/compiler/cpp/cpp_parse_function_generator.cc',
      '<(protobuf_cpp_root)/compiler/cpp/cpp_primitive_field.cc',
      '<(protobuf_cpp_root)/compiler/cpp/cpp_service.cc',
      '<(protobuf_cpp_root)/compiler/cpp/cpp_string_field.cc',
      '<(protobuf_cpp_root)/compiler/importer.cc',
      '<(protobuf_cpp_root)/compiler/parser.cc',
      '<(protobuf_cpp_root)/compiler/plugin.cc',
      '<(protobuf_cpp_root)/compiler/plugin.pb.cc',
      '<(protobuf_cpp_root)/compiler/subprocess.cc',
      '<(protobuf_cpp_root)/compiler/zip_writer.cc',
      '<(protobuf_cpp_root)/io/strtod.cc',
      '<(protobuf_cpp_root)/map.cc',
      '<(protobuf_cpp_root)/map_field.cc',
      'custom_protoc_main.cc',
    ],
  },
  'targets': [
    {
      'target_name': 'protobuf',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'all_dependent_settings': {
        'include_dirs': [
          '<(proto_out_dir)',  # make generated files (*.pb.h) visible.
        ],
      },
      'conditions': [
        ['use_libprotobuf==1', {
          'link_settings': {
            'libraries': [
              '-lprotobuf',
            ],
          },
        },
        {  # else
          'sources': ['<@(protobuf_sources)'],
          'include_dirs': [
            '<(protobuf_root)/src',
          ],
          'all_dependent_settings': {
            'include_dirs': [
              '<(protobuf_root)/src',
            ],
            'msvs_disabled_warnings': [
              '<@(msvc_disabled_warnings_for_proto_headers)',
            ],
          },
          'msvs_disabled_warnings': [
            '<@(msvc_disabled_warnings_for_protoc)',
          ],
          'xcode_settings': {
            'USE_HEADERMAP': 'NO',
          },
          'conditions': [
            ['(_toolset=="target" and (compiler_target=="clang" or compiler_target=="gcc")) or '
             '(_toolset=="host" and (compiler_host=="clang" or compiler_host=="gcc"))', {
              'cflags': [
                '-Wno-tautological-constant-out-of-range-compare',
                '-Wno-unused-const-variable',
                '-Wno-unused-function',
              ],
            }],
            ['OS=="win"', {
              'defines!': [
                'WIN32_LEAN_AND_MEAN',  # protobuf already defines this
              ],
            }],
            ['OS!="win"', {
              'defines': [
                'HAVE_PTHREAD',  # only needed in google/protobuf/stubs/common.cc for now.
              ],
            }],
          ],
        }],
      ],
    },
    {
      'target_name': 'protoc',
      'type': 'executable',
      'toolsets': ['host'],
      'dependencies': [
        'protobuf',
      ],
      'conditions': [
        ['use_libprotobuf==0', {
          'sources': ['<@(protoc_sources)'],
          'include_dirs': [
            '<(protobuf_root)/src',
          ],
          'msvs_disabled_warnings': [
            '<@(msvc_disabled_warnings_for_protoc)',
          ],
          'xcode_settings': {
            'USE_HEADERMAP': 'NO',
          },
          'conditions': [
            ['(_toolset=="target" and (compiler_target=="clang" or compiler_target=="gcc")) or '
             '(_toolset=="host" and (compiler_host=="clang" or compiler_host=="gcc"))', {
              'cflags': [
                '-Wno-tautological-constant-out-of-range-compare',
                '-Wno-unused-const-variable',
                '-Wno-unused-function',
                '-Wno-unused-private-field',
              ],
            }],
            ['OS=="win"', {
              'defines!': [
                'WIN32_LEAN_AND_MEAN',  # protobuf already defines this
              ],
            }],
          ],
        }],
      ],
    },
  ],
}
