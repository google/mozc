# Copyright 2010-2011, Google Inc.
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
    'relative_dir': 'chrome/skk',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
    # Actions with an existing input and non-existing output behave like
    # phony rules.  Nothing matters for an input but its existence, so
    # we use 'skk.gyp' as a dummy input since it must exist.
    'dummy_input_file': 'skk.gyp',
    # GYP's 'copies' rule cannot change the destination file name, so we use
    # our own script to copy files.
    'copy_file': ['python', '../../build_tools/copy_file.py'],
    # "Release" and "Debug" are expected for <(CONFIGURATION_NAME).
    'configuration_nacl_i686': '<(CONFIGURATION_NAME)_NaCl_i686',
    'configuration_nacl_x86_64': '<(CONFIGURATION_NAME)_NaCl_x86-64',
    'conditions': [
      ['OS=="linux"', {
        # We need to copy binaries for NaCl i686 and x86-64 into a single skk
        # directory across configurations.  The following variables must be
        # the same as <(PRODUCT_DIR) for each configuration.
        # These values are dependent on Gyp's make generator.
        'product_nacl_i686_dir': '<(DEPTH)/$(builddir_name)/<(configuration_nacl_i686)',
        'product_nacl_x86_64_dir': '<(DEPTH)/$(builddir_name)/<(configuration_nacl_x86_64)',
      }, {
        # TODO(team): defines product_nacl_{i686,x86_64}_dir for other Gyp
        # generators.
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'skk',
      'type': 'none',
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)',
          'files': [
            '<(gen_out_dir)/skk.crx',
            '<(gen_out_dir)/skk.pem',
          ],
        },
      ],
    },
    {
      'target_name': 'pack_crx',
      'type': 'none',
      'actions': [
        {
          'action_name': 'pack_crx',
          'inputs': [
            '<(gen_out_dir)/skk/ascii_mode.js',
            '<(gen_out_dir)/skk/composer.js',
            '<(gen_out_dir)/skk/dict.js',
            '<(gen_out_dir)/skk/history.js',
            '<(gen_out_dir)/skk/ime.js',
            '<(gen_out_dir)/skk/ime_mode_interface.js',
            '<(gen_out_dir)/skk/kana_mode.js',
            '<(gen_out_dir)/skk/kanji_mode.js',
            '<(gen_out_dir)/skk/manifest.json',
            '<(gen_out_dir)/skk/skk.html',
            '<(gen_out_dir)/skk/skk_dict.nmf',
            '<(gen_out_dir)/skk/skk_dict_i686.nexe',
            '<(gen_out_dir)/skk/skk_dict_x86_64.nexe',
            '<(gen_out_dir)/skk/util.js',
          ],
          'outputs': [
            '<(gen_out_dir)/skk.crx',
            '<(gen_out_dir)/skk.pem',
          ],
          'action': [
            'google-chrome',
            '--pack-extension=<(gen_out_dir)/skk',
            # With --pack-extension-key=skk.pem option, we should use the same
            # private key to sign the extension.  Otherwise, a new packed
            # extension looks a different extension.
            # Of course, the private key must be placed in a secure manner.
            '--enable-experimental-extension-apis',
            '--no-message-box',
          ],
        },
      ],
    },
    {
      'target_name': 'gather_extension_files',
      'type': 'none',
      'copies': [{
        'destination': '<(gen_out_dir)/skk',
        'files': [
          'ascii_mode.js',
          'composer.js',
          'dict.js',
          'history.js',
          'ime.js',
          'ime_mode_interface.js',
          'kana_mode.js',
          'kanji_mode.js',
          'manifest.json',
          'skk.html',
          'skk_dict.nmf',
          'util.js',
        ],
      }],
    },
    {
      'target_name': 'copy_skk_dict_nexe',
      'type': 'none',
      'actions': [
        {
          'action_name': 'copy_skk_dict_i686',
          'inputs': ['<(gen_out_dir)/build_skk_dict_nexe_done'],
          'outputs': ['<(gen_out_dir)/skk/skk_dict_i686.nexe'],
          'action': [
            '<@(copy_file)',
            '--reference=<(gen_out_dir)/build_skk_dict_nexe_done',
            '<(product_nacl_i686_dir)/skk_dict.nexe',
            '<(gen_out_dir)/skk/skk_dict_i686.nexe',
          ],
        },
        {
          'action_name': 'copy_skk_dict_x86_64',
          'inputs': ['<(gen_out_dir)/build_skk_dict_nexe_done'],
          'outputs': ['<(gen_out_dir)/skk/skk_dict_x86_64.nexe'],
          'action': [
            '<@(copy_file)',
            '--reference=<(gen_out_dir)/build_skk_dict_nexe_done',
            '<(product_nacl_x86_64_dir)/skk_dict.nexe',
            '<(gen_out_dir)/skk/skk_dict_x86_64.nexe',
          ],
        },
      ],
      'dependencies': [
        'build_skk_dict_nexe',
      ],
    },
    {
      'target_name': 'build_skk_dict_nexe',
      'type': 'none',
      'actions': [
        {
          'action_name': 'build_skk_dict_nexe',
          'inputs': ['<(dummy_input_file)'],
          'outputs': ['dummy_build_skk_dict_nexe',
                      '<(gen_out_dir)/build_skk_dict_nexe_done'],
          'action': [
            'python', '../build_for_nacl.py',
            '--build_base=<(build_base)',
            '--depth=<(DEPTH)',
            '--nacl_sdk_root=<(nacl_sdk_root)',
            '--target_settings='
            '[{"configuration": "<(configuration_nacl_i686)",'
            '  "toolchain_dir": "linux_x86",'
            '  "toolchain_prefix": "i686-nacl-"},'
            ' {"configuration": "<(configuration_nacl_x86_64)",'
            '  "toolchain_dir": "linux_x86",'
            '  "toolchain_prefix": "x86_64-nacl-"}]',
            '--touch_when_done=<(gen_out_dir)/build_skk_dict_nexe_done',
            'chrome/skk/skk_dict.gyp:skk_dict.nexe',
          ],
        },
      ],
    },
  ],
}
