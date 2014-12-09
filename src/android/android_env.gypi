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
  'variables': {
    'app_package_name': '<(android_application_id)',
    'relative_dir': 'android',
    'abs_android_dir': '<(abs_depth)/<(relative_dir)',
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
    'support_v13_jar_paths': [
      # Path of support-v13 has been changed for new SDK. Try both.
      '<(android_home)/extras/android/compatibility/v13/android-support-v13.jar',
      '<(android_home)/extras/android/support/v13/android-support-v13.jar',
    ],
    'shared_intermediate_mozc_dir': '<(SHARED_INTERMEDIATE_DIR)/',
    'test_connection_data': '<(shared_intermediate_mozc_dir)/data_manager/testing/connection_data.data',
    'test_connection_text_data': '<(shared_intermediate_mozc_dir)/data_manager/testing/connection_single_column.txt',
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
        'guava_jar_path': '<(DEPTH)/third_party/guava/guava-jdk5-13.0.jar',
        'guava_testlib_jar_path': '<(DEPTH)/third_party/guava/guava-testlib-jdk5-13.0.jar',
        'jsr305_jar_path': '<(DEPTH)/third_party/findbug/jsr305-2.0.2.jar',
        'dictionary_data': '<(shared_intermediate_mozc_dir)/data_manager/oss/system.dictionary',
        'connection_data': '<(shared_intermediate_mozc_dir)/data_manager/oss/connection_data.data',
        'connection_text_data': '<(shared_intermediate_mozc_dir)/data_manager/oss/connection_single_column.txt',
        'native_test_small_targets': [
          'oss_data_manager_test',
        ],
        'resources_project_path': 'static_resources/resources_oss',
      },
    }],
    ['android_arch=="arm"', {
      'variables': {
        'abi': 'armeabi-v7a',
        'toolchain': 'arm-linux-androideabi-4.9',
        'platform': 'android-14',
      },
    }],
    ['android_arch=="x86"', {
      'variables': {
        'abi': 'x86',
        'toolchain': 'x86-4.9',
        'platform': 'android-14',
      },
    }],
    ['android_arch=="mips"', {
      'variables': {
        'abi': 'mips',
        'toolchain': 'mipsel-linux-android-4.9',
        'platform': 'android-14',
      },
    }],
  ],
}
