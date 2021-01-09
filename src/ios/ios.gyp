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

{
  'targets': [
    {
      'target_name': 'ios_engine_main',
      'type': 'executable',
      'sources': [
        'ios_engine_main.cc',
      ],
      'dependencies': [
        'ios_engine',
      ],
    },
    {
      'target_name': 'ios_engine',
      'type': 'static_library',
      'sources': [
        'ios_engine.cc',
        'ios_engine.h',
      ],
      'dependencies': [
        '../base/base.gyp:base_core',
        '../composer/composer.gyp:composer',
        '../config/config.gyp:config_handler',
        '../data_manager/oss/oss_data_manager.gyp:oss_data_manager',
        '../engine/engine.gyp:engine_builder',
        '../engine/engine.gyp:minimal_engine',
        '../protocol/protocol.gyp:commands_proto',
        '../protocol/protocol.gyp:config_proto',
        '../session/session.gyp:session',
        '../session/session.gyp:session_handler',
      ],
    },
    {
      'target_name': 'libmozc_gboard',
      'type': 'none',
      'variables': {
        # libraries to be packed into a single library for Gboard.
        # This list is generated from the necessary libraries to build
        # ios_engine_main except for libprotobuf.a. libprotobuf.a is already
        # included by Gboard.
        'src_libs': [
          '<(PRODUCT_DIR)/libbase.a',
          '<(PRODUCT_DIR)/libbase_core.a',
          '<(PRODUCT_DIR)/libbit_vector_based_array.a',
          '<(PRODUCT_DIR)/libcalculator.a',
          '<(PRODUCT_DIR)/libcandidates_proto.a',
          '<(PRODUCT_DIR)/libcharacter_form_manager.a',
          '<(PRODUCT_DIR)/libclock.a',
          '<(PRODUCT_DIR)/libcodec.a',
          '<(PRODUCT_DIR)/libcodec_factory.a',
          '<(PRODUCT_DIR)/libcodec_util.a',
          '<(PRODUCT_DIR)/libcommands_proto.a',
          '<(PRODUCT_DIR)/libcomposer.a',
          '<(PRODUCT_DIR)/libconfig_file_stream.a',
          '<(PRODUCT_DIR)/libconfig_handler.a',
          '<(PRODUCT_DIR)/libconfig_proto.a',
          '<(PRODUCT_DIR)/libconnector.a',
          '<(PRODUCT_DIR)/libconversion_request.a',
          '<(PRODUCT_DIR)/libconverter.a',
          '<(PRODUCT_DIR)/libconverter_util.a',
          '<(PRODUCT_DIR)/libdata_manager.a',
          '<(PRODUCT_DIR)/libdataset_proto.a',
          '<(PRODUCT_DIR)/libdataset_reader.a',
          '<(PRODUCT_DIR)/libdictionary_file.a',
          '<(PRODUCT_DIR)/libdictionary_impl.a',
          '<(PRODUCT_DIR)/libencryptor.a',
          '<(PRODUCT_DIR)/libengine.a',
          '<(PRODUCT_DIR)/libengine_builder.a',
          '<(PRODUCT_DIR)/libengine_builder_proto.a',
          '<(PRODUCT_DIR)/libflags.a',
          '<(PRODUCT_DIR)/libgoogle_data_manager.a',
          '<(PRODUCT_DIR)/libhash.a',
          '<(PRODUCT_DIR)/libimmutable_converter.a',
          '<(PRODUCT_DIR)/libimmutable_converter_interface.a',
          '<(PRODUCT_DIR)/libios_engine.a',
          '<(PRODUCT_DIR)/libkey_event_util.a',
          '<(PRODUCT_DIR)/libkey_parser.a',
          '<(PRODUCT_DIR)/libkeymap.a',
          '<(PRODUCT_DIR)/libkeymap_factory.a',
          '<(PRODUCT_DIR)/liblattice.a',
          '<(PRODUCT_DIR)/liblouds.a',
          '<(PRODUCT_DIR)/liblouds_trie.a',
          '<(PRODUCT_DIR)/libmultifile.a',
          '<(PRODUCT_DIR)/libmutex.a',
          '<(PRODUCT_DIR)/libobfuscator_support.a',
          '<(PRODUCT_DIR)/libprediction.a',
          '<(PRODUCT_DIR)/libprediction_protocol.a',
          # libprotobuf is included in a different place.
          # '<(PRODUCT_DIR)/libprotobuf.a',
          '<(PRODUCT_DIR)/librequest_test_util.a',
          '<(PRODUCT_DIR)/librewriter.a',
          '<(PRODUCT_DIR)/libsegmenter.a',
          '<(PRODUCT_DIR)/libsegmenter_data_proto.a',
          '<(PRODUCT_DIR)/libsegments.a',
          '<(PRODUCT_DIR)/libserialized_dictionary.a',
          '<(PRODUCT_DIR)/libserialized_string_array.a',
          '<(PRODUCT_DIR)/libsession.a',
          '<(PRODUCT_DIR)/libsession_handler.a',
          '<(PRODUCT_DIR)/libsession_internal.a',
          '<(PRODUCT_DIR)/libsession_usage_stats_util.a',
          '<(PRODUCT_DIR)/libsimple_succinct_bit_vector_index.a',
          '<(PRODUCT_DIR)/libsingleton.a',
          '<(PRODUCT_DIR)/libstats_config_util.a',
          '<(PRODUCT_DIR)/libstorage.a',
          '<(PRODUCT_DIR)/libsuffix_dictionary.a',
          '<(PRODUCT_DIR)/libsuggestion_filter.a',
          '<(PRODUCT_DIR)/libsuppression_dictionary.a',
          '<(PRODUCT_DIR)/libsystem_dictionary.a',
          '<(PRODUCT_DIR)/libsystem_dictionary_codec.a',
          '<(PRODUCT_DIR)/libtext_dictionary_loader.a',
          '<(PRODUCT_DIR)/libtransliteration.a',
          '<(PRODUCT_DIR)/libusage_stats.a',
          '<(PRODUCT_DIR)/libusage_stats_protocol.a',
          '<(PRODUCT_DIR)/libuser_dictionary.a',
          '<(PRODUCT_DIR)/libuser_dictionary_storage_proto.a',
          '<(PRODUCT_DIR)/libuser_pos.a',
          '<(PRODUCT_DIR)/libvalue_dictionary.a',
        ]
      },
      'actions': [
        {
          'action_name': 'libtool',
          'outputs': ['<(PRODUCT_DIR)/libmozc_gboard.a'],
          'inputs': ['<@(src_libs)'],
          'action': [
            'libtool', '-static', '-o', '<(PRODUCT_DIR)/libmozc_gboard.a',
            '<@(src_libs)',
          ],
        },
      ],
    },
  ],
}
