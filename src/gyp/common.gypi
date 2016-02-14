# Copyright 2010-2016, Google Inc.
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
    'mac_sdk%': '10.9',
    'mac_deployment_target%': '10.7',

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
    ],

    # gcc_cflags will be shared with Mac and Linux
    'gcc_cflags': [
      '-fmessage-length=0',
      '-fno-strict-aliasing',
      '-funsigned-char',
      '-include base/namespace.h',
      '-pipe',
      '-pthread',
    ],
    # linux_cflags will be used in Linux except for NaCl.
    'linux_cflags': [
      '<@(gcc_cflags)',
      '-fno-omit-frame-pointer',
      '-fstack-protector',
      '--param=ssp-buffer-size=4',
    ],
    # nacl_cflags will be used for NaCl.
    # -fno-omit-frame-pointer flag does not work correctly.
    #   http://code.google.com/p/chromium/issues/detail?id=122623
    'nacl_cflags': [
      '<@(gcc_cflags)',
    ],
    # mac_cflags will be used in Mac.
    # Xcode 4.5 which we are currently using does not support ssp-buffer-size.
    # TODO(horo): When we can use Xcode 4.6 which supports ssp-buffer-size,
    # set ssp-buffer-size in Mac.
    'mac_cflags': [
      '<@(gcc_cflags)',
      '-fno-omit-frame-pointer',
      '-fstack-protector',
    ],
    # Libraries for GNU/Linux environment.
    'linux_ldflags': [
      '-pthread',
    ],

    # Extra defines
    'additional_defines%': [],

    # Extra headers and libraries for Visual C++.
    'msvs_includes%': [],
    'msvs_libs_x86%': [],
    'msvs_libs_x64%': [],

    'conditions': [
      ['OS=="win"', {
        'conditions': [
          ['MSVS_VERSION=="2015"', {
            'compiler_target': 'msvs',
            'compiler_target_version_int': 1900,  # Visual C++ 2015 or higher
            'compiler_host': 'msvs',
            'compiler_host_version_int': 1900,  # Visual C++ 2015 or higher
          }],
        ],
      }],
      ['OS=="mac"', {
        'compiler_target': 'clang',
        'compiler_target_version_int': 303,  # Clang 3.3 or higher
        'compiler_host': 'clang',
        'compiler_host_version_int': 303,  # Clang 3.3 or higher
      }],
      ['target_platform=="Android" and android_compiler=="clang"', {
        'compiler_target': 'clang',
        'compiler_target_version_int': 305,  # Clang 3.5 or higher
        'compiler_host': 'clang',
        'compiler_host_version_int': 304,  # Clang 3.4 or higher
      }],
      ['target_platform=="Android" and android_compiler=="gcc"', {
        'compiler_target': 'gcc',
        'compiler_target_version_int': 409,  # GCC 4.9 or higher
        'compiler_host': 'clang',
        'compiler_host_version_int': 304,  # Clang 3.4 or higher
      }],
      ['target_platform=="NaCl"', {
        'compiler_target': 'clang',
        'compiler_target_version_int': 304,  # Clang 3.3 or higher
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
    'msvc_disabled_warnings': [
      # 'expression' : signed/unsigned mismatch
      # http://msdn.microsoft.com/en-us/library/y92ktdf2.aspx
      '4018',
      # 'argument' : conversion from 'type1' to 'type2', possible loss
      # of data
      # http://msdn.microsoft.com/en-us/library/2d7604yb.aspx
      '4244',
      # 'var' : conversion from 'size_t' to 'type', possible loss of
      # data
      # http://msdn.microsoft.com/en-us/library/6kck0s93.aspx
      '4267',
      # 'identifier' : truncation from 'type1' to 'type2'
      # http://msdn.microsoft.com/en-us/library/0as1ke3f.aspx
      '4305',
      # The file contains a character that cannot be represented in the
      # current code page (number). Save the file in Unicode format to
      # prevent data loss.
      # http://msdn.microsoft.com/en-us/library/ms173715.aspx
      '4819',
      # Suppress warning against functions marked as 'deprecated'.
      # http://msdn.microsoft.com/en-us/library/vstudio/8wsycdzs.aspx
      '4995',
      # Suppress warning against functions marked as 'deprecated'.
      # http://msdn.microsoft.com/en-us/library/vstudio/ttcz0bys.aspx
      '4996',
    ],
  },
  'target_defaults': {
    'variables': {
      # See http://gcc.gnu.org/onlinedocs/gcc-4.4.2/gcc/Optimize-Options.html
      'mac_release_optimization%': '2',  # Use -O2 unless overridden
      'mac_debug_optimization%': '0',    # Use -O0 unless overridden
      # See http://msdn.microsoft.com/en-us/library/aa652360(VS.71).aspx
      'win_optimization_debug%': '0',    # 0 = /Od
      'win_optimization_release%': '2',  # 2 = /Og /Oi /Ot /Oy /Ob2 /Gs /GF /Gy
      'win_optimization_custom%': '4',   # 4 = None but prevents vcbuild from
                                         # inheriting default optimization.
      # See http://msdn.microsoft.com/en-us/library/aa652367(VS.71).aspx
      'win_release_static_crt%': '0',   # 0 = /MT (nondebug static)
      'win_debug_static_crt%': '1',     # 1 = /MTd (debug static)
      'win_release_dynamic_crt%': '2',  # 2 = /MD (nondebug dynamic)
      'win_debug_dynamic_crt%': '3',    # 3 = /MDd (debug dynamic)
      # See http://msdn.microsoft.com/en-us/library/aa652352(VS.71).aspx
      'win_target_machine_x86%': '1',
      'win_target_machine_x64%': '17',
      # See http://msdn.microsoft.com/en-us/library/aa652256(VS.71).aspx
      'win_char_set_not_set%': '0',
      'win_char_set_unicode%': '1',
      'win_char_set_mbcs%': '2',
      # Extra cflags for gcc
      'release_extra_cflags%': ['-O2'],
      'debug_extra_cflags%': ['-O0', '-g'],
    },
    'configurations': {
      'x86_Base': {
        'abstract': 1,
        'msvs_settings': {
          'VCLibrarianTool': {
            'AdditionalLibraryDirectories': [
              '<@(msvs_libs_x86)',
            ],
            'AdditionalLibraryDirectories!': [
              '<@(msvs_libs_x64)',
            ],
          },
          'VCLinkerTool': {
            'TargetMachine': '<(win_target_machine_x86)',
            'AdditionalOptions': [
              '/SAFESEH',
            ],
            'AdditionalLibraryDirectories': [
              '<@(msvs_libs_x86)',
            ],
            'AdditionalLibraryDirectories!': [
              '<@(msvs_libs_x64)',
            ],
            'EnableUAC': 'true',
            'UACExecutionLevel': '0',  # level="asInvoker"
            'UACUIAccess': 'false',    # uiAccess="false"
            'MinimumRequiredVersion': '6.01',
          },
        },
        'msvs_configuration_attributes': {
          'OutputDirectory': '<(build_base)/$(ConfigurationName)',
          'IntermediateDirectory': '<(build_base)/$(ConfigurationName)/obj/$(ProjectName)',
        },
        'msvs_configuration_platform': 'Win32',
      },
      'x64_Base': {
        'abstract': 1,
        'msvs_configuration_attributes': {
          'OutputDirectory': '<(build_base)/$(ConfigurationName)_x64',
          'IntermediateDirectory': '<(build_base)/$(ConfigurationName)_x64/obj/$(ProjectName)',
        },
        'msvs_configuration_platform': 'x64',
        'msvs_settings': {
          'VCLibrarianTool': {
            'AdditionalLibraryDirectories': [
              '<@(msvs_libs_x64)',
            ],
            'AdditionalLibraryDirectories!': [
              '<@(msvs_libs_x86)',
            ],
          },
          'VCLinkerTool': {
            'TargetMachine': '<(win_target_machine_x64)',
            'AdditionalLibraryDirectories': [
              '<@(msvs_libs_x64)',
            ],
            'AdditionalLibraryDirectories!': [
              '<@(msvs_libs_x86)',
            ],
          },
        },
      },
      'Win_Static_Debug_CRT_Base': {
        'abstract': 1,
        'msvs_settings': {
          'VCCLCompilerTool': {
            'RuntimeLibrary': '<(win_debug_static_crt)',
          },
        },
      },
      'Win_Static_Release_CRT_Base': {
        'abstract': 1,
        'msvs_settings': {
          'VCCLCompilerTool': {
            'RuntimeLibrary': '<(win_release_static_crt)',
          },
        },
      },
      'Win_Dynamic_Debug_CRT_Base': {
        'abstract': 1,
        'msvs_settings': {
          'VCCLCompilerTool': {
            'RuntimeLibrary': '<(win_debug_dynamic_crt)',
          },
        },
      },
      'Win_Dynamic_Release_CRT_Base': {
        'abstract': 1,
        'msvs_settings': {
          'VCCLCompilerTool': {
            'RuntimeLibrary': '<(win_release_dynamic_crt)',
          },
        },
      },
      'Debug_Base': {
        'abstract': 1,
        'defines': [
          'DEBUG',
        ],
        'xcode_settings': {
          'COPY_PHASE_STRIP': 'NO',
          'GCC_OPTIMIZATION_LEVEL': '<(mac_debug_optimization)',
          'GCC_INLINES_ARE_PRIVATE_EXTERN': 'YES',
          'OTHER_CFLAGS': [ '<@(debug_extra_cflags)', ],
        },
        'msvs_settings': {
          'VCCLCompilerTool': {
            'Optimization': '<(win_optimization_debug)',
            'PreprocessorDefinitions': ['_DEBUG'],
            'BasicRuntimeChecks': '3',
          },
          'VCResourceCompilerTool': {
            'PreprocessorDefinitions': ['_DEBUG'],
          },
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
          ['target_platform=="NaCl"', {
            'ldflags': [
              '-L<(nacl_sdk_root)/lib/pnacl/Debug',
            ],
          }],
        ],
      },
      'Release_Base': {
        'abstract': 1,
        'defines': [
          'NDEBUG',
          'QT_NO_DEBUG',
          'NO_LOGGING',
          'IGNORE_HELP_FLAG',
          'IGNORE_INVALID_FLAG'
        ],
        'xcode_settings': {
          'DEAD_CODE_STRIPPING': 'YES',  # -Wl,-dead_strip
          'GCC_OPTIMIZATION_LEVEL': '<(mac_release_optimization)',
          'OTHER_CFLAGS': [ '<@(release_extra_cflags)', ],
        },
        'msvs_settings': {
          'VCCLCompilerTool': {
            # '<(win_optimization_release)' (that is /O2) contains /Oy option,
            # which makes debugging extremely difficult. (See b/1852473)
            # http://msdn.microsoft.com/en-us/library/8f8h5cxt.aspx
            # We can still disable FPO by using /Oy- option but the document
            # says there is an order dependency, that is, the last /Oy  or /Oy-
            # is valid.  See the following document for details.
            # http://msdn.microsoft.com/en-us/library/2kxx5t2c.aspx
            # As far as we observed, /Oy- adding in 'AdditionalOptions' always
            # appears at the end of options so using
            # '<(win_optimization_release) here is considered to be harmless.
            # Another reason to use /O2 here is b/2822535, where we observed
            # warning C4748 when we build mozc_tool with Qt libraries, which
            # are built with /O2.  We use the same optimization option between
            # Mozc and Qt just in case warning C4748 is true.
            'Optimization': '<(win_optimization_release)',
            'WholeProgramOptimization': 'true',
          },
          'VCLibrarianTool': {
            'LinkTimeCodeGeneration': 'true',
          },
          'VCLinkerTool': {
            # 1 = 'LinkTimeCodeGenerationOptionUse'
            'LinkTimeCodeGeneration': '1',
            # /PDBALTPATH is documented in Visual C++ 2010
            # http://msdn.microsoft.com/en-us/library/dd998269(VS.100).aspx
            'AdditionalOptions': ['/PDBALTPATH:%_PDB%'],
          },
        },
        'conditions': [
          ['OS=="linux"', {
            'cflags': [
              '<@(release_extra_cflags)',
            ],
          }],
          ['target_platform=="NaCl"', {
            'ldflags': [
              '-L<(nacl_sdk_root)/lib/pnacl/Release',
            ],
          }],
        ],
      },
      #
      # Concrete configurations
      #
      'Debug': {
        'inherit_from': ['x86_Base', 'Debug_Base', 'Win_Static_Debug_CRT_Base'],
      },
      'Release': {
        'inherit_from': ['x86_Base', 'Release_Base', 'Win_Static_Release_CRT_Base'],
      },
      'conditions': [
        ['OS=="win"', {
          'DebugDynamic': {
            'inherit_from': ['x86_Base', 'Debug_Base', 'Win_Dynamic_Debug_CRT_Base'],
          },
          'ReleaseDynamic': {
            'inherit_from': ['x86_Base', 'Release_Base', 'Win_Dynamic_Release_CRT_Base'],
          },
          'Debug_x64': {
            'inherit_from': ['x64_Base', 'Debug_Base', 'Win_Static_Debug_CRT_Base'],
          },
          'Release_x64': {
            'inherit_from': ['x64_Base', 'Release_Base', 'Win_Static_Release_CRT_Base'],
          },
        }],
      ],
    },
    'default_configuration': 'Debug',
    'defines': [
      '<@(additional_defines)',
    ],
    'include_dirs': [
      '<(abs_depth)',
      '<(SHARED_INTERMEDIATE_DIR)',
    ],
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
              '-std=gnu++0x',
            ],
          }],
          ['compiler_target=="clang" or compiler_target=="gcc"', {
            'cflags_cc': [
              '-std=gnu++0x',
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
              '-std=gnu++0x',
            ],
          }],
          ['compiler_host=="clang" or compiler_host=="gcc"', {
            'cflags_cc': [
              '-std=gnu++0x',
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
      }],
      ['OS=="win"', {
        'defines': [
          'COMPILER_MSVC',
          'BUILD_MOZC',  # for ime_shared library
          'ID_TRACE_LEVEL=1',
          'NOMINMAX',
          'OS_WIN',
          'PSAPI_VERSION=2',
          'UNICODE',
          'WIN32',
          'WIN32_IE=0x0800',
          'WINVER=0x0601',
          'WIN32_LEAN_AND_MEAN',
          '_ATL_ALL_WARNINGS',
          '_ATL_ALLOW_CHAR_UNSIGNED',
          '_ATL_CSTRING_EXPLICIT_CONSTRUCTORS',
          '_CRT_SECURE_NO_DEPRECATE',
          '_MIDL_USE_GUIDDEF_',
          '_STL_MSVC',
          '_UNICODE',
          '_WIN32',
          '_WIN32_WINDOWS=0x0601',
          '_WIN32_WINNT=0x0601',
          '_WINDOWS',
        ],
        'include_dirs': [
          '<@(msvs_includes)',
          '<(wtl_dir)/include',
        ],
        'msvs_configuration_attributes': {
          'CharacterSet': '<(win_char_set_unicode)',
        },
        'msvs_cygwin_shell': 0,
        'msvs_disabled_warnings': ['<@(msvc_disabled_warnings)'],  # /wdXXXX
        'msvs_settings': {
          'VCCLCompilerTool': {
            'BufferSecurityCheck': 'true',         # /GS
            'DebugInformationFormat': '3',         # /Zi
            'DefaultCharIsUnsigned': 'true',       # /J
            'EnableFunctionLevelLinking': 'true',  # /Gy
            'EnableIntrinsicFunctions': 'true',    # /Oi
            'ExceptionHandling': '2',              # /EHs
            'ForcedIncludeFiles': ['base/namespace.h'],
                                                   # /FI<header_file.h>
            'SuppressStartupBanner': 'true',       # /nologo
            'TreatWChar_tAsBuiltInType': 'false',  # /Zc:wchar_t-
            'WarningLevel': '3',                   # /W3
            'OmitFramePointers': 'false',          # /Oy-
          },
          'VCLinkerTool': {
            'AdditionalDependencies': [
              'advapi32.lib',
              'comdlg32.lib',
              'delayimp.lib',
              'gdi32.lib',
              'imm32.lib',
              'kernel32.lib',
              'ole32.lib',
              'oleaut32.lib',
              'shell32.lib',
              'user32.lib',
              'uuid.lib',
            ],
            'DataExecutionPrevention': '2',        # /NXCOMPAT
            'EnableCOMDATFolding': '2',            # /OPT:ICF
            'GenerateDebugInformation': 'true',    # /DEBUG
            'LinkIncremental': '1',                # /INCREMENTAL:NO
            'OptimizeReferences': '2',             # /OPT:REF
            'RandomizedBaseAddress': '2',          # /DYNAMICBASE
            'target_conditions': [
              # /TSAWARE is valid only on executable target.
              ['_type=="executable"', {
                'TerminalServerAware': '2',        # /TSAWARE
              }],
            ],
          },
          'VCResourceCompilerTool': {
            'PreprocessorDefinitions': [
              'MOZC_RES_USE_TEMPLATE=1',
            ],
            'AdditionalIncludeDirectories': [
              '<(SHARED_INTERMEDIATE_DIR)/',
              '<(DEPTH)/',
            ],
          },
        },
      }],
      ['OS=="linux"', {
        'cflags': [
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
          ['target_platform!="NaCl"', {
            'cflags': [
              '<@(linux_cflags)',
            ],
          }],
          ['target_platform=="NaCl"', {
            'target_conditions' : [
              ['_toolset=="host"', {
                'defines': ['OS_LINUX',],
                'cflags': [
                  '<@(linux_cflags)',
                ],
              }],
              ['_toolset=="target"', {
                'cflags': [
                  '<@(nacl_cflags)',
                ],
                'ldflags!': [  # Remove all libraries for GNU/Linux.
                  '<@(linux_ldflags)',
                ],
                'defines': [
                  'MOZC_USE_PEPPER_FILE_IO',
                  'OS_NACL',
                  # For the ambiguity of wcsstr.
                  '_WCHAR_H_CPLUSPLUS_98_CONFORMANCE_',
                ],
                'include_dirs': [
                  '<(nacl_sdk_root)/include',
                ],
              }],
              ['_toolset=="target" and _type=="static_library"', {
                # PNaCl's artools.py doesn't handle some thin archive files
                # correctly. (crbug/463026)
                # But GYP creates thin archive files for static_library.
                # To avoid this issue we turn on this flag.
                # TODO(hsumita): Remove this hack when the bug in SDK is fixed.
                'standalone_static_library': 1,
              }],
            ]
          }],
        ],
      }],
      ['OS=="mac"', {
        'defines': [
          'OS_MACOSX',
        ],
        'make_global_settings': [
          ['CC', '/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang'],
          ['CXX', '/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++'],
          ['LINK', '/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++'],
          ['LDPLUSPLUS', '/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++'],
        ],
        'conditions': [
          ['branding=="GoogleJapaneseInput"', {
            'mac_framework_dirs': [
              '<(mac_breakpad_dir)',
            ],
          }],
        ],
        'xcode_settings': {
          'ARCHS': ['i386'],
          'GCC_ENABLE_CPP_EXCEPTIONS': 'NO',  # -fno-exceptions
          'GCC_SYMBOLS_PRIVATE_EXTERN': 'NO',  # No -fvisibility=hidden
          'OTHER_CFLAGS': [
            '<@(mac_cflags)',
          ],
          'WARNING_CFLAGS': ['<@(warning_cflags)'],
          'MACOSX_DEPLOYMENT_TARGET': '<(mac_deployment_target)',
          'SDKROOT': 'macosx<(mac_sdk)',
          'PYTHONPATH': '<(abs_depth)/',
          'CLANG_WARN_CXX0X_EXTENSIONS': 'NO',
          'GCC_VERSION': 'com.apple.compilers.llvm.clang.1_0',
          'WARNING_CFLAGS': [
            '-Wno-c++11-narrowing',
            '-Wno-covered-switch-default',
            '-Wno-unnamed-type-template-args',
          ],
          'CLANG_CXX_LANGUAGE_STANDARD': 'c++11',
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
  'conditions': [
    ['target_platform=="NaCl"', {
      'make_global_settings': [
        ['AR', '<(pnacl_bin_dir)/pnacl-ar'],
        ['CC', '<(pnacl_bin_dir)/pnacl-clang'],
        ['CXX', '<(pnacl_bin_dir)/pnacl-clang++'],
        ['LD', '<(pnacl_bin_dir)/pnacl-ld'],
        ['NM', '<(pnacl_bin_dir)/pnacl-nm'],
        ['READELF', '<(pnacl_bin_dir)/pnacl-readelf'],
        ['AR.host', '<!(which ar)'],
        ['CC.host', '<!(which clang)'],
        ['CXX.host', '<!(which clang++)'],
        ['LD.host', '<!(which ld)'],
        ['NM.host', '<!(which nm)'],
        ['READELF.host', '<!(which readelf)'],
      ],
    }],
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
        ['android_compiler=="gcc"', {
          'variables': {
            'c_compiler': 'gcc',
            'cxx_compiler': 'g++',
          }
        }],
        ['android_compiler=="clang"', {
          'variables': {
            'c_compiler': 'clang',
            'cxx_compiler': 'clang++',
          }
        }],
      ],
      # To use clang only CC and CXX should point clang directly.
      # c.f., https://android.googlesource.com/platform/ndk/+/tools_ndk_r9d/docs/text/STANDALONE-TOOLCHAIN.text
      'make_global_settings': [
        ['AR', '<(ndk_bin_dir)/<(toolchain_prefix)-ar'],
        ['CC', '<(ndk_bin_dir)/<(toolchain_prefix)-<(c_compiler)'],
        ['CXX', '<(ndk_bin_dir)/<(toolchain_prefix)-<(cxx_compiler)'],
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
