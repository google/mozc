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

# This file is included by build_mozc.py for Windows.
# common_win.gypi and common.gypi are exclusive each other.
{
  'includes': ['defines.gypi', 'directories.gypi'],
  'variables': {
    # Compiler to build binaries that run in the target environment.
    # e.g. "clang", "gcc", "msvs".
    'compiler_target%': '',

    # Compiler to build binaries that run in the host environment.
    # e.g. "clang", "gcc", "msvs".
    'compiler_host%': '',

    # Visual C++ Runtime Version.
    'vcruntime_ver%': '',

    'conditions': [
      # https://docs.microsoft.com/en-us/cpp/preprocessor/predefined-macros
      ['MSVS_VERSION=="2015"', {
        'compiler_target': 'msvs',
        'compiler_host': 'msvs',
        'vcruntime_ver': '140',
      }],
      ['MSVS_VERSION=="2017"', {
        'compiler_target': 'msvs',
        'compiler_host': 'msvs',
        'vcruntime_ver': '141',
      }],
      ['MSVS_VERSION=="2019"', {
        'compiler_target': 'msvs',
        'compiler_host': 'msvs',
        'vcruntime_ver': '142',
      }],
      ['MSVS_VERSION=="2022"', {
        'compiler_target': 'msvs',
        'compiler_host': 'msvs',
        'vcruntime_ver': '143',
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
      # https://docs.microsoft.com/en-us/dotnet/api/microsoft.visualstudio.vcprojectengine.vcclcompilertool.optimization?view=visualstudiosdk-2017
      # https://docs.microsoft.com/en-us/dotnet/api/microsoft.visualstudio.vcprojectengine.optimizeoption?view=visualstudiosdk-2017
      # https://docs.microsoft.com/en-us/cpp/build/reference/o1-o2-minimize-size-maximize-speed?view=msvc-150
      'win_optimization_debug%': '0',    # 0 = /Od
      'win_optimization_release%': '2',  # 2 = /O2 (maximize speed)
                                         #   = /Og /Oi /Ot /Oy /Ob2 /GF /Gy
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
    },
    'configurations': {
      'x86_Base': {
        'abstract': 1,
        'msvs_settings': {
          'VCCLCompilerTool': {
            'AdditionalOptions': [
              '/bigobj',
            ],
          },
          'VCLinkerTool': {
            'TargetMachine': '<(win_target_machine_x86)',
            'AdditionalOptions': [
              '/SAFESEH',
            ],
            'EnableUAC': 'true',
            'UACExecutionLevel': '0',  # level="asInvoker"
            'UACUIAccess': 'false',    # uiAccess="false"
            'MinimumRequiredVersion': '10.0',
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
          'VCCLCompilerTool': {
            'AdditionalOptions': [
              '/bigobj',
            ],
          },
          'VCLinkerTool': {
            'TargetMachine': '<(win_target_machine_x64)',
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
      },
      'Release_Base': {
        'abstract': 1,
        'defines': [
          'NDEBUG',
          'QT_NO_DEBUG',
          'ABSL_MIN_LOG_LEVEL=100',
          'IGNORE_HELP_FLAG',
          'IGNORE_INVALID_FLAG'
        ],
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
      'DebugDynamic_x64': {
        'inherit_from': ['x64_Base', 'Debug_Base', 'Win_Dynamic_Debug_CRT_Base'],
      },
      'ReleaseDynamic_x64': {
        'inherit_from': ['x64_Base', 'Release_Base', 'Win_Dynamic_Release_CRT_Base'],
      },
      'Debug_x64': {
        'inherit_from': ['x64_Base', 'Debug_Base', 'Win_Static_Debug_CRT_Base'],
      },
      'Release_x64': {
        'inherit_from': ['x64_Base', 'Release_Base', 'Win_Static_Release_CRT_Base'],
      },
    },
    'default_configuration': 'Debug',
    'defines': [
      'COMPILER_MSVC',
      'BUILD_MOZC',  # for ime_shared library
      'ID_TRACE_LEVEL=1',
      'NOMINMAX',
      'PSAPI_VERSION=2',
      'UNICODE',
      'WIN32',
      'WIN32_LEAN_AND_MEAN',
      'WINVER=0x0A00',
      '_ATL_ALL_WARNINGS',
      '_ATL_ALLOW_CHAR_UNSIGNED',
      '_ATL_CSTRING_EXPLICIT_CONSTRUCTORS',
      '_ATL_NO_HOSTING',
      '_ATL_NO_AUTOMATIC_NAMESPACE',
      '_CRT_SECURE_NO_DEPRECATE',
      '_MIDL_USE_GUIDDEF_',
      '_STL_MSVC',
      '_UNICODE',
      '_WIN32',
      '_WIN32_WINNT=0x0A00',
      '_WINDOWS',
    ],
    'include_dirs': [
      '<@(absl_include_dirs)',
      '<(abs_depth)',
      '<(SHARED_INTERMEDIATE_DIR)',
      '<(third_party_dir)/wil/include',
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
        'SuppressStartupBanner': 'true',       # /nologo
        'WarningLevel': '3',                   # /W3
        'OmitFramePointers': 'false',          # /Oy-
        'AdditionalOptions': [
          '/Zc:strictStrings',
          '/utf-8',
          '/std:c++20',
          '/Zc:__cplusplus',
        ],
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
        'AdditionalOptions': [
          '/CETCOMPAT',
        ],
        'DataExecutionPrevention': '2',        # /NXCOMPAT
        'EnableCOMDATFolding': '2',            # /OPT:ICF
        'GenerateDebugInformation': 'true',    # /DEBUG
        'LinkIncremental': '1',                # /INCREMENTAL:NO
        'OptimizeReferences': '2',             # /OPT:REF
        'RandomizedBaseAddress': '2',          # /DYNAMICBASE
        'target_conditions': [
          ['_type=="executable"', {
            # /TSAWARE is valid only on executable target.
            'TerminalServerAware': '2',        # /TSAWARE
            'AdditionalOptions': [
              # We build *.exe with dynamic CRT and deploy CRT DLLs into the
              # application dir. Thus LOAD_LIBRARY_SEARCH_APPLICATION_DIR is
              # also necessary.
              #   0x200: LOAD_LIBRARY_SEARCH_APPLICATION_DIR
              #   0x800: LOAD_LIBRARY_SEARCH_SYSTEM32
              '/DEPENDENTLOADFLAG:0xA00',
            ],
          }, '_type=="shared_library"', {
            'AdditionalOptions': [
              # We build *.dll with staticd CRT. Thus
              # LOAD_LIBRARY_SEARCH_APPLICATION_DIR is not necessary.
              #   0x800: LOAD_LIBRARY_SEARCH_SYSTEM32
              '/DEPENDENTLOADFLAG:0x800',
            ],
          }],
        ],
      },
      'VCResourceCompilerTool': {
        'AdditionalIncludeDirectories': [
          '<(SHARED_INTERMEDIATE_DIR)/',
          '<(DEPTH)/',
        ],
      },
    },
  },
}
