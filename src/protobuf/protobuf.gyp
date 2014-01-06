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
    # We accept following warnings come from protobuf.
    # This list should be revised when protobuf is updated.
    'msvc_disabled_warnings_for_protoc': [
      # unary minus operator applied to unsigned type, result still
      # unsigned.
      # http://msdn.microsoft.com/en-us/library/4kh09110.aspx
      '4146',
      # 'this' : used in base member initializer list
      # http://msdn.microsoft.com/en-us/library/3c594ae3.aspx
      '4355',
    ],
    'protobuf_cpp_root' : '<(protobuf_root)/src/google/protobuf',
    'protobuf_sources': [
      '<(protobuf_cpp_root)/compiler/importer.cc',
      '<(protobuf_cpp_root)/compiler/parser.cc',
      '<(protobuf_cpp_root)/descriptor.cc',
      '<(protobuf_cpp_root)/descriptor.pb.cc',
      '<(protobuf_cpp_root)/descriptor_database.cc',
      '<(protobuf_cpp_root)/dynamic_message.cc',
      '<(protobuf_cpp_root)/extension_set.cc',
      '<(protobuf_cpp_root)/extension_set_heavy.cc',
      '<(protobuf_cpp_root)/generated_message_reflection.cc',
      '<(protobuf_cpp_root)/generated_message_util.cc',
      '<(protobuf_cpp_root)/io/coded_stream.cc',
      '<(protobuf_cpp_root)/io/gzip_stream.cc',
      '<(protobuf_cpp_root)/io/printer.cc',
      '<(protobuf_cpp_root)/io/tokenizer.cc',
      '<(protobuf_cpp_root)/io/zero_copy_stream.cc',
      '<(protobuf_cpp_root)/io/zero_copy_stream_impl.cc',
      '<(protobuf_cpp_root)/io/zero_copy_stream_impl_lite.cc',
      '<(protobuf_cpp_root)/message.cc',
      '<(protobuf_cpp_root)/message_lite.cc',
      '<(protobuf_cpp_root)/reflection_ops.cc',
      '<(protobuf_cpp_root)/repeated_field.cc',
      '<(protobuf_cpp_root)/service.cc',
      '<(protobuf_cpp_root)/stubs/atomicops_internals_x86_gcc.cc',
      '<(protobuf_cpp_root)/stubs/atomicops_internals_x86_msvc.cc',
      '<(protobuf_cpp_root)/stubs/common.cc',
      '<(protobuf_cpp_root)/stubs/once.cc',
      '<(protobuf_cpp_root)/stubs/stringprintf.cc',
      '<(protobuf_cpp_root)/stubs/structurally_valid.cc',
      '<(protobuf_cpp_root)/stubs/strutil.cc',
      '<(protobuf_cpp_root)/stubs/substitute.cc',
      '<(protobuf_cpp_root)/text_format.cc',
      '<(protobuf_cpp_root)/unknown_field_set.cc',
      '<(protobuf_cpp_root)/wire_format.cc',
      '<(protobuf_cpp_root)/wire_format_lite.cc',
    ],
    'protoc_sources': [
      '<(protobuf_cpp_root)/compiler/code_generator.cc',
      '<(protobuf_cpp_root)/compiler/command_line_interface.cc',
      '<(protobuf_cpp_root)/compiler/cpp/cpp_enum.cc',
      '<(protobuf_cpp_root)/compiler/cpp/cpp_enum_field.cc',
      '<(protobuf_cpp_root)/compiler/cpp/cpp_extension.cc',
      '<(protobuf_cpp_root)/compiler/cpp/cpp_field.cc',
      '<(protobuf_cpp_root)/compiler/cpp/cpp_file.cc',
      '<(protobuf_cpp_root)/compiler/cpp/cpp_generator.cc',
      '<(protobuf_cpp_root)/compiler/cpp/cpp_helpers.cc',
      '<(protobuf_cpp_root)/compiler/cpp/cpp_message.cc',
      '<(protobuf_cpp_root)/compiler/cpp/cpp_message_field.cc',
      '<(protobuf_cpp_root)/compiler/cpp/cpp_primitive_field.cc',
      '<(protobuf_cpp_root)/compiler/cpp/cpp_service.cc',
      '<(protobuf_cpp_root)/compiler/cpp/cpp_string_field.cc',
      '<(protobuf_cpp_root)/compiler/java/java_doc_comment.cc',
      '<(protobuf_cpp_root)/compiler/java/java_enum.cc',
      '<(protobuf_cpp_root)/compiler/java/java_enum_field.cc',
      '<(protobuf_cpp_root)/compiler/java/java_extension.cc',
      '<(protobuf_cpp_root)/compiler/java/java_field.cc',
      '<(protobuf_cpp_root)/compiler/java/java_file.cc',
      '<(protobuf_cpp_root)/compiler/java/java_generator.cc',
      '<(protobuf_cpp_root)/compiler/java/java_helpers.cc',
      '<(protobuf_cpp_root)/compiler/java/java_message.cc',
      '<(protobuf_cpp_root)/compiler/java/java_message_field.cc',
      '<(protobuf_cpp_root)/compiler/java/java_primitive_field.cc',
      '<(protobuf_cpp_root)/compiler/java/java_service.cc',
      '<(protobuf_cpp_root)/compiler/java/java_string_field.cc',
      '<(protobuf_cpp_root)/compiler/main.cc',
      '<(protobuf_cpp_root)/compiler/plugin.cc',
      '<(protobuf_cpp_root)/compiler/plugin.pb.cc',
      '<(protobuf_cpp_root)/compiler/python/python_generator.cc',
      '<(protobuf_cpp_root)/compiler/subprocess.cc',
      '<(protobuf_cpp_root)/compiler/zip_writer.cc',
      '<(protobuf_cpp_root)/stubs/stringprintf.cc',
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
        'conditions': [
          ['target_platform=="Android" and _toolset=="target"', {
            'defines': [
              'GOOGLE_PROTOBUF_NO_RTTI'
            ],
          }],
        ],
      },
      'conditions': [
        ['use_libprotobuf==1', {
          'link_settings': {
            'libraries': [
              '-lprotobuf',
            ],
          },
        }, {  # else
          'sources': ['<@(protobuf_sources)'],
          'include_dirs': [
            '.',  # for config.h
            '<(protobuf_root)/src',
          ],
          'all_dependent_settings': {
            'include_dirs': [
              '<(protobuf_root)/src',
            ],
          },
          'xcode_settings': {
            'WARNING_CFLAGS': [
              '-Wno-error',
              # For GetEnumNumber in wire_format.cc:60.
              '-Wno-unused-function',
            ],
          },
          'msvs_disabled_warnings': [
            '<@(msvc_disabled_warnings_for_protoc)',
          ],
          'conditions': [
            # for gcc and clang
            ['OS=="linux" or OS=="mac"', {
              'cflags': [
                '-Wno-conversion-null',  # coded_stream.cc uses NULL to bool.
                '-Wno-unused-function',
              ],
            }],
            ['OS=="win"', {
              'defines!': [
                'WIN32_LEAN_AND_MEAN',  # protobuf already defines this
              ],
            }],
          ],
        }],
        ['target_platform=="Android" and _toolset=="target"', {
          'defines': [
            'GOOGLE_PROTOBUF_NO_RTTI'
          ],
          'dependencies': [
            '../android/android_base.gyp:android_pthread_once',
          ],
        }],
        ['use_packed_dictionary==1', {
          'dependencies': [
            'zlib'
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
      'include_dirs': [
        '.',  # for config.h
        '<(protobuf_root)/src',
      ],
      'conditions': [
        ['OS=="linux"', {
          'conditions': [
            ['use_libprotobuf!=1', {
              'cflags': [
                '-Wno-unused-result',  # protoc has unused result.
              ],
              'sources': ['<@(protoc_sources)'],
            }],
          ],
        }],
        ['OS=="mac"', {
          'sources': ['<@(protoc_sources)'],
          'xcode_settings': {
            'WARNING_CFLAGS': ['-Wno-error'],
          },
        }],
        ['OS=="win"', {
          'sources': ['<@(protoc_sources)'],
          'defines!': [
            'WIN32_LEAN_AND_MEAN',  # protobuf already defines this
          ],
          'msvs_disabled_warnings': [
            '<@(msvc_disabled_warnings_for_protoc)',
          ],
        }],
      ],
    },
    {
      'target_name': 'install_protoc',
      'type': 'none',
      'toolsets': ['host'],
      'variables': {
        'bin_name': 'protoc',
      },
      'conditions': [
        # use system-installed protoc on Linux
        ['OS!="linux"', {
          'includes' : [
            '../gyp/install_build_tool.gypi',
          ],
        }, {  # OS=="linux"
          'conditions': [
            ['use_libprotobuf==0', {
              'includes' : [
                '../gyp/install_build_tool.gypi',
              ],
            }],
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['use_packed_dictionary==1', {
      'targets': [
        {
          'target_name': 'zlib',
          'type': 'static_library',
          'toolsets': ['host', 'target'],
          'variables': {
            'zlib_root': '<(third_party_dir)/zlib/v1_2_8',
          },
          'sources': [
            '<(zlib_root)/adler32.c',
            '<(zlib_root)/compress.c',
            '<(zlib_root)/crc32.c',
            '<(zlib_root)/crc32.h',
            '<(zlib_root)/deflate.c',
            '<(zlib_root)/deflate.h',
            '<(zlib_root)/gzclose.c',
            '<(zlib_root)/gzguts.h',
            '<(zlib_root)/gzlib.c',
            '<(zlib_root)/gzread.c',
            '<(zlib_root)/gzwrite.c',
            '<(zlib_root)/infback.c',
            '<(zlib_root)/inffast.c',
            '<(zlib_root)/inffast.h',
            '<(zlib_root)/inffixed.h',
            '<(zlib_root)/inflate.c',
            '<(zlib_root)/inflate.h',
            '<(zlib_root)/inftrees.c',
            '<(zlib_root)/inftrees.h',
            '<(zlib_root)/trees.c',
            '<(zlib_root)/trees.h',
            '<(zlib_root)/uncompr.c',
            '<(zlib_root)/zconf.h',
            '<(zlib_root)/zlib.h',
            '<(zlib_root)/zutil.c',
            '<(zlib_root)/zutil.h',
          ],
          'all_dependent_settings': {
            'include_dirs': [
              '<(zlib_root)',
            ],
          },
          'cflags': [
            '-Wno-error=uninitialized',
            '-Wno-implicit-function-declaration',
            '-Wno-uninitialized',
            '-Wno-unused-value',
          ],
          'conditions': [
            ['clang==1', {
              'defines!': [
                'DEBUG',
              ],
            }],
          ],
        },
      ],
    }],
  ],
}
