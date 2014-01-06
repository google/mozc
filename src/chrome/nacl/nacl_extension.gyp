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

{
  'variables': {
    'relative_dir': 'chrome/nacl',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
    'dummy_input_file': 'nacl_extension.gyp',
    # You should get browser_tester file from following URL to run end-to-end
    # test.
    # http://src.chromium.org/viewvc/chrome/trunk/src/ppapi/native_client/tools/
    'browser_tester_dir': '../../third_party/browser_tester',
    'nacl_mozc_files': [
      '<(gen_out_dir)/nacl_mozc/_locales/en/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/ja/messages.json',
      '<(gen_out_dir)/nacl_mozc/credits_en.html',
      '<(gen_out_dir)/nacl_mozc/credits_ja.html',
      '<(gen_out_dir)/nacl_mozc/dictionary_tool.js',
      '<(gen_out_dir)/nacl_mozc/key_translator.js',
      '<(gen_out_dir)/nacl_mozc/manifest.json',
      '<(gen_out_dir)/nacl_mozc/nacl_mozc.html',
      '<(gen_out_dir)/nacl_mozc/nacl_mozc.js',
      '<(gen_out_dir)/nacl_mozc/nacl_mozc_init.js',
      '<(gen_out_dir)/nacl_mozc/nacl_mozc_version.js',
      '<(gen_out_dir)/nacl_mozc/nacl_session_handler.nmf',
      '<(gen_out_dir)/nacl_mozc/nacl_session_handler_x86_32.nexe',
      '<(gen_out_dir)/nacl_mozc/nacl_session_handler_x86_64.nexe',
      '<(gen_out_dir)/nacl_mozc/nacl_session_handler_arm.nexe',
      '<(gen_out_dir)/nacl_mozc/product_icon_32bpp-128.png',
      '<(gen_out_dir)/nacl_mozc/option_page.js',
      '<(gen_out_dir)/nacl_mozc/options.css',
      '<(gen_out_dir)/nacl_mozc/options.html',
      '<(gen_out_dir)/nacl_mozc/options.js',
    ],
    'partial_supported_messages': [
      '<(gen_out_dir)/nacl_mozc/_locales/am/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/ar/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/bg/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/bn/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/ca/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/cs/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/da/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/de/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/el/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/en_GB/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/es_419/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/es/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/et/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/fa/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/fil/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/fi/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/fr/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/gu/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/hi/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/hr/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/hu/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/id/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/it/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/iw/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/kn/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/ko/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/lt/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/lv/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/ml/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/mr/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/ms/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/nl/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/no/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/pl/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/pt_BR/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/pt_PT/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/ro/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/ru/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/sk/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/sl/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/sr/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/sv/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/sw/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/ta/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/te/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/th/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/tr/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/uk/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/vi/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/zh_CN/messages.json',
      '<(gen_out_dir)/nacl_mozc/_locales/zh_TW/messages.json',
    ],
    'conditions': [
      ['branding=="GoogleJapaneseInput"', {
        'nacl_mozc_files': [
          '<(gen_out_dir)/nacl_mozc/zipped_data_chromeos',
          '<@(partial_supported_messages)',
        ],
      }],
      ['branding=="Mozc"', {
        'nacl_mozc_files': [
          '<(gen_out_dir)/nacl_mozc/zipped_data_oss',
        ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'translate_nacl_session_handler',
      'sources': [
        '<(PRODUCT_DIR)/nacl_session_handler',
      ],
      'includes': ['pnacl_translate.gypi'],
    },
    {
      'target_name': 'nacl_session_handler',
      'type': 'executable',
      'sources': [
        'nacl_session_handler.cc',
      ],
      'link_settings': {
        'libraries': ['-lppapi', '-lppapi_cpp'],
      },
      'dependencies': [
        'dictionary_downloader',
        '../../base/base.gyp:base',
        '../../config/config.gyp:config_handler',
        '../../config/config.gyp:config_protocol',
        '../../dictionary/dictionary_base.gyp:user_dictionary',
        '../../dictionary/dictionary_base.gyp:user_pos',
        '../../net/net.gyp:http_client',
        '../../net/net.gyp:json_util',
        '../../session/session_base.gyp:key_parser',
        '../../session/session_base.gyp:session_protocol',
        '../../session/session.gyp:session',
        '../../session/session.gyp:session_handler',
        '../../session/session.gyp:session_usage_observer',
        '../../usage_stats/usage_stats_base.gyp:usage_stats',
        '../../usage_stats/usage_stats.gyp:usage_stats_uploader',
      ],
    },
    {
      'target_name': 'dictionary_downloader',
      'type': 'static_library',
      'sources': [
        'dictionary_downloader.cc',
      ],
      'dependencies': [
        'url_loader_util',
        '../../base/base.gyp:base',
        '../../net/net.gyp:http_client',
      ],
      'link_settings': {
        'libraries': ['-lppapi', '-lppapi_cpp'],
      },
    },
    {
      'target_name': 'url_loader_util',
      'type': 'static_library',
      'sources': [
        'url_loader_util.cc',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
      ],
      'link_settings': {
        'libraries': ['-lppapi', '-lppapi_cpp'],
      },
    },
    {
      'target_name': 'gather_nacl_net_test_files',
      'type': 'none',
      'copies': [{
        'destination': '<(gen_out_dir)/nacl_net_test',
        'files': [
          '<(PRODUCT_DIR)/nacl_net_test_module_x86_64.nexe',
          'browser_test/nacl_net_test/nacl_net_test_module.html',
          'browser_test/nacl_net_test/nacl_net_test_module.nmf',
          'browser_test/nacl_net_test/manifest.json',
        ],
      }],
    },
    {
      'target_name': 'run_nacl_net_test',
      'type': 'none',
      'actions': [
        {
          'action_name': 'run_nacl_net_test',
          'inputs': [
            'nacl_net_test_server.py',
            '<(gen_out_dir)/nacl_net_test/nacl_net_test_module_x86_64.nexe',
            '<(gen_out_dir)/nacl_net_test/nacl_net_test_module.html',
            '<(gen_out_dir)/nacl_net_test/nacl_net_test_module.nmf',
            '<(gen_out_dir)/nacl_net_test/manifest.json',
          ],
          'outputs': ['dummy_run_nacl_net_test'],
          'action': [
            'xvfb-run',
            '--auto-servernum',
            'python',
            'nacl_net_test_server.py',
            '--browser_path=/usr/bin/google-chrome',
            '--load_extension=<(gen_out_dir)/nacl_net_test',
            '--timeout=100',
          ],
        },
      ],
      'dependencies': [
        'translate_nacl_net_test_module',
      ],
    },
    {
      'target_name': 'translate_nacl_net_test_module',
      'sources': [
        '<(PRODUCT_DIR)/nacl_net_test_module',
      ],
      'includes': ['pnacl_translate.gypi'],
    },
    {
      'target_name': 'nacl_net_test_module',
      'type': 'executable',
      'sources': [
        '../../base/file_util_test.cc',
        'nacl_net_test_module.cc',
      ],
      'link_settings': {
        'libraries': ['-lppapi', '-lppapi_cpp'],
      },
      'dependencies': [
        'dictionary_downloader',
        '../../base/base.gyp:base',
        '../../net/net.gyp:http_client',
        '../../net/net.gyp:json_util',
        '../../testing/testing.gyp:nacl_mock_module',
      ],
    },
    {
      'target_name': 'nacl_mozc_crx',
      'type': 'none',
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)',
          'files': [
            '<(gen_out_dir)/nacl_mozc.crx',
            '<(gen_out_dir)/nacl_mozc.pem',
          ],
        },
      ],
    },
    {
      'target_name': 'pack_nacl_mozc_crx',
      'type': 'none',
      'actions': [
        {
          'action_name': 'pack_nacl_mozc_crx',
          'inputs': [
            '<@(nacl_mozc_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/nacl_mozc.crx',
            '<(gen_out_dir)/nacl_mozc.pem',
          ],
          'action': [
            'xvfb-run',
            '--auto-servernum',
            'google-chrome',
            '--pack-extension=<(gen_out_dir)/nacl_mozc',
            # With --pack-extension-key=nacl_mozc.pem option, we should use the
            # same private key to sign the extension. Otherwise, a new packed
            # extension looks a different extension.
            # Of course, the private key must be placed in a secure manner.
            '--no-message-box',
          ],
        },
      ],
    },
    {
      'target_name': 'nacl_mozc_versioning',
      'type': 'none',
      'actions': [
        {
          'action_name': 'nacl_mozc_versioning',
          'inputs': [
            '../../mozc_version.txt',
            '../../build_tools/versioning_files.py',
            '<(PRODUCT_DIR)/nacl_mozc.zip',
            '<(PRODUCT_DIR)/nacl-mozc.tgz',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/nacl_mozc_versioning_dummy',
          ],
          'action': [
            'python',
            '../../build_tools/versioning_files.py',
            '--version_file', '../../mozc_version.txt',
            '--configuration', '<(CONFIGURATION_NAME)',
            '<(PRODUCT_DIR)/nacl_mozc.zip',
            '<(PRODUCT_DIR)/nacl-mozc.tgz',
          ],
          'dependencies': [
            'nacl_mozc',
          ],
        },
      ],
    },
    {
      'target_name': 'nacl_mozc',
      'type': 'none',
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)',
          'files': [
            '<(gen_out_dir)/nacl_mozc.zip',
            '<(gen_out_dir)/nacl-mozc.tgz',
          ],
        },
      ],
    },
    {
      'target_name': 'archive_nacl_mozc_files',
      'type': 'none',
      'actions': [
        {
          'action_name': 'archive_nacl_mozc_files',
          'inputs': [
            '../../mozc_version.txt',
            'archive_files.py',
            '<@(nacl_mozc_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/nacl_mozc.zip',
            '<(gen_out_dir)/nacl-mozc.tgz',
          ],
          'action': [
            'python',
            'archive_files.py',
            '--zip_output=<(gen_out_dir)/nacl_mozc.zip',
            '--tgz_output=<(gen_out_dir)/nacl-mozc.tgz',
            '--version_file', '../../mozc_version.txt',
            '--top_dir_base', 'nacl-mozc',
            '--base_path=<(gen_out_dir)/nacl_mozc/',
            '<@(nacl_mozc_files)',
          ],
        },
      ],
    },
    {
      'target_name': 'gather_nacl_mozc_files',
      'type': 'none',
      'copies': [{
        'destination': '<(gen_out_dir)/nacl_mozc',
        'files': [
          '../../data/images/product_icon_32bpp-128.png',
          '../../data/installer/credits_en.html',
          '../../data/installer/credits_ja.html',
          '<(gen_out_dir)/manifest.json',
          '<(gen_out_dir)/nacl_mozc_version.js',
          '<(PRODUCT_DIR)/nacl_session_handler_x86_32.nexe',
          '<(PRODUCT_DIR)/nacl_session_handler_x86_64.nexe',
          '<(PRODUCT_DIR)/nacl_session_handler_arm.nexe',
          'dictionary_tool.js',
          'key_translator.js',
          'nacl_mozc.html',
          'nacl_mozc.js',
          'nacl_mozc_init.js',
          'nacl_session_handler.nmf',
          'option_page.js',
          'options.css',
          'options.html',
          'options.js',
        ],
        'conditions': [
          ['branding=="GoogleJapaneseInput"', {
            'files': [
              '<(SHARED_INTERMEDIATE_DIR)/data_manager/packed/zipped_data_chromeos',
            ],
          }],
          ['branding=="Mozc"', {
            'files': [
              '<(SHARED_INTERMEDIATE_DIR)/data_manager/packed/zipped_data_oss',
            ],
          }],
        ],
      }],
    },
    {
      'target_name': 'gen_manifest_and_messages',
      'type': 'none',
      'toolsets': ['host'],
      'actions': [
        {
          'action_name': 'gen_manifest',
          'inputs': [
            '../../mozc_version.txt',
            '../../build_tools/replace_version.py',
            'manifest/manifest_template.json',
          ],
          'outputs': [
            '<(gen_out_dir)/manifest.json',
          ],
          'action': [
            'python', '../../build_tools/replace_version.py',
            '--version_file', '../../mozc_version.txt',
            '--input', 'manifest/manifest_template.json',
            '--output', '<(gen_out_dir)/manifest.json',
            '--branding', '<(branding)',
          ],
        },
        {
          'action_name': 'gen_nacl_mozc_version',
          'inputs': [
            '../../mozc_version.txt',
            '../../build_tools/replace_version.py',
            'nacl_mozc_version_template.js',
          ],
          'outputs': [
            '<(gen_out_dir)/nacl_mozc_version.js',
          ],
          'action': [
            'python', '../../build_tools/replace_version.py',
            '--version_file', '../../mozc_version.txt',
            '--input', 'nacl_mozc_version_template.js',
            '--output', '<(gen_out_dir)/nacl_mozc_version.js',
            '--branding', '<(branding)',
          ],
        },
        {
          'action_name': 'gen_en_messages',
          'inputs': [
            '../../mozc_version.txt',
            '../../build_tools/replace_version.py',
            '_locales/en/messages_template.json',
          ],
          'outputs': [
            '<(gen_out_dir)/nacl_mozc/_locales/en/messages.json',
          ],
          'action': [
            'python', '../../build_tools/replace_version.py',
            '--version_file', '../../mozc_version.txt',
            '--input', '_locales/en/messages_template.json',
            '--output',
            '<(gen_out_dir)/nacl_mozc/_locales/en/messages.json',
            '--branding', '<(branding)',
          ],
        },
        {
          'action_name': 'gen_ja_messages',
          'inputs': [
            '../../mozc_version.txt',
            '../../build_tools/replace_version.py',
            '_locales/ja/messages_template.json',
          ],
          'outputs': [
            '<(gen_out_dir)/nacl_mozc/_locales/ja/messages.json',
          ],
          'action': [
            'python', '../../build_tools/replace_version.py',
            '--version_file', '../../mozc_version.txt',
            '--input', '_locales/ja/messages_template.json',
            '--output',
            '<(gen_out_dir)/nacl_mozc/_locales/ja/messages.json',
            '--branding', '<(branding)',
          ],
        },
      ],
      'conditions': [
        ['branding=="GoogleJapaneseInput"', {
          'actions': [
            {
              'action_name': 'gen_partial_supported_messages',
              'inputs': [
                '../../mozc_version.txt',
                'gen_partial_supported_messages.py',
              ],
              'outputs': [
                '<@(partial_supported_messages)',
              ],
              'action': [
                'python', 'gen_partial_supported_messages.py',
                '--version_file', '../../mozc_version.txt',
                '--outdir', '<(gen_out_dir)/nacl_mozc/_locales/',
              ],
            },
          ],
        }],
      ],
    },
    {
      'target_name': 'run_nacl_test',
      'type': 'none',
      'hard_dependency': 1,
      'dependencies': [
        'build_nacl_test',
      ],
      'export_dependent_settings': [
        'build_nacl_test',
      ],
      'actions': [
        {
          'action_name': 'run_nacl_test',
          'inputs': ['<(dummy_input_file)'],
          'outputs': ['dummy_run_nacl_test'],
          'action': [
            'python', 'run_nacl_test.py',
            '--nacl_sdk_root=<(nacl_sdk_root)',
            '--test_bin_dir=<(PRODUCT_DIR)',
          ],
        },
      ],
    },
    {
      'target_name': 'build_nacl_test',
      'sources': [
        '<(PRODUCT_DIR)/base_test',
        '<(PRODUCT_DIR)/rewriter_test',
        '<(PRODUCT_DIR)/session_handler_test',
      ],
      'includes': ['pnacl_translate.gypi'],
    },
    # This test doesn't works well on general environment since it depends on
    # external source code.
    {
      'target_name': 'run_nacl_end_to_end_test_target',
      'type': 'none',
      'actions': [
        {
          'action_name': 'run_nacl_end_to_end_test',
          'inputs': [
            '<(gen_out_dir)/nacl_mozc/key_translator.js',
            '<(gen_out_dir)/nacl_mozc/manifest.json',
            '<(gen_out_dir)/nacl_mozc/nacl_mozc.html',
            '<(gen_out_dir)/nacl_mozc/nacl_mozc.js',
            '<(gen_out_dir)/nacl_mozc/nacl_mozc_version.js',
            '<(gen_out_dir)/nacl_mozc/nacl_session_handler.nmf',
            '<(gen_out_dir)/nacl_mozc/nacl_session_handler_x86_32.nexe',
            '<(gen_out_dir)/nacl_mozc/nacl_session_handler_x86_64.nexe',
            'browser_test/nacl_mozc_test.html',
            'browser_test/nacl_mozc_test_util.js',
          ],
          'outputs': ['dummy_run_nacl_end_to_end_test'],
          'action': [
            'xvfb-run',
            '--auto-servernum',
            'python',
            '<(browser_tester_dir)/browser_tester.py',
            '--browser_path=/usr/bin/google-chrome',
            '--file=<(browser_tester_dir)/nacltest.js',
            '--serving_dir=<(gen_out_dir)/nacl_mozc',
            '--serving_dir=browser_test',
            '--url=nacl_mozc_test.html',
            '--timeout=15',
          ],
        },
      ],
      'dependencies': [
        'gather_nacl_mozc_files',
      ],
    },
  ],
}
