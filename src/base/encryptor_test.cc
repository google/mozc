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

#include "base/encryptor.h"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>

#include "base/random.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace {

struct TestData {
  const char* password;
  size_t password_size;
  const char* salt;
  size_t salt_size;
  const char* iv;
  const char* input;
  size_t input_size;
  const char* encrypted;
  size_t encrypted_size;
};

constexpr TestData kTestData[] = {
    {"foohoge", 7, "salt", 4, "1111111111111111", "foo", 3,
     "\x27\x32\x66\x88\x82\x33\x78\x80\x58\x29\xBF\xDD\x46\x9A\xCC\x87", 16},
    {"password", 8, "salt", 4, "1111111111111111", "a", 1,
     "\x2A\xA1\x73\xB0\x91\x1C\x22\x40\x55\xDB\xAB\xC0\x77\x39\xE6\x57", 16},
    {"password", 8, "salt", 4, "1111111111111111", "test", 4,
     "\x13\x16\x0A\xA4\x2B\xA3\x02\xC4\xEF\x47\x98\x6D\x9F\xC9\xAD\x43", 16},
    {"password", 8, "salt", 4, "0123456789abcdef",
     "dhoifasoifaoishd"
     "oifahsoifdhaoish"
     "fioashdoifahisod"
     "fhaioshdfioa",
     60,
     "\x27\x92\xD1\x4F\xCE\x71\xFF\xA0\x9E\x52\xAB\x96\xB4\x5D\x1A\x2F"
     "\xE0\xC7\xB3\x92\xD7\xB8\x29\xB0\xEF\xD3\x51\x9F\xBD\x87\xE0\xB4"
     "\x0A\x06\xE0\x9A\x03\x72\x48\xB3\x8F\x9A\x5E\xAC\xCD\x5D\xB8\x0B"
     "\x01\x1D\x2C\xD7\xAA\x55\x05\x0F\x4E\xD5\x73\xC0\xCB\xE2\x10\x69",
     64}};

class EncryptorTest : public testing::TestWithTempUserProfile {};

TEST_F(EncryptorTest, VerificationTest) {
  {
    const std::string password(kTestData[0].password,
                               kTestData[0].password_size);
    const std::string salt(kTestData[0].salt, kTestData[0].salt_size);
    Encryptor::Key key1, key2;
    EXPECT_TRUE(key1.DeriveFromPassword(
        password, salt, reinterpret_cast<const uint8_t*>(kTestData[0].iv)));
    EXPECT_TRUE(key1.IsAvailable());
    EXPECT_TRUE(key2.DeriveFromPassword(
        password, salt, reinterpret_cast<const uint8_t*>(kTestData[0].iv)));
    EXPECT_TRUE(key2.IsAvailable());
    std::string input(kTestData[0].input, kTestData[0].input_size);
    const std::string encrypted(kTestData[0].encrypted,
                                kTestData[0].encrypted_size);
    const std::string decrypted(input.data(), input.size());

    EXPECT_TRUE(Encryptor::EncryptString(key1, &input));
    EXPECT_EQ(input, encrypted);
    EXPECT_TRUE(Encryptor::DecryptString(key2, &input));
    EXPECT_EQ(input, decrypted);
  }
  {
    const std::string password(kTestData[1].password,
                               kTestData[1].password_size);
    const std::string salt(kTestData[1].salt, kTestData[1].salt_size);
    Encryptor::Key key1, key2;
    EXPECT_TRUE(key1.DeriveFromPassword(
        password, salt, reinterpret_cast<const uint8_t*>(kTestData[1].iv)));
    EXPECT_TRUE(key1.IsAvailable());
    EXPECT_TRUE(key2.DeriveFromPassword(
        password, salt, reinterpret_cast<const uint8_t*>(kTestData[1].iv)));
    EXPECT_TRUE(key2.IsAvailable());
    std::string input(kTestData[1].input, kTestData[1].input_size);
    const std::string encrypted(kTestData[1].encrypted,
                                kTestData[1].encrypted_size);
    const std::string decrypted(input.data(), input.size());

    EXPECT_TRUE(Encryptor::EncryptString(key1, &input));
    EXPECT_EQ(input, encrypted);
    EXPECT_TRUE(Encryptor::DecryptString(key2, &input));
    EXPECT_EQ(input, decrypted);
  }
  {
    const std::string password(kTestData[2].password,
                               kTestData[2].password_size);
    const std::string salt(kTestData[2].salt, kTestData[2].salt_size);
    Encryptor::Key key1, key2;
    EXPECT_TRUE(key1.DeriveFromPassword(
        password, salt, reinterpret_cast<const uint8_t*>(kTestData[2].iv)));
    EXPECT_TRUE(key1.IsAvailable());
    EXPECT_TRUE(key2.DeriveFromPassword(
        password, salt, reinterpret_cast<const uint8_t*>(kTestData[2].iv)));
    EXPECT_TRUE(key2.IsAvailable());
    std::string input(kTestData[2].input, kTestData[2].input_size);
    const std::string encrypted(kTestData[2].encrypted,
                                kTestData[2].encrypted_size);
    const std::string decrypted(input.data(), input.size());

    EXPECT_TRUE(Encryptor::EncryptString(key1, &input));
    EXPECT_EQ(input, encrypted);
    EXPECT_TRUE(Encryptor::DecryptString(key2, &input));
    EXPECT_EQ(input, decrypted);
  }
  {
    const std::string password(kTestData[3].password,
                               kTestData[3].password_size);
    const std::string salt(kTestData[3].salt, kTestData[3].salt_size);
    Encryptor::Key key1, key2;
    EXPECT_TRUE(key1.DeriveFromPassword(
        password, salt, reinterpret_cast<const uint8_t*>(kTestData[3].iv)));
    EXPECT_TRUE(key1.IsAvailable());
    EXPECT_TRUE(key2.DeriveFromPassword(
        password, salt, reinterpret_cast<const uint8_t*>(kTestData[3].iv)));
    EXPECT_TRUE(key2.IsAvailable());
    std::string input(kTestData[3].input, kTestData[3].input_size);
    const std::string encrypted(kTestData[3].encrypted,
                                kTestData[3].encrypted_size);
    const std::string decrypted(input.data(), input.size());

    EXPECT_TRUE(Encryptor::EncryptString(key1, &input));
    EXPECT_EQ(input, encrypted);
    EXPECT_TRUE(Encryptor::DecryptString(key2, &input));
    EXPECT_EQ(input, decrypted);
  }
}

TEST_F(EncryptorTest, BasicTest) {
  Encryptor::Key key;
  EXPECT_FALSE(key.DeriveFromPassword(""));
  EXPECT_FALSE(key.IsAvailable());
  EXPECT_TRUE(key.DeriveFromPassword("test"));
  EXPECT_TRUE(key.IsAvailable());
  EXPECT_EQ(key.block_size(), 16);
  EXPECT_EQ(key.key_size(), 256);
  EXPECT_EQ(key.iv_size(), 16);
}

TEST_F(EncryptorTest, EncryptBatch) {
  constexpr size_t kSizeTable[] = {1,    10,   16,    32,    100,
                                   1000, 1600, 10000, 16000, 100000};

  Random random;
  for (size_t i = 0; i < std::size(kSizeTable); ++i) {
    const std::string original = random.ByteString(kSizeTable[i]);

    Encryptor::Key key1, key2, key3, key4;

    EXPECT_TRUE(key1.DeriveFromPassword("test", "salt"));
    EXPECT_TRUE(key2.DeriveFromPassword("test", "salt"));

    EXPECT_TRUE(key3.DeriveFromPassword("test", "salt2"));  // wrong salt
    EXPECT_TRUE(key4.DeriveFromPassword("test2", "salt"));  // wrong key

    EXPECT_TRUE(key1.IsAvailable());
    EXPECT_TRUE(key2.IsAvailable());
    EXPECT_TRUE(key3.IsAvailable());
    EXPECT_TRUE(key4.IsAvailable());

    std::string encrypted(original);

    EXPECT_TRUE(Encryptor::EncryptString(key1, &encrypted));
    EXPECT_EQ(encrypted.size() % key1.block_size(), 0);
    EXPECT_NE(encrypted, original);

    std::string encrypted3(encrypted.data(), encrypted.size());
    std::string encrypted4(encrypted.data(), encrypted.size());

    EXPECT_TRUE(Encryptor::DecryptString(key2, &encrypted));
    EXPECT_EQ(encrypted.size(), original.size());
    EXPECT_EQ(encrypted, original);

    // wrong key
    Encryptor::DecryptString(key3, &encrypted3);
    Encryptor::DecryptString(key4, &encrypted4);
    EXPECT_NE(original, encrypted3);
    EXPECT_NE(original, encrypted4);

    // invalid values
    EXPECT_FALSE(Encryptor::EncryptString(key1, nullptr));
    EXPECT_FALSE(Encryptor::DecryptString(key1, nullptr));

    // empty string
    std::string empty_string;
    EXPECT_FALSE(Encryptor::EncryptString(key1, &empty_string));
    EXPECT_FALSE(Encryptor::DecryptString(key1, &empty_string));
  }
}

TEST_F(EncryptorTest, ProtectData) {
  constexpr size_t kSizeTable[] = {1, 10, 100, 1000, 10000, 100000};

  Random random;
  for (size_t i = 0; i < std::size(kSizeTable); ++i) {
    const std::string input = random.ByteString(kSizeTable[i]);
    std::string output;
    EXPECT_TRUE(Encryptor::ProtectData(input, &output));
    EXPECT_NE(input, output);
    std::string result;
    EXPECT_TRUE(Encryptor::UnprotectData(output, &result));
    EXPECT_EQ(result, input);
  }
}

}  // namespace
}  // namespace mozc
