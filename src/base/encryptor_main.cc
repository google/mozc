// Copyright 2010-2020, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <iostream>
#include <string>

#include "base/encryptor.h"
#include "base/file_stream.h"
#include "base/flags.h"
#include "base/init_mozc.h"
#include "base/logging.h"
#include "base/mmap.h"
#include "base/util.h"

DEFINE_string(password, "", "password");
DEFINE_string(salt, "", "salt");
DEFINE_string(iv, "", "initialization vector");

DEFINE_bool(encrypt, false, "encrypt mode");
DEFINE_bool(decrypt, false, "decrypt mode");

// encrypt/decrypt files
DEFINE_string(input_file, "", "input file");
DEFINE_string(output_file, "", "input file");

// perform encryption/decription with test_input.
// used for making a golden data for unittesting
DEFINE_string(test_input, "", "test input string");

namespace {
std::string Escape(const std::string &buf) {
  std::string tmp;
  mozc::Util::Escape(buf, &tmp);
  return tmp;
}
}  // namespace

int main(int argc, char **argv) {
  mozc::InitMozc(argv[0], &argc, &argv);

  if (!FLAGS_iv.empty()) {
    CHECK_EQ(16, FLAGS_iv.size()) << "iv size must be 16 byte";
  }

  const uint8 *iv = FLAGS_iv.empty()
                        ? nullptr
                        : reinterpret_cast<const uint8 *>(FLAGS_iv.data());

  if (!FLAGS_input_file.empty() && !FLAGS_output_file.empty()) {
    mozc::Encryptor::Key key;
    CHECK(key.DeriveFromPassword(FLAGS_password, FLAGS_salt, iv));

    mozc::Mmap mmap;
    CHECK(mmap.Open(FLAGS_input_file.c_str(), "r"));
    std::string buf(mmap.begin(), mmap.size());
    if (FLAGS_encrypt) {
      CHECK(mozc::Encryptor::EncryptString(key, &buf));
    } else if (FLAGS_decrypt) {
      CHECK(mozc::Encryptor::DecryptString(key, &buf));
    } else {
      LOG(FATAL) << "unknown mode. set --encrypt or --decrypt";
    }
    mozc::OutputFileStream ofs(FLAGS_output_file.c_str(), std::ios::binary);
    CHECK(ofs);
    ofs.write(buf.data(), buf.size());
  } else if (!FLAGS_test_input.empty()) {
    mozc::Encryptor::Key key1, key2;
    CHECK(key1.DeriveFromPassword(FLAGS_password, FLAGS_salt, iv));
    CHECK(key2.DeriveFromPassword(FLAGS_password, FLAGS_salt, iv));

    std::string buf = FLAGS_test_input;
    std::string iv_buf(reinterpret_cast<const char *>(key1.iv()),
                       key1.iv_size());

    std::cout << "Password:  \"" << Escape(FLAGS_password) << "\"" << std::endl;
    std::cout << "Salt:      \"" << Escape(FLAGS_salt) << "\"" << std::endl;
    std::cout << "IV:        \"" << Escape(iv_buf) << "\"" << std::endl;
    std::cout << "Input:     \"" << Escape(buf) << "\"" << std::endl;
    CHECK(mozc::Encryptor::EncryptString(key1, &buf));
    std::cout << "Encrypted: \"" << Escape(buf) << "\"" << std::endl;
    CHECK(mozc::Encryptor::DecryptString(key2, &buf));
    std::cout << "Decrypted: \"" << Escape(buf) << "\"" << std::endl;
  } else {
    LOG(ERROR) << "Unknown mode. set --input_file/--output_file/--test_input";
  }

  return 0;
}
