# Copyright 2010, Google Inc.
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
  'variables': {
    # Define default values in the inner 'variables' dict. This looks
    # weird but necessary.
    'variables': {
      # By default, we build mozc in two passes to avoid unnecessary code
      # generation. For instance, it takes quite some time to generate
      # embedded_dictionary_data.h. If we build mozc in one pass, we end up
      # invoking the code generation every time code used in the code
      # generation tool is changed. In fact, a comment fix in 'base/util.h'
      # causes the code generation. We want to avoid that.
      'two_pass_build%': 1,
      # This variable need to be set to 1 when you build Mozc for Chromium OS.
      'chromeos%': 0,
    },
    'two_pass_build%': '<(two_pass_build)',
    'chromeos%': '<(chromeos)',

    # warning_cflags_cflags will be shared with Mac and Linux.
    'warning_cflags': [
      '-Wall',
      '-Werror',
      '-Wno-char-subscripts',
      '-Wno-sign-compare',
      '-Wwrite-strings',
    ],
    # gcc_cflags_cflags will be shared with Mac and Linux.
    'gcc_cflags': [
      '-fmessage-length=0',
      '-fno-omit-frame-pointer',
      '-fno-strict-aliasing',
      '-funsigned-char',
      '-include base/namespace.h',
      '-pipe',
    ],
    'msvc_libs_x86': [
      '<(DEPTH)/../third_party/platformsdk/v6_1/files/Lib',
      '<(DEPTH)/../third_party/atlmfc_vc80/files/lib',
      '<(DEPTH)/../third_party/vc_80/files/vc/lib',
    ],
    'msvc_libs_x64': [
      '<(DEPTH)/../third_party/platformsdk/v6_1/files/lib/x64',
      '<(DEPTH)/../third_party/atlmfc_vc80/files/lib/amd64',
      '<(DEPTH)/../third_party/vc_80/files/vc/lib/amd64',
    ],
    # We wanted to have this directory under the build output directory
    # (ex. 'out' for Linux), but there is no variable defined for the top
    # level source directory, hence we create the directory in the top
    # level source directory.
    'mozc_build_tools_dir': '<(DEPTH)/mozc_build_tools/<(OS)',
    'proto_out_dir': '<(SHARED_INTERMEDIATE_DIR)/proto_out',
    'branding%': 'Mozc',
  },
  'target_defaults': {
    'variables': {
      # See http://gcc.gnu.org/onlinedocs/gcc-4.4.2/gcc/Optimize-Options.html
      'mac_release_optimization%': '2', # Use -O2 unless overridden
      'mac_debug_optimization%': '0',   # Use -O0 unless overridden
      # See http://msdn.microsoft.com/en-us/library/aa652360(VS.71).aspx
      'win_release_optimization%': '2', # 2 = /Os
      'win_debug_optimization%': '0',   # 0 = /Od
      # See http://msdn.microsoft.com/en-us/library/aa652367(VS.71).aspx
      'win_release_static_crt%': '0',  # 0 = /MT (nondebug static)
      'win_debug_static_crt%': '1',    # 1 = /MTd (debug static)
      'win_release_dynamic_crt%': '2', # 2 = /MD (nondebug dynamic)
      'win_debug_dynamic_crt%': '3',   # 3 = /MDd (debug dynamic)
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
      'Common_Base': {
        'abstract': 1,
        'msvs_configuration_attributes': {
          'CharacterSet': '<(win_char_set_unicode)',
        },
        'conditions': [
          ['branding=="GoogleJapaneseInput"', {
           'defines': ['GOOGLE_JAPANESE_INPUT_BUILD'],
          }, {  # else
           'defines': ['MOZC_BUILD'],
          },],
        ],
      },
      'x86_Base': {
        'abstract': 1,
        'msvs_settings': {
          'VCLibrarianTool': {
            'AdditionalLibraryDirectories': [
              '<@(msvc_libs_x86)',
            ],
            'AdditionalLibraryDirectories!': [
              '<@(msvc_libs_x64)',
            ],
          },
          'VCLinkerTool': {
            'TargetMachine': '<(win_target_machine_x86)',
            'AdditionalOptions': [
              '/SAFESEH',
            ],
            'AdditionalLibraryDirectories': [
              '<@(msvc_libs_x86)',
            ],
            'AdditionalLibraryDirectories!': [
              '<@(msvc_libs_x64)',
            ],
          },
        },
        'msvs_configuration_attributes': {
          'OutputDirectory': '<(DEPTH)/out_win/$(ConfigurationName)',
          'IntermediateDirectory': '<(DEPTH)/out_win/$(ConfigurationName)/obj/$(ProjectName)',
        },
        'msvs_configuration_platform': 'Win32',
      },
      'x64_Base': {
        'abstract': 1,
        'msvs_configuration_attributes': {
          'OutputDirectory': '<(DEPTH)/out_win/$(ConfigurationName)64',
          'IntermediateDirectory': '<(DEPTH)/out_win/$(ConfigurationName)64/obj/$(ProjectName)',
        },
        'msvs_configuration_platform': 'x64',
        'msvs_settings': {
          'VCLibrarianTool': {
            'AdditionalLibraryDirectories': [
              '<@(msvc_libs_x64)',
            ],
            'AdditionalLibraryDirectories!': [
              '<@(msvc_libs_x86)',
            ],
          },
          'VCLinkerTool': {
            'TargetMachine': '<(win_target_machine_x64)',
            'AdditionalLibraryDirectories': [
              '<@(msvc_libs_x64)',
            ],
            'AdditionalLibraryDirectories!': [
              '<@(msvc_libs_x86)',
            ],
          },
        },
      },
      'Debug_Base': {
        'abstract': 1,
        'xcode_settings': {
          'COPY_PHASE_STRIP': 'NO',
          'GCC_OPTIMIZATION_LEVEL': '<(mac_debug_optimization)',
          'OTHER_CFLAGS': [ '<@(debug_extra_cflags)', ],
        },
        'msvs_settings': {
          'VCCLCompilerTool': {
            'Optimization': '<(win_debug_optimization)',
            'PreprocessorDefinitions': ['_DEBUG'],
            'BasicRuntimeChecks': '3',
            'RuntimeLibrary': '<(win_debug_static_crt)',
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
        ],
      },
      'Optimize_Base': {
        'abstract': 1,
        'defines': [
          'NDEBUG',
          'QT_NO_DEBUG',
        ],
        'xcode_settings': {
          'DEAD_CODE_STRIPPING': 'YES',  # -Wl,-dead_strip
          'GCC_OPTIMIZATION_LEVEL': '<(mac_release_optimization)',
          'OTHER_CFLAGS': [ '<@(release_extra_cflags)', ],
        },
        'msvs_settings': {
          'VCCLCompilerTool': {
            'Optimization': '<(win_release_optimization)',
            'RuntimeLibrary': '<(win_release_static_crt)',
          },
        },
        'conditions': [
          ['OS=="linux"', {
            'cflags': [
             '<@(release_extra_cflags)',
            ],
          }],
        ],
      },
      'Release_Base': {
        'abstract': 1,
        'defines': [
          'NO_LOGGING',
          'IGNORE_HELP_FLAG',
          'IGNORE_INVALID_FLAG'
        ],
        'msvs_configuration_attributes': {
          'WholeProgramOptimization': '1',
        },
        'msvs_settings': {
          'VCLinkerTool': {
            # /PDBALTPATH is documented in Visual C++ 2010
            # http://msdn.microsoft.com/en-us/library/dd998269(VS.100).aspx
            'AdditionalOptions': ['/PDBALTPATH:%_PDB%'],
          },
        },
      },
      #
      # Concrete configurations
      #
      'Debug': {
        'inherit_from': ['Common_Base', 'x86_Base', 'Debug_Base'],
      },
      'Optimize': {
        'inherit_from': ['Common_Base', 'x86_Base', 'Optimize_Base'],
      },
      'Release': {
        'inherit_from': ['Optimize', 'Release_Base'],
      },
      'conditions': [
        ['OS=="win"', {
          'Debug_x64': {
            'inherit_from': ['Common_Base', 'x64_Base', 'Debug_Base'],
          },
          'Optimize_x64': {
            'inherit_from': ['Common_Base', 'x64_Base', 'Optimize_Base'],
          },
          'Release_x64': {
            'inherit_from': ['Optimize_x64', 'Release_Base'],
          },
        }],
      ],
    },
    'default_configuration': 'Debug',
    'defines': [
      # For gtest
      'GTEST_HAS_TR1_TUPLE=0',
      # For gtest
      # For gtest
      'MOZC_DATA_DIR="<(SHARED_INTERMEDIATE_DIR)"',
    ],
    'include_dirs': [
      '<(DEPTH)',
      '<(DEPTH)/..',
      '<(DEPTH)/third_party/breakpad/src',
      '<(SHARED_INTERMEDIATE_DIR)',
      '<(SHARED_INTERMEDIATE_DIR)/proto_out',
    ],
    'conditions': [
      ['OS=="win"', {
        'defines': [
          'COMPILER_MSVC',
          'BUILD_MOZC',  # for ime_shared library
          'ID_TRACE_LEVEL=1',
          'OS_WINDOWS',
          'UNICODE',
          'WIN32',
          'WIN32_IE=0x0600',
          'WINVER=0x0501',
          '_ATL_ALL_WARNINGS',
          '_ATL_APARTMENT_THREADED',
          '_ATL_CSTRING_EXPLICIT_CONSTRUCTORS',
          '_CONSOLE',
          '_CRT_SECURE_NO_DEPRECATE',
          '_MIDL_USE_GUIDDEF_',
          '_STL_MSVC',
          '_UNICODE',
          '_USRDLL',
          '_WIN32',
          '_WIN32_WINDOWS=0x0410',
          '_WIN32_WINNT=0x0501',
          '_WINDLL',
          '_WINDOWS',
        ],
        'include_dirs': [
          '../protobuf/files/src',
          '<(DEPTH)/../third_party/atlmfc_vc80/files/include',
          '<(DEPTH)/../third_party/platformsdk/v6_1/files/include',
          '<(DEPTH)/../third_party/vc_80/files/vc/include',
          '<(DEPTH)/../third_party/wtl_80/files/include',
          '<(DEPTH)/third_party/gtest',
          '<(DEPTH)/third_party/gtest/include',
        ],
        # We don't have cygwin in our tree, but we need to have
        # setup_env.bat in the directory specified in 'msvs_cygwin_dirs'
        # for GYP to be happy.
        'msvs_cygwin_dirs': [
          '<(DEPTH)/gyp',
        ],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'BufferSecurityCheck': 'True',         # /GS
            'CompileAs': '2',                      # /TP
            'CompileOnly': 'True',                 # /c
            'DebugInformationFormat': '3',         # /Zi
            'DefaultCharIsUnsigned': 'True',       # /J
            'DisableSpecificWarnings': ['4018', '4150', '4722'],
                                                   # /wdXXXX
            'EnableFunctionLevelLinking': 'True',  # /Gy
            'EnableIntrinsicFunctions': 'True',    # /Oi
            'ExceptionHandling': '2',              # /EHs
            'ForcedIncludeFiles': ['base/namespace.h'],
                                                   # /FI<header_file.h>
            'OptimizeForWindows98': '1',           # /OPT:NOWIN98
            'OmitFramePointers': 'False',          # /Oy-
            'SuppressStartupBanner': 'True',       # /nologo
            'TreatWChar_tAsBuiltInType': 'False',  # /Zc:wchar_t-
            'WarningLevel': '3',                   # /W3
          },
          'VCLinkerTool': {
            # TODO(satorux): Reduce the number of libraries. We should
            # only have common libraries here.
            'AdditionalDependencies': [
              'Advapi32.lib',
              'comdlg32.lib',
              'crypt32.lib',
              'dbghelp.lib',
              'delayimp.lib',
              'gdi32.lib',
              'imm32.lib',
              'kernel32.lib',
              'msi.lib',
              'odbc32.lib',
              'odbccp32.lib',
              'ole32.lib',
              'oleaut32.lib',
              'psapi.lib',
              'shell32.lib',
              'shlwapi.lib',
              'user32.lib',
              'uuid.lib',
              'version.lib',
              'wininet.lib',
              'winmm.lib',
              'winspool.lib',
            ],
            'AdditionalOptions': [
              '/DYNAMICBASE',  # 'RandomizedBaseAddress': 'True'
              '/NXCOMPAT',
            ],
            'EnableCOMDATFolding': '2',            # /OPT:ICF
            'GenerateDebugInformation': 'True',    # /DEBUG
            'LinkIncremental': '1',                # /INCREMENTAL:NO
            'OptimizeReferences': '2',             # /OPT:REF
            'TerminalServerAware': '2',            # /TSAWARE
          },
          'VCResourceCompilerTool': {
            # TODO(yukawa): embed version info via command line parameters.
            'PreprocessorDefinitions': [
              'MOZC_RES_USE_TEMPLATE=1',
              'MOZC_RES_FILE_VERSION_NUMBER="0,0,1,9999"',
              'MOZC_RES_PRODUCT_VERSION_NUMBER="0,0,1,9999"',
              'MOZC_RES_FILE_VERSION_STRING="""0.0.1.9999"""',
              'MOZC_RES_PRODUCT_VERSION_STRING="""0.0.1.9999"""',
              'MOZC_RES_COMPANY_NAME="""Google Inc."""',
              'MOZC_RES_LEGAL_COPYRIGHT="""Copyright (C) 2010 Google Inc. All Rights Reserved."""',
            ],
            'AdditionalIncludeDirectories': ['<(DEPTH)'],
          },
        },
      }],
      ['OS=="linux"', {
        'defines': [
          'OS_LINUX',
        ],
        'cflags': [
          '<@(gcc_cflags)',
          '-fPIC',
          '-fno-exceptions',
        ],
        'libraries': [
          '-lcurl',
          '-lcrypto',
          '-lpthread',
          '-lrt',
          '-lssl',
          '-lz',
        ],
        'conditions': [
          ['chromeos==1', {
            'defines': [
              'OS_CHROMEOS',
            ],
          }],
        ],
      }],
      ['OS=="mac"', {
        'defines': [
          'OS_MACOSX',
        ],
        'include_dirs': [
          '../protobuf/files/src',
          '<(DEPTH)/third_party/gtest',
          '<(DEPTH)/third_party/gtest/include',
        ],
        'mac_framework_dirs': [
          '<(DEPTH)/../mac/Releases/GoogleBreakpad',
        ],
        'xcode_settings': {
          'GCC_ENABLE_CPP_EXCEPTIONS': 'NO',  # -fno-exceptions
          'OTHER_CFLAGS': [
            '<@(gcc_cflags)',
          ],
          'WARNING_CFLAGS': ['<@(warning_cflags)'],
        },
        'link_settings': {
          'libraries': [
            '$(SDKROOT)/System/Library/Frameworks/Cocoa.framework',
            '$(SDKROOT)/System/Library/Frameworks/CoreFoundation.framework',
            '$(SDKROOT)/System/Library/Frameworks/Security.framework',
            '$(SDKROOT)/System/Library/Frameworks/SystemConfiguration.framework',
            '/usr/lib/libcrypto.dylib',
            '/usr/lib/libcurl.dylib',
            '/usr/lib/libiconv.dylib',
            '/usr/lib/libssl.dylib',
            '/usr/lib/libz.dylib',
          ],
        }
      }],
    ],
  },
}
