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

# TARGETS:
#   install  = Installs the apk package into a real device or an emulator.
#   apk      = Builds the apk package.
#   run_test = Installs the apk package and runs the Java test suite.
#   run_native_small_test = Builds and runs C++ unit tests (small size).
#   run_native_large_test = Builds and runs C++ unit tests (large size).
#   adt_apk_dependencies  = Prepares everything ADT needs.
#
# NOTES:
# The apk package will be the release or debug version depending on -c option
# of build_mozc.py (Release_Android or Debug_Android).
#
# For signing, neither key.store nor key.alias in project.properties is set,
# and the default debug key is used.  You need to set them in local.properties
# (recommended) or project.properties when you need to sign the package with
# the release key.
{
  'variables': {
    'app_package_name': '<(android_application_id)',
    'relative_dir': 'android',
    # Actions with an existing input and non-existing output behave like
    # phony rules.  Nothing matters for an input but its existence, so
    # we use 'android.gyp' as a dummy input since it must exist.
    'dummy_input_file': 'android.gyp',
    # GYP's 'copies' rule cannot copy a whole directory recursively, so we use
    # our own script to copy files.
    'copy_file': ['python', '../build_tools/copy_file.py'],
    # Android Development Tools
    'adt_gen_dir': 'gen_for_adt',
    'adt_test_gen_dir': 'tests/gen_for_adt',
    # Android SDK
    'sdk_gen_dir': 'gen',
    'sdk_test_gen_dir': 'tests/gen',
    'sdk_asset_dir': 'assets',
    'support_v4_jar_paths': [
      # Path of support-v4 has been changed for new SDK.
      '<(android_sdk_home)/extras/android/compatibility/v4/android-support-v4.jar',
      '<(android_sdk_home)/extras/android/support/v4/android-support-v4.jar',
      ],
    'ndk_app_dst_dir': 'libs',
    'ndk_build_with_args': [
      'ndk-build',
      # Run make-jobs simultaneously.
      '--jobs=$(lastword 4 $(MAKE_JOBS))',  # Defaults to 4 parallel jobs.
      '-C', '<(DEPTH)',  # Change the current directory to the project root dir.
      'NDK_LOG=1',
      'APP_ABI=<(android_arch_abi)',  # Specified by build_mozc.py
      'APP_BUILD_SCRIPT=Makefile',
      # If target API level above 16, PIE (position-independent executables) is enabled by default.
      # But our minimum support level is 7 so PIE shouldn't be built.
      'APP_PIE=false',
      # Set the build mode according to CONFIGURATION_NAME.
      'NDK_DEBUG=$(if $(filter Release_Android,<(CONFIGURATION_NAME)),0,1)',
      # On OSS version,
      # find command returns the results like ./base/base.host.mk.
      # But NO_LOAD doesn't accept the entries starting with ./ so
      # remove them by filter script.
      'NO_LOAD=$(shell find  -name \'*.host.mk\' | python <(relative_dir)/filter_find_result.py)',
      # Link a static version of STLport.
      'APP_STL=stlport_static',
      # Do not invoke the default makefile target, which is usually 'all',
      # because the 'all' target in <(DEPTH)/Makefile is used by GYP.
      # Invoke the 'installed_modules' target instead, which is defined by
      # Android NDK.
      'installed_modules',
    ],
    # Tests and tools
    'native_test_small_targets': [
      # base
      'base_core_test',
      'base_init_test',
      'base_test',
      'encryptor_test',
      'file_util_test',
      'jni_proxy_test',
      'number_util_test',
      'scheduler_stub_test',
      'task_test',
      'trie_test',
      'util_test',
      # composer
      'composer_test',
      # config
      'character_form_manager_test',
      'config_handler_test',
      'stats_config_util_test',
      # converter
      'cached_connector_test',
      'converter_test',
      'converter_regression_test',
      'sparse_connector_test',
      # dictionary
      'dictionary_test',
      'text_dictionary_loader_test',
      # dictionary/file
      'codec_test',
      'dictionary_file_test',
      # dictionary/system
      'system_dictionary_builder_test',
      'system_dictionary_codec_test',
      'system_dictionary_test',
      'value_dictionary_test',
      # net
      'http_client_mock_test',
      # prediction
      'prediction_test',
      # rewriter
      'rewriter_test',
      'single_kanji_rewriter_test',
      # rewriter/calculator
      'calculator_test',
      # session
      'generic_storage_manager_test',
      'random_keyevents_generator_test',
      'request_test_util_test',
      'session_converter_test',
      'session_handler_scenario_test',
      'session_handler_test',
      'session_internal_test',
      'session_module_test',
      'session_regression_test',
      'session_test',
      # storage
      'encrypted_string_storage_test',
      'storage_test',
      # storage/louds
      'bit_stream_test',
      'bit_vector_based_array_test',
      'key_expansion_table_test',
      'louds_trie_test',
      'simple_succinct_bit_vector_index_test',
      # transliteration
      'transliteration_test',
      # usage_stats
      'usage_stats_test',
      'usage_stats_uploader_test',
    ],
    'native_test_large_targets': [
      # dictionary
      'system_dictionary_test',
      # session
      'session_handler_stress_test',
      'session_converter_stress_test',
    ],
    'native_tool_targets': [
      # net
      'http_client_main',
    ],
    'test_connection_data': '<(SHARED_INTERMEDIATE_DIR)/converter/test_connection_data.data',
    # e.g. xxxx/out_android/gtest_report
    'test_report_dir': '<(SHARED_INTERMEDIATE_DIR)/../../gtest_report',
  },
  'conditions': [
    ['branding=="GoogleJapaneseInput"', {
    }, {
      'variables': {
        # Currently dexmaker* and easymock* properties are not used.
        # TODO(matsuzakit): Support Java-side unit test.
        'dexmaker_jar_path': '<(DEPTH)/third_party/dexmaker/dexmaker-0.9.jar',
        # TODO(matsuzakit): Make copy_and_patch.py support non-jar file tree.
        'dexmaker_src_path': '<(DEPTH)/third_party/dexmaker/src/main/java',
        'easymock_jar_path': '<(DEPTH)/third_party/easymock/easymock-3_1.jar',
        # TODO(matsuzakit): Make copy_and_patch.py support non-jar file tree.
        'easymock_src_path': '<(DEPTH)/third_party/easymock/src/main/java',
        'dictionary_data': '<(SHARED_INTERMEDIATE_DIR)/data_manager/oss/system.dictionary',
        'connection_data': '<(SHARED_INTERMEDIATE_DIR)/data_manager/oss/connection_data.data',
        'native_test_small_targets': [
          'oss_data_manager_test',
        ],
        'resources_project_path': 'resources_oss',
      },
    }],
  ],
  'targets': [
    {
      'target_name': 'build_java_test',
      'type': 'none',
      'dependencies': [
        # This target should not depend on 'apk' target because 'ant run-tests'
        # command tries building the apk package, and gyp-generated build rules
        # may conflict it.  Let Ant take care of the build, but prepare things
        # that Ant doesn't know (code generation and native builds).
        'sdk_apk_dependencies',
        'build_jnitestingbackdoor_library',
      ],
      'actions': [
        {
          'action_name': 'build_java_test',
          'inputs': ['<(dummy_input_file)'],
          'outputs': ['dummy_java_test'],
          'action': [
            '../build_tools/run_after_chdir.py', 'tests', 'ant', 'debug',
          ],
        },
      ],
    },
    {
      'target_name': 'run_test',
      'type': 'none',
      'dependencies': [
        'build_java_test',
      ],
      'actions': [
        {
          'action_name': 'run_test',
          'inputs': ['<(dummy_input_file)'],
          'outputs': ['dummy_run_test'],
          'action': [
            'python', 'run_android_test.py',
            '--android_devices=$(ANDROID_DEVICES)',
            '--run_java_test',
            '--app_package_name=<(app_package_name)',
            '--configuration=<(CONFIGURATION_NAME)',
            '--output_report_dir=<(test_report_dir)',
          ],
        },
      ],
    },
    {
      'target_name': 'install',
      'type': 'none',
      'dependencies': [
        'apk',
      ],
      'actions': [
        {
          'action_name': 'install',
          'inputs': ['<(dummy_input_file)'],
          'outputs': ['dummy_install'],
          'action': [
            'ant', 'install', '-Dgyp.build_type=<(CONFIGURATION_NAME)',
          ],
        },
      ],
    },
    {
      'target_name': 'apk',
      'type': 'none',
      'dependencies': [
        'sdk_apk_dependencies',
      ],
      'actions': [
        {
          'action_name': 'apk',
          'inputs': [
            'AndroidManifest.xml',
            'build.xml',
            'project.properties',
            'ant.properties',
            'proguard-project.txt',
            # Protocol Buffer
            'protobuf/AndroidManifest.xml',
            'protobuf/build.xml',
            'protobuf/project.properties',
            'protobuf/ant.properties',
          ],
          # The actual output is one of
          #   'bin/GoogleJapaneseInput-debug.apk'
          #   'bin/GoogleJapaneseInput-release.apk'
          #   'bin/GoogleJapaneseInput-unsigned.apk'
          # depending on CONFIGURATION_NAME and/or key.store.
          'outputs': ['dummy_apk'],
          'action': [
            'ant', 'apk', '-Dgyp.build_type=<(CONFIGURATION_NAME)',
          ],
        },
      ],
    },
    {
      'target_name': 'sdk_apk_dependencies',
      'type': 'none',
      'dependencies': [
        'common_apk_dependencies',
        'genproto_java.gyp:sdk_genproto_java',
        'sdk_gen_emoji_data',
        'sdk_gen_emoticon_data',
        'sdk_gen_symbol_data',
      ],
    },
    {
      'target_name': 'adt_apk_dependencies',
      'type': 'none',
      'dependencies': [
        'adt_gen_emoji_data',
        'adt_gen_emoticon_data',
        'adt_gen_symbol_data',
        'build_jnitestingbackdoor_library',
        'common_apk_dependencies',
        'genproto_java.gyp:adt_genproto_java',
      ],
    },
    {
      'target_name': 'common_apk_dependencies',
      'type': 'none',
      'dependencies': [
        'android_manifest',
        'assets',
        'build_native_library',
        'gen_mozc_drawable',
        'support_v4_library',
      ],
    },
    {
      'target_name': 'android_manifest',
      'type': 'none',
      'actions': [
        {
          'action_name': 'android_manifest',
          'inputs': [
            '../build_tools/replace_version.py',
            '../mozc_version.txt',
            'AndroidManifest_template.xml',
          ],
          'outputs': [
            'AndroidManifest.xml',
          ],
          'action': [
            'python', '../build_tools/replace_version.py',
            '--version_file', '../mozc_version.txt',
            '--input', 'AndroidManifest_template.xml',
            '--output', 'AndroidManifest.xml',
          ],
        },
        {
          'action_name': 'android_test_manifest',
          'inputs': [
            '../build_tools/replace_version.py',
            '../mozc_version.txt',
            'tests/AndroidManifest_template.xml',
          ],
          'outputs': [
            'tests/AndroidManifest.xml',
          ],
          'action': [
            'python', '../build_tools/replace_version.py',
            '--version_file', '../mozc_version.txt',
            '--input', 'tests/AndroidManifest_template.xml',
            '--output', 'tests/AndroidManifest.xml',
          ],
        },
      ],
    },
    {
      'target_name': 'assets',
      'type': 'none',
      'dependencies': [
        'assets_connection_data',
        'assets_credits',
        'assets_dictionary',
        'assets_touch_stat_data',
      ]
    },
    {
      'target_name': 'assets_credits',
      'type': 'none',
      'copies': [{
        'destination': '<(sdk_asset_dir)',
        'files': [
          # Copies the copyright and credit info.
          '../data/installer/credits_en.html',
          '../data/installer/credits_ja.html',
        ],
      }],
    },
    {
      'target_name': 'assets_connection_data',
      'type': 'none',
      'conditions': [
        ['use_separate_connection_data==1',
          {
            'actions': [
              {
                'action_name': 'assets_copy_connection_data',
                'inputs': [
                  '<(connection_data)',
                ],
                'outputs': [
                  '<(sdk_asset_dir)/connection.data.imy',
                ],
                'action': [
                  # Note that multiple output files cannot be handled
                  # by copy_file script.
                  '<@(copy_file)', '<@(_inputs)', '<(_outputs)',
                ],
              },
            ],
          },
        ],
      ],
    },
    {
      'target_name': 'assets_dictionary',
      'type': 'none',
      'conditions': [
        ['use_separate_dictionary==1',
          {
            'actions': [
              {
                'action_name': 'assets_copy_dictionary',
                'inputs': [
                  '<(dictionary_data)'
                ],
                'outputs': [
                  '<(sdk_asset_dir)/system.dictionary.imy',
                ],
                'action': [
                  # Note that multiple output files cannot be handled
                  # by copy_file script.
                  '<@(copy_file)', '<@(_inputs)', '<(_outputs)',
                ],
              },
            ],
          },
        ],
      ],
    },
    {
      'target_name': 'sdk_gen_emoji_data',
      'type': 'none',
      'copies': [{
        'destination': '<(sdk_gen_dir)/org/mozc/android/inputmethod/japanese/emoji',
        'files': [
          '<(adt_gen_dir)/org/mozc/android/inputmethod/japanese/emoji/EmojiData.java',
        ],
      }],
    },
    {
      'target_name': 'adt_gen_emoji_data',
      'type': 'none',
      'actions': [
        {
          'action_name': 'gen_emoticon_data',
          'inputs': [
            '../data/emoji/emoji_data.tsv',
          ],
          'outputs': [
            '<(adt_gen_dir)/org/mozc/android/inputmethod/japanese/emoji/EmojiData.java',
          ],
          'action': [
            'python', 'gen_emoji_data.py',
            '--emoji_data', '<(_inputs)',
            '--output', '<(_outputs)',
          ],
        },
      ],
    },
    {
      'target_name': 'sdk_gen_emoticon_data',
      'type': 'none',
      'copies': [{
        'destination': '<(sdk_gen_dir)/org/mozc/android/inputmethod/japanese/',
        'files': [
          '<(adt_gen_dir)/org/mozc/android/inputmethod/japanese/EmoticonData.java',
        ],
      }],
    },
    {
      'target_name': 'adt_gen_emoticon_data',
      'type': 'none',
      'actions': [
        {
          'action_name': 'gen_emoticon_data',
          'inputs': [
            '../data/emoticon/categorized.tsv',
          ],
          'outputs': [
            '<(adt_gen_dir)/org/mozc/android/inputmethod/japanese/EmoticonData.java',
          ],
          'action': [
            'python', 'gen_emoticon_data.py',
            '--input', '<(_inputs)',
            '--output', '<(_outputs)',
            '--class_name', 'EmoticonData',
            '--value_column', '0',
            '--category_column', '1',
          ],
        },
      ],
    },
    {
      'target_name': 'sdk_gen_symbol_data',
      'type': 'none',
      'copies': [{
        'destination': '<(sdk_gen_dir)/org/mozc/android/inputmethod/japanese/',
        'files': [
          '<(adt_gen_dir)/org/mozc/android/inputmethod/japanese/SymbolData.java',
        ],
      }],
    },
    {
      'target_name': 'adt_gen_symbol_data',
      'type': 'none',
      'actions': [
        {
          'action_name': 'gen_symbol_data',
          'inputs': [
            '../data/symbol/categorized.tsv',
          ],
          'outputs': [
            '<(adt_gen_dir)/org/mozc/android/inputmethod/japanese/SymbolData.java',
          ],
          'action': [
            'python', 'gen_emoticon_data.py',
            '--input', '<(_inputs)',
            '--output', '<(_outputs)',
            '--class_name', 'SymbolData',
            '--value_column', '0',
            '--category_column', '1',
          ],
        },
      ],
    },
    {
      'target_name': 'assets_touch_stat_data',
      'type': 'none',
      'actions': [
        {
          'action_name': 'assets_touch_stat_data',
          'inputs': [
            '../data/typing/touch_event_stats.csv',
            'collected_keyboards.csv'
          ],
          'outputs': ['dummy_touch_stat_data'],
          'action': [
            'python', 'gen_touch_event_stats.py',
            '--output_dir', 'assets',
            '--stats_data', '../data/typing/touch_event_stats.csv',
            '--collected_keyboards', 'collected_keyboards.csv',
          ],
        },
      ],
    },
    # "copy" command tries to preserve the access permissions, which would
    # cause errors when we re-build the apk file by running
    # "build_mozc.py build". So, instead, we use our own copy command here.
    {
      'target_name': 'dexmaker_library',
      'type': 'none',
      'actions': [
        {
          'action_name': 'copy_dexmaker_library',
          'inputs': [
            '<(dexmaker_jar_path)',
          ],
          'outputs': [
            'tests/libs/dexmaker.jar'
          ],
          'action': [
            '<@(copy_file)',
            '<(_inputs)',
            'tests/libs',
          ]
        },
      ],
    },
    {
      'target_name': 'easymock_library',
      'type': 'none',
      'actions': [
        {
          'action_name': 'copy_easymock_library',
          'inputs': [
            '<(easymock_jar_path)',
          ],
          'outputs': [
            'tests/libs/easymock-3_1.jar'
          ],
          'action': [
            '<@(copy_file)',
            '<(_inputs)',
            'tests/libs',
          ]
        },
      ],
    },
    {
      'target_name': 'support_v4_library',
      'type': 'none',
      'actions': [
        {
          'action_name': 'copy_support_v4_library',
          'inputs': [
            '<(dummy_input_file)',
          ],
          'outputs': [
            'libs/android-support-v4.jar',
          ],
          'action': [
            '<@(copy_file)',
            '--ignore_existence_check',
            '<@(support_v4_jar_paths)',
            'libs',
          ]
        },
      ],
    },
    {
      'target_name': 'gen_mozc_drawable',
      'type': 'none',
      'actions': [
        {
          'action_name': 'generate_pic_files',
          'inputs': [
            '<(dummy_input_file)',
            'gen_mozc_drawable.py',
          ],
          'outputs': [
            'dummy_gen_mozc_drawable_output',
          ],
          'action': [
            'python', 'gen_mozc_drawable.py',
            '--svg_dir=../data/images/android/svg',
            '--output_dir=<(resources_project_path)/res/raw',
          ],
        },
      ],
    },
    {
      'target_name': 'build_native_library',
      'type': 'none',
      'dependencies': [
        'gen_native_build_deps#host',
      ],
      'actions': [
        {
          'action_name': 'build_native_library',
          'inputs': [
            '<(dummy_input_file)',
            'libmozc.map'
          ],
          'outputs': ['dummy_build_native_library'],
          'action': [
            '<@(ndk_build_with_args)',
            # A hack to invoke <(DEPTH)/Makefile as Android.mk being on <(DEPTH) as
            # the current working directory.
            'NDK_PROJECT_PATH=<(relative_dir)',
            'NDK_APPLICATION_MK=<(relative_dir)/Application.mk',
            'APP_MODULES=mozc',  # Build libmozc.so
            'LOCAL_LDFLAGS=-Wl,--version-script,<(relative_dir)/libmozc.map',
          ],
        },
      ],
    },
    {
      # Generates all the code which is necessary to build libmozc.so.
      #
      # The target 'build_native_library' invokes 'ndk-build' command of
      # Android NDK, which is a thin wrapper of 'make', and it builds
      # binaries (static libraries, shared libraries and executables)
      # for the target architectures.  However, ndk-build does not build
      # or run any other gyp targets, especially code generation targets
      # (this is a limitation of Gyp's Android NDK support).
      # So we need to generate all the code beforehand which libmozc.so
      # needs.
      #
      # Note: We need only code generation here and 'build_native_library'
      # compiles it for specified architectures.  This is the reason why
      # this target depends on genproto_xxx instead of xxx_protocol.
      'target_name': 'gen_native_build_deps',
      'type': 'none',
      'toolsets': ['host'],
      'dependencies': [
        '../base/base.gyp:gen_character_set',
        '../base/base.gyp:gen_config_file_stream_data',
        '../base/base.gyp:gen_version_def',
        '../config/config.gyp:genproto_config',
        '../dictionary/dictionary_base.gyp:gen_pos_map',
        '../dictionary/dictionary_base.gyp:genproto_dictionary',
        '../dictionary/dictionary_base.gyp:pos_matcher',
        '../ipc/ipc.gyp:genproto_ipc',
        '../prediction/prediction.gyp:gen_zero_query_number_data',
        '../prediction/prediction.gyp:genproto_prediction',
        '../rewriter/rewriter_base.gyp:gen_rewriter_files',
        '../session/session_base.gyp:genproto_session',
        '../usage_stats/usage_stats_base.gyp:gen_usage_stats_list',
        '../usage_stats/usage_stats_base.gyp:genproto_usage_stats',
      ],
      'conditions': [
        ['enable_typing_correction==1', {
          'defines': [
            'MOZC_ENABLE_TYPING_CORRECTION'
          ],
          'dependencies': [
            '../composer/composer.gyp:gen_typing_model',
          ],
        }],
        ['branding=="GoogleJapaneseInput"', {
          'dependencies': [
            '../data_manager/android/android_data_manager.gyp:gen_android_embedded_data#host',
            '../data_manager/android/android_data_manager.gyp:gen_connection_data_for_android#host',
            '../data_manager/android/android_data_manager.gyp:gen_dictionary_data_for_android#host',
            '../data_manager/android/android_data_manager.gyp:gen_embedded_symbol_rewriter_data_for_android#host',
          ]
        }, {
          'dependencies': [
            '../data_manager/oss/oss_data_manager.gyp:gen_oss_embedded_data#host',
            '../data_manager/oss/oss_data_manager.gyp:gen_connection_data_for_oss#host',
            '../data_manager/oss/oss_data_manager.gyp:gen_dictionary_data_for_oss#host',
            '../data_manager/oss/oss_data_manager.gyp:gen_embedded_symbol_rewriter_data_for_oss#host',
          ]
        }],
      ],
    },
    {
      'target_name': 'gen_native_test_deps',
      'type': 'none',
      'toolsets': ['host'],
      'dependencies': [
        '../testing/testing.gyp:gen_mozc_data_dir_header',
        'gen_native_build_deps',
      ],
      'conditions': [
        ['branding=="GoogleJapaneseInput"', {
          'dependencies': [
            '../data_manager/android/android_data_manager.gyp:gen_android_segmenter_inl_header#host',
          ]
        }, {
          'dependencies': [
            '../data_manager/oss/oss_data_manager.gyp:gen_oss_segmenter_inl_header#host',
          ]
        }],
      ],
    },
    {
      'target_name': 'mozc',  # libmozc.so
      'type': 'shared_library',
      'sources': [
        'jni/mozcjni.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../dictionary/dictionary.gyp:dictionary',
        '../session/session.gyp:session',
        '../session/session.gyp:session_handler',
        '../session/session.gyp:session_usage_observer',
        '../usage_stats/usage_stats.gyp:usage_stats_uploader',
      ],
      'conditions': [
        ['branding=="GoogleJapaneseInput"', {
          'dependencies': [
            '../data_manager/android/android_data_manager.gyp:android_data_manager',
            '../engine/engine.gyp:android_engine_factory',
          ]
        }, {
          'dependencies': [
            '../data_manager/oss/oss_data_manager.gyp:oss_data_manager',
            '../engine/engine.gyp:oss_engine_factory',
          ]
        }],
      ],
    },
    {
      'target_name': 'build_jnitestingbackdoor_library',
      'type': 'none',
      'actions': [
        {
          'action_name': 'build_jnitestingbackdoor_library',
          'inputs': [
            '<(dummy_input_file)',
          ],
          'outputs': ['dummy_build_jnitestingbackdoor_library'],
          'action': [
            '<@(ndk_build_with_args)',
            # A hack to invoke <(DEPTH)/Makefile as Android.mk being on <(DEPTH) as
            # the current working directory.
            'NDK_PROJECT_PATH=<(relative_dir)/tests',
            'NDK_APPLICATION_MK=<(relative_dir)/tests/Application.mk',
            'APP_MODULES=jnitestingbackdoor',  # Build libjnitestingbackdoor.so
          ],
        },
      ],
    },
    {
      'target_name': 'jnitestingbackdoor',  # libjnitestingbackdoor.so
      'type': 'shared_library',
      'sources': [
        'tests/jni/jnitestingbackdoor.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:http_client',
      ],
    },
    {
      'target_name': 'install_native_test_data',
      'type': 'none',
      'dependencies': [
        '../base/base_test.gyp:install_util_test_data',
        '../dictionary/system/system_dictionary.gyp:install_system_dictionary_test_data',
        '../config/config_test.gyp:install_stats_config_util_test_data',
        '../rewriter/calculator/calculator.gyp:install_calculator_test_data',
        '../session/session_test.gyp:install_session_handler_scenario_test_data',
        '../session/session_test.gyp:install_session_handler_usage_stats_scenario_test_data',
      ],
      'conditions': [
        ['branding=="GoogleJapaneseInput"', {
          'dependencies': [
            '../data_manager/android/android_data_manager_test.gyp:install_android_data_manager_test_data',
          ]
        }, {
          'dependencies': [
            '../data_manager/oss/oss_data_manager_test.gyp:install_oss_data_manager_test_data',
          ]
        }],
      ],
    },
    {
      'target_name': 'run_native_small_test',
      'type': 'none',
      'dependencies': [
        'build_native_small_test',
      ],
      'actions': [
        {
          'action_name': 'run_native_small_test',
          'inputs': ['<(dummy_input_file)'],
          'outputs': ['dummy_run_native_small_test'],
          'action': [
            'python', 'run_android_test.py',
            '--android_devices=$(ANDROID_DEVICES)',
            '--run_native_test',
            '--test_bin_dir=<(ndk_app_dst_dir)',
            # Make comma-separated test targets.
            '--test_case='
            '<!(<(python_executable)'
            ' -c "import sys; print \\",\\".join(sys.argv[1:])"'
            ' <(native_test_small_targets))',
            '--mozc_dictionary_data_file=<(dictionary_data)',
            '--mozc_connection_data_file=<(connection_data)',
            '--mozc_test_connection_data_file=<(test_connection_data)',
            '--mozc_data_dir=<(mozc_data_dir)',
            '--output_report_dir=<(test_report_dir)',
          ],
        },
      ],
    },
    {
      'target_name': 'build_native_small_test',
      'type': 'none',
      'dependencies': [
        # Generates all the code which is necessary to build unit tests.
        # See the comment of 'gen_native_build_deps'.
        '../converter/converter_test.gyp:generate_test_connection_data_image',
        '../data_manager/testing/mock_data_manager.gyp:gen_mock_embedded_data#host',
        '../session/session.gyp:gen_session_stress_test_data#host',
        'gen_native_test_deps#host',
        'install_native_test_data',
      ],
      'actions': [
        {
          'action_name': 'build_native_small_test',
          'inputs': ['<(dummy_input_file)'],
          'outputs': ['dummy_build_native_small_test'],
          'action': [
            '<@(ndk_build_with_args)',
            # TODO(matsuzakit): '<(relative_dir)/tests' might be better.
            'NDK_PROJECT_PATH=<(relative_dir)',
            'APP_MODULES=<(native_test_small_targets)',
          ],
        },
      ],
    },
    {
      'target_name': 'run_native_large_test',
      'type': 'none',
      'dependencies': [
        'build_native_large_test',
      ],
      'actions': [
        {
          'action_name': 'run_native_large_test',
          'inputs': ['<(dummy_input_file)'],
          'outputs': ['dummy_run_native_large_test'],
          'action': [
            'python', 'run_android_test.py',
            '--android_devices=$(ANDROID_DEVICES)',
            '--run_native_test',
            '--test_bin_dir=<(ndk_app_dst_dir)',
            # Make comma-separated test targets.
            '--test_case='
            '<!(<(python_executable)'
            ' -c "import sys; print \\",\\".join(sys.argv[1:])"'
            ' <(native_test_large_targets))',
            '--mozc_connection_data_file=<(connection_data)',
            '--mozc_test_connection_data_file=<(test_connection_data)',
            '--mozc_dictionary_data_file=<(dictionary_data)',
            '--mozc_data_dir=<(mozc_data_dir)',
            '--output_report_dir=<(test_report_dir)',
          ],
        },
      ],
    },
    {
      'target_name': 'build_native_large_test',
      'type': 'none',
      'dependencies': [
        # Generates all the code which is necessary to build unit tests.
        # See the comment of 'gen_native_build_deps'.
        '../session/session.gyp:gen_session_stress_test_data#host',
        'gen_native_test_deps#host',
        'install_native_test_data',
      ],
      'actions': [
        {
          'action_name': 'build_native_large_test',
          'inputs': ['<(dummy_input_file)'],
          'outputs': ['dummy_build_native_large_test'],
          'action': [
            '<@(ndk_build_with_args)',
            # TODO(matsuzakit): '<(relative_dir)/tests' might be better.
            'NDK_PROJECT_PATH=<(relative_dir)',
            'APP_MODULES=<(native_test_large_targets)',
          ],
        },
      ],
    },
    {
      'target_name': 'build_native_tool',
      'type': 'none',
      'dependencies': [
        'gen_native_build_deps#host',
      ],
      'actions': [
        {
          'action_name': 'build_native_tool',
          'inputs': ['<(dummy_input_file)'],
          'outputs': ['dummy_build_native_tool'],
          'action': [
            '<@(ndk_build_with_args)',
            # TODO(matsuzakit): '<(relative_dir)/tests' might be better.
            'NDK_PROJECT_PATH=<(relative_dir)',
            'APP_MODULES=<(native_tool_targets)',
          ],
        },
      ],
    },
  ],
}
