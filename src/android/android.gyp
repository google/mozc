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
# of build_mozc.py (Debug or Release).
#
# For signing, neither key.store nor key.alias in project.properties is set,
# and the default debug key is used.  You need to set them in local.properties
# (recommended) or project.properties when you need to sign the package with
# the release key.
{
  'includes': ['android_env.gypi'],
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
        'jnitestingbackdoor',
      ],
      'actions': [
        {
          'action_name': 'build_java_test',
          'inputs': ['<(dummy_input_file)'],
          'outputs': ['dummy_java_test'],
          # TODO(komatsu): use ant.gypi when the build rule is moved to tests/.
          'ninja_use_console': 1,
          'action': [
            '../build_tools/run_after_chdir.py', 'tests',
            'ant',
            'debug',
            '-Dsdk.dir=<(android_home)',
          ],
        },
      ],
    },
    {
      'target_name': 'run_java_test',
      'type': 'none',
      'actions': [
        {
          'action_name': 'run_java_test',
          'inputs': ['<(dummy_input_file)'],
          'outputs': ['dummy_run_test'],
          'action': [
            'python', 'run_android_test.py',
            '--android_home=<(android_home)',
            '--run_java_test',
            '--app_package_name=<(app_package_name)',
            '--configuration=<(CONFIGURATION_NAME)',
            '--native_abi=<(abi)',
            '--output_report_dir=<(test_report_dir)',
            '--run_java_test',
          ],
        },
      ],
    },
    {
      'target_name': 'apk',
      'type': 'none',
      'dependencies': [
        'resources/resources.gyp:resources',
        'sdk_apk_dependencies',
        'userfeedback/userfeedback.gyp:userfeedback',
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
          ],
          # The actual output is one of
          #   'bin/GoogleJapaneseInput-debug.apk'
          #   'bin/GoogleJapaneseInput-release.apk'
          #   'bin/GoogleJapaneseInput-unsigned.apk'
          # depending on CONFIGURATION_NAME and/or key.store.
          'outputs': ['dummy_apk'],
          'includes': ['ant.gypi'],
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
        'jnitestingbackdoor',
        'common_apk_dependencies',
        'genproto_java.gyp:adt_genproto_java',
      ],
    },
    {
      'target_name': 'common_apk_dependencies',
      'type': 'none',
      'dependencies': [
        '../protobuf/protobuf.gyp:protobuf_jar',
        'android_manifest',
        'assets',
        'mozc',
        'guava_library',
        'userfeedback/userfeedback.gyp:userfeedback_project',
        'resources/resources.gyp:resources_project',
        'support_libraries',
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
            '--branding', '<(branding)',
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
            '--branding', '<(branding)',
          ],
        },
      ],
    },
    {
      'target_name': 'assets',
      'type': 'none',
      'dependencies': [
        'assets_credits',
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
        ],
      }],
    },
    {
      # CAVEAT:
      # This target is not actually used for build but placed here just to
      # execute 'make-standalone-toolchain.sh' in GYP evaluation phase as a
      # side-effect.
      # Do not evaluate this target unless target_platform=="Android".
      # Note that this .gyp file is evaluated only when
      # target_platform=="Android". c.f., GetGypFileNames in build_mozc.py
      'target_name': 'make_standalone_toolchain',
      'toolsets': ['host'],
      'type': 'none',
      'variables': {
        'make_standalone_toolchain_commands': [
          'python',
          '<(android_ndk_home)/build/tools/make_standalone_toolchain.py',
          '--force',
          '--arch=<(android_arch)',
          '--stl=libc++',
          '--install-dir=<(mozc_build_tools_dir)/ndk-standalone-toolchain/<(android_arch)',
          '--api=<(ndk_target_api_level)',
        ],
        'make_standalone_toolchain_result': '<!(<(make_standalone_toolchain_commands))',
      },
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
            '--output_dir', '<(sdk_asset_dir)',
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
      'target_name': 'support_libraries',
      'type': 'none',
      'actions': [
        {
          'action_name': 'copy_support_v13_library',
          'inputs': [
            '<(dummy_input_file)',
          ],
          'outputs': [
            'libs/android-support-v13.jar',
          ],
          'action': [
            '<@(copy_file)',
            '--ignore_existence_check',
            '<@(support_v13_jar_paths)',
            'libs',
          ]
        },
      ],
    },
    {
      'target_name': 'guava_library',
      'type': 'none',
      'actions': [
        {
          'action_name': 'copy_guava_library',
          'inputs': [
            '<(guava_jar_path)',
          ],
          'outputs': [
            'libs/guava.jar',
          ],
          'action': [
            '<@(copy_file)',
            '<(_inputs)',
            'libs/guava.jar',
          ]
        },
        {
          'action_name': 'copy_guava_test_library',
          'inputs': [
            '<(guava_testlib_jar_path)',
          ],
          'outputs': [
            'tests/libs/guava-testlib.jar',
          ],
          'action': [
            '<@(copy_file)',
            '<(_inputs)',
            'tests/libs/guava-testlib.jar',
          ]
        },
        {
          # Needed for Guava libarary.
          'action_name': 'copy_jr305_library',
          'inputs': [
            '<(jsr305_jar_path)',
          ],
          'outputs': [
            'libs/jsr305.jar',
          ],
          'action': [
            '<@(copy_file)',
            '<(_inputs)',
            'libs/jsr305.jar',
          ]
        },
      ],
    },
    {
      # The final artifact of the native layer, libmozc.so.
      # Gyp generates executable artifact to <(PRODUCT_DIR),
      # but shared libraries are generated to local intermediate directory
      # (e.g., out_andorid/Debug/obj.target/....)
      # which cannot be accessed from other targets.
      # We use libmozc.so to build the .apk so explicitly set the destination
      # through 'product_dir' field.
      'target_name': 'mozc',  # libmozc.so
      'type': 'shared_library',
      'sources': [
        'jni/mozcjni.cc',
      ],
      'defines': [
        'MOZC_USE_CUSTOM_DATA_MANAGER',
      ],
      'product_dir': '<(abs_android_dir)/libs/<(abi)',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:jni_proxy',
        '../data_manager/data_manager_base.gyp:data_manager',
        '../data_manager/oss/oss_data_manager.gyp:oss_data_manager',
        '../dictionary/dictionary.gyp:dictionary',
        '../engine/engine.gyp:engine',
        '../engine/engine.gyp:engine_builder',
        '../engine/engine.gyp:minimal_engine',
        '../session/session.gyp:session',
        '../session/session.gyp:session_handler',
        '../session/session.gyp:session_usage_observer',
        '../usage_stats/usage_stats.gyp:usage_stats_uploader',
      ],
      'ldflags': [
         # -s: Strip unused symbols
         # --version-script: Remove almost all exportable symbols
         '-Wl,-s,--version-script,<(abs_android_dir)/libmozc.lds',
      ],
    },
    {
      'target_name': 'jnitestingbackdoor',  # libjnitestingbackdoor.so
      'type': 'shared_library',
      'sources': [
        'tests/jni/jnitestingbackdoor.cc',
      ],
      'product_dir': '<(abs_android_dir)/tests/libs/<(abi)',
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
        '../data_manager/oss/oss_data_manager_test.gyp:install_oss_data_manager_test_data',
        '../data_manager/testing/mock_data_manager_test.gyp:install_test_connection_txt',
        '../config/config_test.gyp:install_stats_config_util_test_data',
        '../rewriter/calculator/calculator.gyp:install_calculator_test_data',
        '../data/test/session/scenario/scenario.gyp:install_session_handler_scenario_test_data',
        '../data/test/session/scenario/usage_stats/usage_stats.gyp:install_session_handler_usage_stats_scenario_test_data',
      ],
    },
    {
      'target_name': 'run_native_test',
      'type': 'none',
      'actions': [
        {
          'action_name': 'run_native_test',
          'inputs': ['<(dummy_input_file)'],
          'outputs': ['dummy_run_native_small_test'],
          'action': [
            'python', 'run_android_test.py',
            '--android_home=<(android_home)',
            '--mozc_connection_text_data_file=<(connection_text_data)',
            '--mozc_data_dir=<(mozc_data_dir)',
            '--mozc_dataset_file=<(mozc_dataset)',
            '--mozc_test_dataset_file=<(test_mozc_dataset)',
            '--mozc_test_connection_text_data_file=<(test_connection_text_data)',
            '--native_abi=<(abi)',
            '--output_report_dir=<(test_report_dir)',
            '--run_native_test',
            '--test_bin_dir=<(PRODUCT_DIR)',
          ],
        },
      ],
    },
  ],
}
