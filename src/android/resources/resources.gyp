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
  'variables': {
    'relative_dir': 'android/resources',
    'abs_android_dir': '<(abs_depth)/<(relative_dir)',
    # Actions with an existing input and non-existing output behave like
    # phony rules.  Nothing matters for an input but its existence, so
    # we use 'resources.gyp' as a dummy input since it must exist.
    'dummy_input_file': 'resources.gyp',
    # GYP's 'copies' rule cannot copy a whole directory recursively, so we use
    # our own script to copy files.
    'copy_file': ['python', '../../build_tools/copy_file.py'],
    'shared_intermediate_mozc_dir': '<(SHARED_INTERMEDIATE_DIR)/',
    'static_resources_dir': '<(abs_depth)/android/static_resources',
    'sdk_resources_dir': '<(shared_intermediate_mozc_dir)/android/resources',
  },
  'conditions': [
    ['branding=="GoogleJapaneseInput"', {
      'conditions': [
        ['android_hide_icon==1', {
          'variables': {
            'launcher_icon_bools': '<(static_resources_dir)/launcher_icon_resources/launcher_icon_preinstall_bools.xml',
          },
        }, { # else
          'variables': {
            'launcher_icon_bools': '<(static_resources_dir)/launcher_icon_resources/launcher_icon_standard_bools.xml',
          },
        }],
        ['android_release_icon==1', {
          'variables': {
            'application_icon_dir': '<(static_resources_dir)/application_icon/release_icon/',
          },
        }, {  # else
          'variables': {
            'application_icon_dir': '<(static_resources_dir)/application_icon/dogfood_icon/',
          },
        }],
      ],
      'variables': {
        'original_sdk_resources_dir': '<(static_resources_dir)/resources',
      },
    }, {  # 'branding!="GoogleJapaneseInput"'
      'variables': {
        'launcher_icon_bools': '<(static_resources_dir)/launcher_icon_resources/launcher_icon_standard_bools.xml',
        'original_sdk_resources_dir': '<(static_resources_dir)/resources_oss',
        'application_icon_dir': '<(static_resources_dir)/application_icon/oss_icon/',
      },
    }],
  ],
  'targets': [
    {
      'target_name': 'resources',
      'type': 'none',
      'dependencies': [
        'resources_project',
      ],
      'actions': [
        {
          'action_name': 'build_resources',
          'inputs': [
            'AndroidManifest.xml',
            'build.xml',
            'project.properties',
            'ant.properties',
            'proguard-project.txt',
          ],
          'outputs': [
            'bin/classes.jar',
            'gen/org/mozc/android/inputmethod/japanese/resources/R.java',
          ],
          'includes': ['../ant.gypi'],
        },
      ],
    },
    {
      'target_name': 'resources_project',
      'type': 'none',
      'dependencies': [
        'copy_resources',
        'gen_kcm_data',
        'gen_mozc_drawable',
      ],
    },
    {
      # Copies original resources to intermediate directory.
      # Then make symbolic link from android/resources to the intermediate directory.
      # The symbolic link is required to build both by ADT and ant.
      'target_name': 'copy_resources',
      'type': 'none',
      'includes': ['../android_resources.gypi'],
      'inputs': ['<@(android_resources)'],
      'outputs': ['dummy_copy_resources'],
      'actions': [
        {
          'action_name': 'copy_files',
          'inputs': ['<@(android_resources)'],
          'outputs': ['dummy_copy_files'],
          'action': [
            '<@(copy_file)', '-pr',
            '<@(android_resources)',
            '<(sdk_resources_dir)',
            '--src_base', '<(original_sdk_resources_dir)',
          ],
        },
        {
          # Make sure the link of resources/res does not exist.
          # the new link is created in the next step.
          'action_name': 'remove_symbolic_link',
          'inputs': [
            'dummy_copy_files'
          ],
          'outputs': [
            'dummy_remove_symbolic_link',
          ],
          'action': [
            'rm', '-f', 'res',
          ],
        },
        {
          'action_name': 'make_symbolic_link',
          'inputs': [
            'dummy_remove_symbolic_link'
          ],
          'outputs': [
            'dummy_make_symbolic_link',
          ],
          'action': [
            'ln', '-s', '-f',
            '<(sdk_resources_dir)/res',
            'res',
          ],
        },
        {
          'action_name': 'copy_configuration_dependent_resources',
          'inputs': [
            'dummy_make_symbolic_link',
          ],
          'outputs': [
            'dummy_copy_configuration_dependent_resources',
          ],
          'action': [
            'ln', '-s', '-f',
            '<(launcher_icon_bools)',
            '<(sdk_resources_dir)/res/values/',
          ],
        },
        {
          'action_name': 'copy_application_icons',
          'variables': {
            'icon_files': [
              '<(application_icon_dir)/drawable-hdpi/application_icon.png',
              '<(application_icon_dir)/drawable-mdpi/application_icon.png',
              '<(application_icon_dir)/drawable-xhdpi/application_icon.png',
              '<(application_icon_dir)/drawable-xxhdpi/application_icon.png',
              '<(application_icon_dir)/drawable-xxxhdpi/application_icon.png',
            ],
          },
          'inputs': [
            'dummy_make_symbolic_link',
            '<@(icon_files)',
          ],
          'outputs': [
            '<(sdk_resources_dir)/res/drawable-hdpi/application_icon.png',
            '<(sdk_resources_dir)/res/drawable-mdpi/application_icon.png',
            '<(sdk_resources_dir)/res/drawable-xhdpi/application_icon.png',
            '<(sdk_resources_dir)/res/drawable-xxhdpi/application_icon.png',
            '<(sdk_resources_dir)/res/drawable-xxxhdpi/application_icon.png',
          ],
          'action': ['<@(copy_file)', '-pr',
                     '<@(icon_files)',
                     '<(sdk_resources_dir)/res/',
                     '--src_base', '<(application_icon_dir)'],
        },
      ],
    },
    {
      'target_name': 'gen_kcm_data',
      'type': 'none',
      'dependencies': ['copy_resources'],
      'copies': [{
        'destination': '<(sdk_resources_dir)/res/raw',
        'files': [
          '../../data/android/keyboard_layout_japanese109a.kcm',
        ],
      }],
    },
    {
      # This (and 'copy_asis_svg') tareget outputs single .zip file
      # in order to make build system work correctly.
      # Without this hack, all the names of the generated files
      # should be listed up in 'inputs' and/or 'outputs' field.
      'target_name': 'transform_template_svg',
      'type': 'none',
      'actions': [
        {
          'action_name': 'transform_template_svg',
          'inputs': [
            '../../data/images/android/template/transform.py',
          ],
          'outputs': [
            '<(shared_intermediate_mozc_dir)/data/images/android/svg/transformed.zip',
          ],
          'conditions': [
            ['OS=="linux" or OS=="mac"', {
              'inputs': ['<!@(ls -1 ../../data/images/android/template/*.svg)']
            }, {
              # TODO: Support Windows.
              'inputs': ['<(dummy_input_file)']
            }],
          ],
          'action': [
            'python', '../../data/images/android/template/transform.py',
            '--input_dir',
            '<(abs_depth)/data/images/android/template',
            '--output_zip', '<@(_outputs)',
          ],
        },
      ],
    },
    {
      'target_name': 'copy_asis_svg',
      'type': 'none',
      'actions': [
        {
          'action_name': 'archive_asis_svg',
          'outputs': [
            '<(shared_intermediate_mozc_dir)/data/images/android/svg/asis.zip',
          ],
          'conditions': [
            ['OS=="linux" or OS=="mac"', {
              'inputs': ['<!@(ls -1 ../../data/images/android/svg/*.svg)']
            }, {
              # TODO: Support Windows.
              'inputs': ['<(dummy_input_file)']
            }],
          ],
          'action': [
            # TODO: Support Windows.
            'zip',
            '-q',  # quiet operation
            '-1',  # compress faster
            '-j',  # don't record directory names
            '<@(_outputs)',
            '<@(_inputs)',
          ],
        },
      ],
    },
    {
      'target_name': 'gen_mozc_drawable',
      'type': 'none',
      'dependencies': [
        'copy_asis_svg',
        'transform_template_svg',
      ],
      'actions': [
        {
          'action_name': 'generate_pic_files',
          'inputs': [
            '<(shared_intermediate_mozc_dir)/data/images/android/svg/asis.zip',
            '<(shared_intermediate_mozc_dir)/data/images/android/svg/transformed.zip',
            '../gen_mozc_drawable.py',
          ],
          'outputs': [
            '<(shared_intermediate_mozc_dir)/data/images/android/svg/gen_mozc_drawable.log',
          ],
          'dependencies': ['copy_resources'],
          'action': [
            'python', '../gen_mozc_drawable.py',
            '--svg_paths=<(shared_intermediate_mozc_dir)/data/images/android/svg/asis.zip,<(shared_intermediate_mozc_dir)/data/images/android/svg/transformed.zip',
            '--output_dir=<(sdk_resources_dir)/res/raw',
            '--build_log=<(shared_intermediate_mozc_dir)/data/images/android/svg/gen_mozc_drawable.log',
          ],
        },
      ],
    },
  ],
}
