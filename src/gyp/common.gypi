# Copyright 2010-2020, Google Inc.
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
    'compiler_target_version_int%': '0',  # (major_ver) * 100 + (minor_ver)

    # Compiler to build binaries that run in the host environment.
    # e.g. "clang", "gcc", "msvs".
    'compiler_host%': '',
    'compiler_host_version_int%': '0',  # (major_ver) * 100 + (minor_ver)

    # Versioning stuff for Mac.
    'mac_sdk%': '10.15',
    'mac_deployment_target%': '10.9',

    # Flag to specify if the build target is for simulator or not.
    # This is used for iOS simulator so far.
    'simulator_build%': '0',

    # 'conditions' is put inside of 'variables' so that we can use
    # another 'conditions' in this gyp element level later. Note that
    # you can have only one 'conditions' in a gyp element.
    'variables': {
      'extra_warning_cflags': '',
      'conditions': [
        ['warn_as_error!=0', {
          'extra_warning_cflags': '-Werror',
        }],
      ],
    },

    # warning_cflags will be shared with Mac and Linux.
    'warning_cflags': [
      '-Wall',
      '-Wno-char-subscripts',
      '-Wno-sign-compare',
      '-Wno-deprecated-declarations',
      '-Wwrite-strings',
      '<@(extra_warning_cflags)',

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
      '-lc++',
      '-pthread',
    ],

    'conditions': [
      ['OS=="mac"', {
        'compiler_target': 'clang',
        'compiler_target_version_int': 303,  # Clang 3.3 or higher
        'compiler_host': 'clang',
        'compiler_host_version_int': 303,  # Clang 3.3 or higher
      }],
      ['target_platform=="Android"', {
        'compiler_target': 'clang',
        'compiler_target_version_int': 308,  # Clang 3.8 or higher
        'compiler_host': 'clang',
        'compiler_host_version_int': 304,  # Clang 3.4 or higher
      }],
      ['target_platform=="Linux"', {
        'compiler_target': 'clang',
        'compiler_target_version_int': 304,  # Clang 3.4 or higher
        'compiler_host': 'clang',
        'compiler_host_version_int': 304,  # Clang 3.4 or higher
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
          ['target_platform=="Android"', {
            'target_conditions' : [
              ['_toolset=="target"', {
                # We won't debug target's .so file so remove debug symbol.
                # If the symbol is required, remove following line.
                'cflags!': ['-g'],
              }],
            ],
          }],
        ],
      },
      'Release': {
        'defines': [
          'NDEBUG',
          'QT_NO_DEBUG',
          'MOZC_NO_LOGGING',
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
      '<(abs_depth)',
      '<(SHARED_INTERMEDIATE_DIR)',
      '<(absl_dir)',
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
              '-Wno-covered-switch-default',
              '-Wno-unnamed-type-template-args',
              '-Wno-c++11-narrowing',
            ],
          }],
          ['compiler_target=="clang" or compiler_target=="gcc"', {
            'cflags_cc': [
              '-std=c++17',
              '-stdlib=libc++',
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
              '-Wno-covered-switch-default',
              '-Wno-unnamed-type-template-args',
              '-Wno-c++11-narrowing',
            ],
          }],
          ['compiler_host=="clang" or compiler_host=="gcc"', {
            'cflags_cc': [
              '-std=c++17',
              '-stdlib=libc++',
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
        'conditions': [
          ['target_platform=="Linux"', {
            # OS_LINUX is defined always (target and host).
            'defines': ['OS_LINUX',],
          }],
          ['target_platform=="Android"', {
            'defines': ['NO_USAGE_REWRITER'],
            'target_conditions' : [
              ['_toolset=="host"', {
                'defines': ['OS_LINUX',],
              }],
              ['_toolset=="target" and _type=="executable"', {
                # For unittest:
                # Android 5.0+ requires standalone native executables to be PIE.
                # See crbug.com/373219.
                'ldflags': [
                  '-pie',
                ],
              }],
              ['_toolset=="target"', {
                'defines': [
                  'OS_ANDROID',
                  # For the ambiguity of wcsstr.
                  '_WCHAR_H_CPLUSPLUS_98_CONFORMANCE_',
                ],
                'cflags': [
                  # For unittest:
                  # Android 5.0+ requires standalone native executables to be
                  # PIE. Note that we can specify this option even for ICS
                  # unless we ship a standalone native executable.
                  # See crbug.com/373219.
                  '-fPIE',
                ],
                'ldflags!': [  # Remove all libraries for GNU/Linux.
                  '<@(linux_ldflags)',
                ],
                'ldflags': [
                  '-llog',
                  '-static-libstdc++',
                ],
                'conditions': [
                  ['android_arch=="arm"', {
                    'ldflags': [
                      # Support only armv7-a.
                      # Both LDFLAG and CLFAGS should have this.
                      '-march=armv7-a',
                    ],
                    'cflags': [
                      # Support only armv7-a.
                      # Both LDFLAG and CLFAGS should have this.
                      '-march=armv7-a',
                      '-mfloat-abi=softfp',
                      '-mfpu=vfpv3-d16',
                      # Force thumb interaction set for smaller file size.
                      '-mthumb',
                    ],
                  }],
                ],
              }],
            ],
          }],
        ],
      }],
      ['OS=="mac"', {
        'defines': [
          '__APPLE__',
        ],
        'make_global_settings': [
          ['CC', '/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang'],
          ['CXX', '/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++'],
          ['LINK', '/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++'],
          ['LDPLUSPLUS', '/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++'],
        ],
        'conditions': [
          ['target_platform=="iOS"', {
            'target_conditions': [
              ['_toolset=="target"', {
                # OS_IOS and __APPLE__ should be exclusive
                # when iOS is fully supported.
                'defines': ['OS_IOS'],
                'xcode_settings': {
                  'SDKROOT': 'iphoneos',
                  'IPHONEOS_DEPLOYMENT_TARGET': '7.0',
                },
              }],
              ['_toolset=="host"', {
                'xcode_settings': {
                  # TODO(komatsu): If it is possible to remove the setting of
                  # MACOSX_DEPLOYMENT_TARGET when _toolset is "target"
                  # we should do it rather than setting it here and the
                  # next condition.
                  'MACOSX_DEPLOYMENT_TARGET': '<(mac_deployment_target)',
                },
              }],
            ],
            'link_settings': {
              'target_conditions': [
                ['_toolset=="target"', {
                  'libraries': [
                    '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
                  ],
                  'libraries!': [
                    '$(SDKROOT)/System/Library/Frameworks/Cocoa.framework',
                  ],
                }],
              ],
            },
          }],
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
          'CLANG_CXX_LANGUAGE_STANDARD': 'c++17',
          'CLANG_CXX_LIBRARY': 'libc++',
          'OTHER_CPLUSPLUSFLAGS': [
            '$(inherited)',
          ],
          'SYMROOT': '<(build_base)',
          'GCC_INLINES_ARE_PRIVATE_EXTERN': 'YES',
          'OTHER_LDFLAGS': [
            '-headerpad_max_install_names',
          ],
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
  'conditions': [
    ['target_platform=="Linux"', {
      'make_global_settings': [
        ['AR', '<!(which ar)'],
        ['CC', '<!(which clang)'],
        ['CXX', '<!(which clang++)'],
        ['LD', '<!(which ld)'],
        ['NM', '<!(which nm)'],
        ['READELF', '<!(which readelf)'],
      ],
    }],
    ['target_platform=="Android"', {
      'conditions': [
        ['android_arch=="arm"', {
          'variables': {
            'toolchain_prefix': 'arm-linux-androideabi',
          },
        }],
        ['android_arch=="x86"', {
          'variables': {
            'toolchain_prefix': 'i686-linux-android',
          },
        }],
        ['android_arch=="mips"', {
          'variables': {
            'toolchain_prefix': 'mipsel-linux-android',
          },
        }],
        ['android_arch=="arm64"', {
          'variables': {
            'toolchain_prefix': 'aarch64-linux-android',
          },
        }],
        ['android_arch=="x86_64"', {
          'variables': {
            'toolchain_prefix': 'x86_64-linux-android',
          },
        }],
        ['android_arch=="mips64"', {
          'variables': {
            'toolchain_prefix': 'mips64el-linux-android',
          },
        }],
      ],
      # To use clang only CC and CXX should point clang directly.
      # c.f., https://android.googlesource.com/platform/ndk/+/tools_ndk_r9d/docs/text/STANDALONE-TOOLCHAIN.text
      'make_global_settings': [
        ['AR', '<(ndk_bin_dir)/<(toolchain_prefix)-ar'],
        ['CC', '<(ndk_bin_dir)/<(toolchain_prefix)-clang'],
        ['CXX', '<(ndk_bin_dir)/<(toolchain_prefix)-clang++'],
        ['LD', '<(ndk_bin_dir)/<(toolchain_prefix)-ld'],
        ['NM', '<(ndk_bin_dir)/<(toolchain_prefix)-nm'],
        ['READELF', '<(ndk_bin_dir)/<(toolchain_prefix)-readelf'],
        ['AR.host', '<!(which ar)'],
        ['CC.host', '<!(which clang)'],
        ['CXX.host', '<!(which clang++)'],
        ['LD.host', '<!(which ld)'],
        ['NM.host', '<!(which nm)'],
        ['READELF.host', '<!(which readelf)'],
      ],
    }],
  ],
}
