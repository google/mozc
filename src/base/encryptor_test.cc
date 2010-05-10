// Copyright 2010, Google Inc.
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

#include "base/base.h"
#include "base/encryptor.h"
#include "base/password_manager.h"
#include "base/util.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {

namespace {
struct TestData {
  const char *password;
  size_t password_size;
  const char *salt;
  size_t salt_size;
  const char *iv;
  const char *input;
  size_t input_size;
  const char *encrypted;
  size_t encrypted_size;
};

const TestData kTestData[] = {
  {
    "\x66\x6F\x6F\x68\x6F\x67\x65", 7,
    "\x73\x61\x6C\x74", 4,
    "\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31",
    "\x66\x6F\x6F", 3,
    "\x27\x32\x66\x88\x82\x33\x78\x80\x58\x29\xBF\xDD\x46\x9A\xCC\x87", 16
  },
  {
    "\x70\x61\x73\x73\x77\x6F\x72\x64", 8,
    "\x73\x61\x6C\x74", 4,
    "\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31",
    "\x61", 1,
    "\x2A\xA1\x73\xB0\x91\x1C\x22\x40\x55\xDB\xAB\xC0\x77\x39\xE6\x57", 16
  },
  {
    "\x70\x61\x73\x73\x77\x6F\x72\x64", 8,
    "\x73\x61\x6C\x74", 4,
    "\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31\x31",
    "\x74\x65\x73\x74", 4,
    "\x13\x16\x0A\xA4\x2B\xA3\x02\xC4\xEF\x47\x98\x6D\x9F\xC9\xAD\x43", 16
  },
  {
    "\x70\x61\x73\x73\x77\x6F\x72\x64", 8,
    "\x73\x61\x6C\x74", 4,
    "\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x61\x62\x63\x64\x65\x66",
    "\x64\x68\x6F\x69\x66\x61\x73\x6F\x69\x66\x61\x6F\x69\x73\x68\x64"
    "\x6F\x69\x66\x61\x68\x73\x6F\x69\x66\x64\x68\x61\x6F\x69\x73\x68"
    "\x66\x69\x6F\x61\x73\x68\x64\x6F\x69\x66\x61\x68\x69\x73\x6F\x64"
    "\x66\x68\x61\x69\x6F\x73\x68\x64\x66\x69\x6F\x61", 60,
    "\x27\x92\xD1\x4F\xCE\x71\xFF\xA0\x9E\x52\xAB\x96\xB4\x5D\x1A\x2F"
    "\xE0\xC7\xB3\x92\xD7\xB8\x29\xB0\xEF\xD3\x51\x9F\xBD\x87\xE0\xB4"
    "\x0A\x06\xE0\x9A\x03\x72\x48\xB3\x8F\x9A\x5E\xAC\xCD\x5D\xB8\x0B"
    "\x01\x1D\x2C\xD7\xAA\x55\x05\x0F\x4E\xD5\x73\xC0\xCB\xE2\x10\x69", 64
  }
};
}

TEST(EncryptorTest, VerificationTest) {
  kUseMockPasswordManager = true;
  {
    const string password(kTestData[0].password, kTestData[0].password_size);
    const string salt(kTestData[0].salt, kTestData[0].salt_size);
    Encryptor::Key key1, key2;
    EXPECT_TRUE(key1.DeriveFromPassword(
                    password, salt,
                    reinterpret_cast<const uint8 *>(kTestData[0].iv)));
    EXPECT_TRUE(key1.IsAvailable());
    EXPECT_TRUE(key2.DeriveFromPassword(
                    password, salt,
                    reinterpret_cast<const uint8 *>(kTestData[0].iv)));
    EXPECT_TRUE(key2.IsAvailable());
    string input(kTestData[0].input, kTestData[0].input_size);
    const string encrypted(kTestData[0].encrypted, kTestData[0].encrypted_size);
    const string decrypted(input.data(), input.size());

    EXPECT_TRUE(Encryptor::EncryptString(key1, &input));
    EXPECT_EQ(encrypted, input);
    EXPECT_TRUE(Encryptor::DecryptString(key2, &input));
    EXPECT_EQ(decrypted, input);
  }
  {
    const string password(kTestData[1].password, kTestData[1].password_size);
    const string salt(kTestData[1].salt, kTestData[1].salt_size);
    Encryptor::Key key1, key2;
    EXPECT_TRUE(key1.DeriveFromPassword(
                    password, salt,
                    reinterpret_cast<const uint8 *>(kTestData[1].iv)));
    EXPECT_TRUE(key1.IsAvailable());
    EXPECT_TRUE(key2.DeriveFromPassword(
                    password, salt,
                    reinterpret_cast<const uint8 *>(kTestData[1].iv)));
    EXPECT_TRUE(key2.IsAvailable());
    string input(kTestData[1].input, kTestData[1].input_size);
    const string encrypted(kTestData[1].encrypted, kTestData[1].encrypted_size);
    const string decrypted(input.data(), input.size());

    EXPECT_TRUE(Encryptor::EncryptString(key1, &input));
    EXPECT_EQ(encrypted, input);
    EXPECT_TRUE(Encryptor::DecryptString(key2, &input));
    EXPECT_EQ(decrypted, input);
  }
  {
    const string password(kTestData[2].password, kTestData[2].password_size);
    const string salt(kTestData[2].salt, kTestData[2].salt_size);
    Encryptor::Key key1, key2;
    EXPECT_TRUE(key1.DeriveFromPassword(
                    password, salt,
                    reinterpret_cast<const uint8 *>(kTestData[2].iv)));
    EXPECT_TRUE(key1.IsAvailable());
    EXPECT_TRUE(key2.DeriveFromPassword(
                    password, salt,
                    reinterpret_cast<const uint8 *>(kTestData[2].iv)));
    EXPECT_TRUE(key2.IsAvailable());
    string input(kTestData[2].input, kTestData[2].input_size);
    const string encrypted(kTestData[2].encrypted, kTestData[2].encrypted_size);
    const string decrypted(input.data(), input.size());

    EXPECT_TRUE(Encryptor::EncryptString(key1, &input));
    EXPECT_EQ(encrypted, input);
    EXPECT_TRUE(Encryptor::DecryptString(key2, &input));
    EXPECT_EQ(decrypted, input);
  }
  {
    const string password(kTestData[3].password, kTestData[3].password_size);
    const string salt(kTestData[3].salt, kTestData[3].salt_size);
    Encryptor::Key key1, key2;
    EXPECT_TRUE(key1.DeriveFromPassword(
                    password, salt,
                    reinterpret_cast<const uint8 *>(kTestData[3].iv)));
    EXPECT_TRUE(key1.IsAvailable());
    EXPECT_TRUE(key2.DeriveFromPassword(
                    password, salt,
                    reinterpret_cast<const uint8 *>(kTestData[3].iv)));
    EXPECT_TRUE(key2.IsAvailable());
    string input(kTestData[3].input, kTestData[3].input_size);
    const string encrypted(kTestData[3].encrypted, kTestData[3].encrypted_size);
    const string decrypted(input.data(), input.size());

    EXPECT_TRUE(Encryptor::EncryptString(key1, &input));
    EXPECT_EQ(encrypted, input);
    EXPECT_TRUE(Encryptor::DecryptString(key2, &input));
    EXPECT_EQ(decrypted, input);
  }
}

TEST(EncryptorTest, BasicTest) {
  kUseMockPasswordManager = true;
  Encryptor::Key key;
  EXPECT_FALSE(key.DeriveFromPassword(""));
  EXPECT_FALSE(key.IsAvailable());
  EXPECT_TRUE(key.DeriveFromPassword("test"));
  EXPECT_TRUE(key.IsAvailable());
  EXPECT_EQ(16, key.block_size());
  EXPECT_EQ(256, key.key_size());
  EXPECT_EQ(16, key.iv_size());
}

TEST(EncryptorTest, EncryptBatch) {
  kUseMockPasswordManager = true;
  const size_t kSizeTable[] = { 1, 10, 16, 32, 100, 1000, 1600,
                                10000, 16000, 100000 };

  for (size_t i = 0; i < arraysize(kSizeTable); ++i) {
    scoped_array<char> buf(new char[kSizeTable[i]]);
    Util::GetSecureRandomSequence(buf.get(), kSizeTable[i]);

    Encryptor::Key key1, key2, key3, key4;

    EXPECT_TRUE(key1.DeriveFromPassword("test", "salt"));
    EXPECT_TRUE(key2.DeriveFromPassword("test", "salt"));

    EXPECT_TRUE(key3.DeriveFromPassword("test", "salt2"));  // wrong salt
    EXPECT_TRUE(key4.DeriveFromPassword("test2", "salt"));  // wrong key

    EXPECT_TRUE(key1.IsAvailable());
    EXPECT_TRUE(key2.IsAvailable());
    EXPECT_TRUE(key3.IsAvailable());
    EXPECT_TRUE(key4.IsAvailable());

    string original(buf.get(), kSizeTable[i]);

    // enfoce to copy. disable reference counting
    string encrypted(original.data(), original.size());

    EXPECT_TRUE(Encryptor::EncryptString(key1, &encrypted));
    EXPECT_EQ(0, encrypted.size() % key1.block_size());
    EXPECT_NE(encrypted, original);

    string encrypted3(encrypted.data(), encrypted.size());
    string encrypted4(encrypted.data(), encrypted.size());

    EXPECT_TRUE(Encryptor::DecryptString(key2, &encrypted));
    EXPECT_EQ(original.size(), encrypted.size());
    EXPECT_EQ(original, encrypted);

    // wrong key
    Encryptor::DecryptString(key3, &encrypted3);
    Encryptor::DecryptString(key4, &encrypted4);
    EXPECT_NE(original, encrypted3);
    EXPECT_NE(original, encrypted4);

    // invalid values
    EXPECT_FALSE(Encryptor::EncryptString(key1, NULL));
    EXPECT_FALSE(Encryptor::DecryptString(key1, NULL));

    // empty string
    string empty_string;
    EXPECT_FALSE(Encryptor::EncryptString(key1, &empty_string));
    EXPECT_FALSE(Encryptor::DecryptString(key1, &empty_string));
  }
}

TEST(EncryptorTest, ProtectData) {
  kUseMockPasswordManager = true;
  Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
  const size_t kSizeTable[] = { 1, 10, 100, 1000, 10000, 100000 };

  for (size_t i = 0; i < arraysize(kSizeTable); ++i) {
    scoped_array<char> buf(new char[kSizeTable[i]]);
    Util::GetSecureRandomSequence(buf.get(), kSizeTable[i]);
    string input(buf.get(), kSizeTable[i]);
    string output;
    EXPECT_TRUE(Encryptor::ProtectData(input, &output));
    EXPECT_NE(input, output);
    string result;
    EXPECT_TRUE(Encryptor::UnprotectData(output, &result));
    EXPECT_EQ(result, input);
  }
}
}  // namespace mozc
