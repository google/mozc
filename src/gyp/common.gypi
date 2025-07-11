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

# IMPORTANT:
# Please don't directly include this file since the gypi files is
# automatically included.
# You may find cool techniques in the following *.gypi file.
# http://src.chromium.org/viewvc/chrome/trunk/src/build/common.gypi
{
  'includes': ['defines.gypi', 'directories.gypi'],
  'variables': {
    # Compiler to build binaries that run in the target environment.
    # e.g. "clang", "gcc", "msvs".
    'compiler_target%': '',

    # Compiler to build binaries that run in the host environment.
    # e.g. "clang", "gcc", "msvs".
    'compiler_host%': '',

    # Versioning stuff for Mac.
    'mac_sdk%': '15.0',
    'mac_deployment_target%': '12.0',

    # warning_cflags will be shared with Mac and Linux.
    'warning_cflags': [
      '-Wall',
      '-Wno-char-subscripts',
      '-Wno-sign-compare',
      '-Wno-deprecated-declarations',
      '-Wwrite-strings',

      '-Wno-unknown-warning-option',
      '-Wno-inconsistent-missing-override',
    ],

    # gcc_cflags will be shared with Mac and Linux
    'gcc_cflags': [
      '-fmessage-length=0',
      '-fno-strict-aliasing',
      '-funsigned-char',
      '-pipe',
      '-pthread',
    ],
    # linux_cflags will be used in Linux.
    'linux_cflags': [
      '<@(gcc_cflags)',
      '-fno-omit-frame-pointer',
      '-fstack-protector',
      '--param=ssp-buffer-size=4',
    ],
    # mac_cflags will be used in Mac.
    # Xcode 4.5 which we are currently using does not support ssp-buffer-size.
    # TODO(horo): When we can use Xcode 4.6 which supports ssp-buffer-size,
    # set ssp-buffer-size in Mac.
    'mac_cflags': [
      '<@(gcc_cflags)',
      '-fno-omit-frame-pointer',
      '-fstack-protector',
      '-fobjc-arc',
    ],
    # Libraries for GNU/Linux environment.
    'linux_ldflags': [
      '-pthread',
    ],

    'conditions': [
      ['OS=="mac"', {
        'compiler_target': 'clang',
        'compiler_host': 'clang',
      }],
      ['target_platform=="Linux"', {
        # In most Linux distributions, system-provided Qt6 libraries are
        # supposed to be built with libstdc++ rather than libc++.  This means
        # that mozc_tool also need to link to libstdc++ to avoid ABI mismatch
        # unless we build Qt6 from the source code like we do so in macOS and
        # Windows builds.  For now, let's assume GCC and libstdc++ in Linux CI.
        'compiler_target': 'gcc',
        'compiler_host': 'gcc',
      }],
    ],
  },
  'target_defaults': {
    'variables': {
      # See http://gcc.gnu.org/onlinedocs/gcc-4.4.2/gcc/Optimize-Options.html
      'mac_release_optimization%': '2',  # Use -O2 unless overridden
      'mac_debug_optimization%': '0',    # Use -O0 unless overridden

      # Extra cflags for gcc
      'release_extra_cflags%': ['-O2'],
      'debug_extra_cflags%': ['-O0', '-g'],
    },
    'configurations': {
      'Debug': {
        'defines': [
          'DEBUG',
        ],
        'xcode_settings': {
          'COPY_PHASE_STRIP': 'NO',
          'GCC_OPTIMIZATION_LEVEL': '<(mac_debug_optimization)',
          'GCC_INLINES_ARE_PRIVATE_EXTERN': 'YES',
          'OTHER_CFLAGS': [ '<@(debug_extra_cflags)', ],
        },
        'conditions': [
          ['OS=="linux"', {
            'cflags': [
              '<@(debug_extra_cflags)',
            ],
          }],
        ],
      },
      'Release': {
        'defines': [
          'NDEBUG',
          'QT_NO_DEBUG',
          'ABSL_MIN_LOG_LEVEL=100',
          'IGNORE_HELP_FLAG',
          'IGNORE_INVALID_FLAG'
        ],
        'xcode_settings': {
          'DEAD_CODE_STRIPPING': 'YES',  # -Wl,-dead_strip
          'GCC_OPTIMIZATION_LEVEL': '<(mac_release_optimization)',
          'OTHER_CFLAGS': [ '<@(release_extra_cflags)', ],
        },
        'conditions': [
          ['OS=="linux"', {
            'cflags': [
              '<@(release_extra_cflags)',
            ],
          }],
        ],
      },
    },
    'default_configuration': 'Debug',
    'include_dirs': [
      '<@(absl_include_dirs)',
      '<(abs_depth)',
      '<(SHARED_INTERMEDIATE_DIR)',
    ],
    'mac_framework_headers': [],
    'target_conditions': [
      ['_toolset=="target"', {
        'conditions': [
          ['compiler_target=="clang"', {
            'cflags': [
              '-Wtype-limits',
            ],
            'cflags_cc': [
              '-stdlib=libc++',
              '-Wno-covered-switch-default',
              '-Wno-unnamed-type-template-args',
              '-Wno-c++11-narrowing',
            ],
          }],
          ['compiler_target=="clang" or compiler_target=="gcc"', {
            'cflags_cc': [
              '-std=c++20',
            ],
          }],
        ],
      }],
      ['_toolset=="host"', {
        'conditions': [
          ['compiler_host=="clang"', {
            'cflags': [
              '-Wtype-limits',
            ],
            'cflags_cc': [
              '-stdlib=libc++',
              '-Wno-covered-switch-default',
              '-Wno-unnamed-type-template-args',
              '-Wno-c++11-narrowing',
            ],
          }],
          ['compiler_host=="clang" or compiler_host=="gcc"', {
            'cflags_cc': [
              '-std=c++20',
            ],
          }],
        ],
      }],
    ],
    'conditions': [
      ['OS=="linux"', {
        'ldflags': [
          '<@(linux_ldflags)',
        ],
        'cflags': [
          '<@(linux_cflags)',
          '<@(warning_cflags)',
          '-fPIC',
          '-fno-exceptions',
        ],
        'cflags_cc': [
          # We use deprecated <hash_map> and <hash_set> instead of upcoming
          # <unordered_map> and <unordered_set>.
          '-Wno-deprecated',
        ],
      }],
      ['OS=="mac"', {
        'make_global_settings': [
          ['CC', '/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang'],
          ['CXX', '/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++'],
          ['LINK', '/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++'],
          ['LDPLUSPLUS', '/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++'],
        ],
        'conditions': [
          ['target_platform=="Mac"', {
            'xcode_settings': {
              'ARCHS': ['x86_64'],
              'MACOSX_DEPLOYMENT_TARGET': '<(mac_deployment_target)',
            },
          }],
        ],
        'xcode_settings': {
          'SDKROOT': 'macosx<(mac_sdk)',
          'GCC_ENABLE_CPP_EXCEPTIONS': 'NO',  # -fno-exceptions
          'GCC_SYMBOLS_PRIVATE_EXTERN': 'NO',  # No -fvisibility=hidden
          'OTHER_CFLAGS': [
            '<@(mac_cflags)',
          ],
          'WARNING_CFLAGS': ['<@(warning_cflags)'],
          'PYTHONPATH': '<(abs_depth)/',
          'CLANG_WARN_CXX0X_EXTENSIONS': 'NO',
          'GCC_VERSION': 'com.apple.compilers.llvm.clang.1_0',
          'WARNING_CFLAGS': [
            '-Wno-c++11-narrowing',
            '-Wno-covered-switch-default',
            '-Wno-unnamed-type-template-args',
          ],
          'CLANG_CXX_LANGUAGE_STANDARD': 'c++20',
          'CLANG_CXX_LIBRARY': 'libc++',
          'OTHER_CPLUSPLUSFLAGS': [
            '$(inherited)',
          ],
          'SYMROOT': '<(build_base)',
          'GCC_INLINES_ARE_PRIVATE_EXTERN': 'YES',
        },
        'link_settings': {
          'libraries': [
            '$(SDKROOT)/System/Library/Frameworks/Cocoa.framework',
            '$(SDKROOT)/System/Library/Frameworks/CoreFoundation.framework',
            '$(SDKROOT)/System/Library/Frameworks/IOKit.framework',
            '$(SDKROOT)/System/Library/Frameworks/Security.framework',
            '$(SDKROOT)/System/Library/Frameworks/SystemConfiguration.framework',
          ],
        },
      }],
    ],
  },
}
