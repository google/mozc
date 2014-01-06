// Copyright 2010-2014, Google Inc.
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

package org.mozc.android.inputmethod.japanese.nativecallback;

import org.mozc.android.inputmethod.japanese.MozcUtil;

import android.test.MoreAsserts;
import android.test.suitebuilder.annotation.SmallTest;

import junit.framework.TestCase;

import java.io.EOFException;
import java.io.FileInputStream;
import java.io.IOException;
import java.security.InvalidAlgorithmParameterException;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;
import java.util.Arrays;

import javax.crypto.BadPaddingException;
import javax.crypto.IllegalBlockSizeException;
import javax.crypto.NoSuchPaddingException;

/**
 */
public class EncryptorTest extends TestCase {
  // Note: following test cases are ported from base/enryptor_test.cc

  private static final int BLOCK_SIZE = 16;

  // Ported from Util::GetSecureRandomSequence in base/util.cc
  private static byte[] getSecureRandomSequence(int size) throws IOException {
    byte[] buffer = new byte[size];
    FileInputStream inputStream = new FileInputStream("/dev/urandom");
    boolean succeeded = false;
    try {
      int currentPosition = 0;
      while (currentPosition < size) {
        int result = inputStream.read(buffer, currentPosition, size - currentPosition);
        if (result == -1) {
          throw new EOFException("EOF is found during reading /dev/urandom");
        }
        currentPosition += result;
      }

      succeeded = true;
    } finally {
      MozcUtil.close(inputStream, !succeeded);
    }
    return buffer;
  }

  private static byte[] newByteArray(int... values) {
    byte[] result = new byte[values.length];
    for (int i = 0; i < values.length; ++i) {
      int value = values[i];
      if ((value & ~0xFF) != 0) {
        throw new IllegalArgumentException("Non byte value is passed: " + value);
      }
      result[i] = (byte) values[i];
    }
    return result;
  }

  @SmallTest
  public void testVerification()
      throws NoSuchAlgorithmException, InvalidKeyException, IllegalBlockSizeException,
             BadPaddingException, NoSuchPaddingException, InvalidAlgorithmParameterException {
    class Parameter {
      final byte[] password;
      final byte[] salt;
      final byte[] iv;
      final byte[] input;
      final byte[] encrypted;

      public Parameter(byte[] password, byte[] salt, byte[] iv, byte[] input, byte[] encrypted) {
        this.password = password;
        this.salt = salt;
        this.iv = iv;
        this.input = input;
        this.encrypted = encrypted;
      }
    }
    Parameter[] parameters = {
        new Parameter(
            newByteArray(0x66, 0x6F, 0x6F, 0x68, 0x6F, 0x67, 0x65),
            newByteArray(0x73, 0x61, 0x6C, 0x74),
            newByteArray(0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31,
                         0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31),
            newByteArray(0x66, 0x6F, 0x6F),
            newByteArray(0x27, 0x32, 0x66, 0x88, 0x82, 0x33, 0x78, 0x80,
                         0x58, 0x29, 0xBF, 0xDD, 0x46, 0x9A, 0xCC, 0x87)),
        new Parameter(
            newByteArray(0x70, 0x61, 0x73, 0x73, 0x77, 0x6F, 0x72, 0x64),
            newByteArray(0x73, 0x61, 0x6C, 0x74),
            newByteArray(0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31,
                         0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31),
            newByteArray(0x61),
            newByteArray(0x2A, 0xA1, 0x73, 0xB0, 0x91, 0x1C, 0x22, 0x40,
                         0x55, 0xDB, 0xAB, 0xC0, 0x77, 0x39, 0xE6, 0x57)),
        new Parameter(
            newByteArray(0x70, 0x61, 0x73, 0x73, 0x77, 0x6F, 0x72, 0x64),
            newByteArray(0x73, 0x61, 0x6C, 0x74),
            newByteArray(0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31,
                         0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31),
            newByteArray(0x74, 0x65, 0x73, 0x74),
            newByteArray(0x13, 0x16, 0x0A, 0xA4, 0x2B, 0xA3, 0x02, 0xC4,
                         0xEF, 0x47, 0x98, 0x6D, 0x9F, 0xC9, 0xAD, 0x43)),
        new Parameter(
            newByteArray(0x70, 0x61, 0x73, 0x73, 0x77, 0x6F, 0x72, 0x64),
            newByteArray(0x73, 0x61, 0x6C, 0x74),
            newByteArray(0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
                         0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66),
            newByteArray(0x64, 0x68, 0x6F, 0x69, 0x66, 0x61, 0x73, 0x6F,
                         0x69, 0x66, 0x61, 0x6F, 0x69, 0x73, 0x68, 0x64,
                         0x6F, 0x69, 0x66, 0x61, 0x68, 0x73, 0x6F, 0x69,
                         0x66, 0x64, 0x68, 0x61, 0x6F, 0x69, 0x73, 0x68,
                         0x66, 0x69, 0x6F, 0x61, 0x73, 0x68, 0x64, 0x6F,
                         0x69, 0x66, 0x61, 0x68, 0x69, 0x73, 0x6F, 0x64,
                         0x66, 0x68, 0x61, 0x69, 0x6F, 0x73, 0x68, 0x64,
                         0x66, 0x69, 0x6F, 0x61),
            newByteArray(0x27, 0x92, 0xD1, 0x4F, 0xCE, 0x71, 0xFF, 0xA0,
                         0x9E, 0x52, 0xAB, 0x96, 0xB4, 0x5D, 0x1A, 0x2F,
                         0xE0, 0xC7, 0xB3, 0x92, 0xD7, 0xB8, 0x29, 0xB0,
                         0xEF, 0xD3, 0x51, 0x9F, 0xBD, 0x87, 0xE0, 0xB4,
                         0x0A, 0x06, 0xE0, 0x9A, 0x03, 0x72, 0x48, 0xB3,
                         0x8F, 0x9A, 0x5E, 0xAC, 0xCD, 0x5D, 0xB8, 0x0B,
                         0x01, 0x1D, 0x2C, 0xD7, 0xAA, 0x55, 0x05, 0x0F,
                         0x4E, 0xD5, 0x73, 0xC0, 0xCB, 0xE2, 0x10, 0x69)),
    };

    for (Parameter parameter : parameters) {
      byte[] key1 = Encryptor.deriveFromPassword(parameter.password, parameter.salt);
      byte[] key2 = Encryptor.deriveFromPassword(parameter.password, parameter.salt);

      byte[] encrypted = Encryptor.encrypt(parameter.input.clone(), key1, parameter.iv.clone());
      MoreAsserts.assertEquals(parameter.encrypted, encrypted);
      byte[] decrypted = Encryptor.decrypt(encrypted, key2, parameter.iv.clone());
      MoreAsserts.assertEquals(parameter.input, decrypted);
    }
  }

  @SmallTest
  public void testEncryptBatch()
      throws IOException, NoSuchAlgorithmException, InvalidKeyException,
             IllegalBlockSizeException, BadPaddingException, NoSuchPaddingException,
             InvalidAlgorithmParameterException {
    int[] parameters = { 1, 10, 16, 32, 100, 1000, 1600, 1000, 16000, 100000 };
    byte[] iv = new byte[16];
    for (int parameter : parameters) {
      byte[] buf = getSecureRandomSequence(parameter);

      byte[] key1 = Encryptor.deriveFromPassword(
          "test".getBytes("utf-8"), "salt".getBytes("utf-8"));
      byte[] key2 = Encryptor.deriveFromPassword(
          "test".getBytes("utf-8"), "salt".getBytes("utf-8"));

      byte[] key3 = Encryptor.deriveFromPassword(
          "test".getBytes("utf-8"), "salt2".getBytes("utf-8"));  // wrong salt.
      byte[] key4 = Encryptor.deriveFromPassword(
          "test2".getBytes("utf-8"), "salt".getBytes("utf-8"));  // wrong key.

      byte[] encrypted = Encryptor.encrypt(buf.clone(), key1, iv.clone());
      assertEquals(0, encrypted.length % BLOCK_SIZE);
      assertFalse(Arrays.equals(buf, encrypted));

      byte[] decrypted = Encryptor.decrypt(encrypted.clone(), key2, iv.clone());
      MoreAsserts.assertEquals(buf, decrypted);

      // Wrong key.
      {
        boolean decryptSucceeded = false;
        try {
          byte[] decrypted3 = Encryptor.decrypt(encrypted.clone(), key3, iv.clone());
          decryptSucceeded = Arrays.equals(buf, decrypted3);
        } catch (Exception e) {
          // Exception may be thrown by decrypting based on different key.
        }
        assertFalse(decryptSucceeded);
      }

      {
        boolean decryptSucceeded = false;
        try {
          byte[] decrypted4 = Encryptor.decrypt(encrypted.clone(), key4, iv.clone());
          decryptSucceeded = Arrays.equals(buf, decrypted4);
        } catch (Exception e) {
          // Exception may be thrown by decrypting based on different key.
        }
        assertFalse(decryptSucceeded);
      }
    }
  }
}
