# Copyright 2010-2018, Google Inc.
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
  'targets': [
    {
      'target_name': 'zinnia',
      'conditions': [
        ['use_libzinnia==1', {
          'type': 'none',
          'link_settings': {
            'libraries': [
              '<!@(pkg-config --libs zinnia)',
            ],
          },
        }, {  # use_libzinnia==0
          'type': 'static_library',
          'variables': {
            'zinnia_src_dir': '<(third_party_dir)/zinnia/zinnia',
          },
          'sources': [
            '<(zinnia_src_dir)/character.cpp',
            '<(zinnia_src_dir)/param.cpp',
            '<(zinnia_src_dir)/feature.cpp',
            '<(zinnia_src_dir)/recognizer.cpp',
            '<(zinnia_src_dir)/trainer.cpp',
            '<(zinnia_src_dir)/libzinnia.cpp',
            '<(zinnia_src_dir)/sexp.cpp',
          ],
          'include_dirs': [
            # So that dependent file can look up <zinnia.h>
            '<(zinnia_src_dir)',
          ],
          'all_dependent_settings': {
            'include_dirs': [
              # So that dependent file can look up <zinnia.h>
              '<(zinnia_src_dir)',
            ],
            'conditions': [
              ['target_platform=="Windows"', {
                'defines': [
                  'ZINNIA_STATIC_LIBRARY',
                ],
              }],
            ],
          },
          'cflags': [
            '-Wno-type-limits',
          ],
          'msvs_disabled_warnings': [
            # destructor never returns, potential memory leak
            # http://msdn.microsoft.com/en-us/library/khwfyc5d.aspx
            '4722',  # Zinnia contains this kind of code
          ],
          'conditions': [
            ['target_platform=="Windows"', {
              'defines': [
                'HAVE_WINDOWS_H=1',
                'PACKAGE="zinnia"',
                'VERSION="0.06"',
                'ZINNIA_STATIC_LIBRARY',
              ],
            }],
            ['target_platform=="Linux" or target_platform=="Mac"', {
              'defines': [
                'HAVE_CONFIG_H=1'
              ],
            }],
            ['(_toolset=="target" and compiler_target=="clang") or '
             '(_toolset=="host" and compiler_host=="clang")', {
               'cflags': [
                 '-Wno-missing-field-initializers',
                 '-Wno-tautological-compare',
               ],
            }],
          ],
        }],
      ],
    },
  ],
}
