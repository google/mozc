# Copyright 2010-2013, Google Inc.
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
   'zinnia_sources': [
     '<(DEPTH)/third_party/zinnia/v0_04/character.cpp',
     '<(DEPTH)/third_party/zinnia/v0_04/param.cpp',
     '<(DEPTH)/third_party/zinnia/v0_04/svm.cpp',
     '<(DEPTH)/third_party/zinnia/v0_04/feature.cpp',
     '<(DEPTH)/third_party/zinnia/v0_04/recognizer.cpp',
     '<(DEPTH)/third_party/zinnia/v0_04/trainer.cpp',
     '<(DEPTH)/third_party/zinnia/v0_04/libzinnia.cpp',
     '<(DEPTH)/third_party/zinnia/v0_04/sexp.cpp',
    ],
  },
  'targets': [
    {
      'target_name': 'zinnia',
      'type': 'static_library',
      'cflags': [
        '-Wno-type-limits',
      ],
      'conditions': [
        ['OS=="linux"', {
          'conditions': [
            ['use_libzinnia==1', {
              'link_settings': {
                'libraries': [
                  ' <!@(<(pkg_config_command) --libs zinnia)',
                ],
              },
            }, {  # OS=="linux" and use_libzinnia==0
              'sources': ['<@(zinnia_sources)'],
              'defines': ['HAVE_CONFIG_H'],
            }],
          ],
        }],
        ['OS=="mac"', {
          'sources': ['<@(zinnia_sources)'],
          'defines': ['HAVE_CONFIG_H'],
          'xcode_settings': {
            'conditions': [
              ['clang==1', {
                'WARNING_CFLAGS+': [
                  '-Wno-tautological-compare',
                ],
              }],
            ],
          },
        }],
        ['clang==1', {
          'cflags+': [
            '-Wno-missing-field-initializers',
            '-Wno-tautological-compare',
          ],
        }],
        ['OS=="win"', {
          'sources': ['<@(zinnia_sources)'],
          'defines': [
             'VERSION="0.04"',
             'PACKAGE="zinnia"',
             'HAVE_WINDOWS_H',
          ],
          'msvs_disabled_warnings': [
            # destructor never returns, potential memory leak
            # http://msdn.microsoft.com/en-us/library/khwfyc5d.aspx
            '4722',  # Zinnia contains this kind of code
          ],
        }],
      ],
    },
  ],
}
