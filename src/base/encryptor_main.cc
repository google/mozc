// Copyright 2010-2021, Google Inc.
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

#include <cstdint>
#include <iostream>
#include <ostream>
#include <string>

#include "base/encryptor.h"
#include "base/file_util.h"
#include "base/init_mozc.h"
#include "base/logging.h"
#include "base/status.h"
#include "base/util.h"
#include "absl/flags/flag.h"

ABSL_FLAG(std::string, password, "", "password");
ABSL_FLAG(std::string, salt, "", "salt");
ABSL_FLAG(std::string, iv, "", "initialization vector");

ABSL_FLAG(bool, encrypt, false, "encrypt mode");
ABSL_FLAG(bool, decrypt, false, "decrypt mode");

// encrypt/decrypt files
ABSL_FLAG(std::string, input_file, "", "input file");
ABSL_FLAG(std::string, output_file, "", "input file");

// perform encryption/decryption with test_input.
// used for making a golden data for unittesting
ABSL_FLAG(std::string, test_input, "", "test input string");

namespace {
std::string Escape(const std::string &buf) {
  std::string tmp;
  mozc::Util::Escape(buf, &tmp);
  return tmp;
}
}  // namespace

int main(int argc, char **argv) {
  mozc::InitMozc(argv[0], &argc, &argv);

  if (!absl::GetFlag(FLAGS_iv).empty()) {
    CHECK_EQ(16, absl::GetFlag(FLAGS_iv).size()) << "iv size must be 16 byte";
  }

  const std::string iv_str = absl::GetFlag(FLAGS_iv);
  const uint8_t *iv = iv_str.empty()
                          ? nullptr
                          : reinterpret_cast<const uint8_t *>(iv_str.data());

  if (!absl::GetFlag(FLAGS_input_file).empty() &&
      !absl::GetFlag(FLAGS_output_file).empty()) {
    mozc::Encryptor::Key key;
    CHECK(key.DeriveFromPassword(absl::GetFlag(FLAGS_password),
                                 absl::GetFlag(FLAGS_salt), iv));
    std::string buf;
    CHECK_OK(
        mozc::FileUtil::GetContents(absl::GetFlag(FLAGS_input_file), &buf));
    if (absl::GetFlag(FLAGS_encrypt)) {
      CHECK(mozc::Encryptor::EncryptString(key, &buf));
    } else if (absl::GetFlag(FLAGS_decrypt)) {
      CHECK(mozc::Encryptor::DecryptString(key, &buf));
    } else {
      LOG(FATAL) << "unknown mode. set --encrypt or --decrypt";
    }
    CHECK_OK(
        mozc::FileUtil::SetContents(absl::GetFlag(FLAGS_output_file), buf));
  } else if (!absl::GetFlag(FLAGS_test_input).empty()) {
    mozc::Encryptor::Key key1, key2;
    CHECK(key1.DeriveFromPassword(absl::GetFlag(FLAGS_password),
                                  absl::GetFlag(FLAGS_salt), iv));
    CHECK(key2.DeriveFromPassword(absl::GetFlag(FLAGS_password),
                                  absl::GetFlag(FLAGS_salt), iv));

    std::string buf = absl::GetFlag(FLAGS_test_input);
    std::string iv_buf(reinterpret_cast<const char *>(key1.iv()),
                       key1.iv_size());

    std::cout << "Password:  \"" << Escape(absl::GetFlag(FLAGS_password))
              << "\"" << std::endl;
    std::cout << "Salt:      \"" << Escape(absl::GetFlag(FLAGS_salt)) << "\""
              << std::endl;
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
