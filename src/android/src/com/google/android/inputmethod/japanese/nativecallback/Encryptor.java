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

import java.security.InvalidAlgorithmParameterException;
import java.security.InvalidKeyException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Arrays;

import javax.crypto.BadPaddingException;
import javax.crypto.Cipher;
import javax.crypto.IllegalBlockSizeException;
import javax.crypto.NoSuchPaddingException;
import javax.crypto.spec.IvParameterSpec;
import javax.crypto.spec.SecretKeySpec;

/**
 * AES/CBC encryptor implementation.
 *
 * This class is designed to be invoked from native layers, in order to use Java library
 * whose functionalities are not included by c libraries on android instead of
 * including libraries built by ourselves because of package size.
 *
 * See also {@code JavaEncryptorProxy} class in code base/android_util. It is a proxy class
 * to invoke static methods in this class from mozc server.
 *
 */
public class Encryptor {
  // Note: the same constant is defined in c++.
  private static final int KEY_SIZE_IN_BITS = 256;

  // Encryption algorithm used in mozc server on other platforms.
  private static final String TRANSFORMATION_TYPE = "AES/CBC/PKCS5Padding";

  // Disallow instantiation.
  private Encryptor() {
  }

  /**
   * Makes a session key from the given password and salt.
   */
  public static byte[] deriveFromPassword(byte[] password, byte[] salt)
      throws NoSuchAlgorithmException {
    return GetMsCryptDeriveKeyWithSha1(password, salt);
  }

  /**
   * Ported from base/encryptor.cc. See it for details.
   * @throws NoSuchAlgorithmException if SHA-1 is not supported.
   */
  private static byte[] GetMsCryptDeriveKeyWithSha1(byte[] password, byte[] salt)
      throws NoSuchAlgorithmException {
    // Step 1.
    byte[] buf1 = new byte[64];
    Arrays.fill(buf1, (byte) 0x36);

    // Step 2.
    byte[] buf2 = new byte[64];
    Arrays.fill(buf2, (byte) 0x5C);

    // Step 3 & 4.
    MessageDigest sha1 = MessageDigest.getInstance("SHA-1");
    byte[] hash = sha1.digest(concatByteArray(password, salt));
    for (int i = 0; i < hash.length; ++i) {
      byte h = hash[i];
      buf1[i] ^= h;
      buf2[i] ^= h;
    }

    // Step 5 & 6.
    sha1.reset();
    byte[] result1 = sha1.digest(buf1);
    sha1.reset();
    byte[] result2 = sha1.digest(buf2);

    // SHA-1's result is 160 bits, so result1 + result2 length is 320 bits,
    // which should be larger than expected key bits.
    byte[] result = new byte[KEY_SIZE_IN_BITS / 8];
    System.arraycopy(result1, 0, result, 0, result1.length);
    System.arraycopy(result2, 0, result, result1.length, result.length - result1.length);
    return result;
  }

  /**
   * Concatenates given two arrays.
   */
  private static byte[] concatByteArray(byte[] array1, byte[] array2) {
    byte[] result = new byte[array1.length + array2.length];
    System.arraycopy(array1, 0, result, 0, array1.length);
    System.arraycopy(array2, 0, result, array1.length, array2.length);
    return result;
  }

  /**
   * Encrypts {@code data}, based on given {@code key} and {@code iv}.
   * The used algorithm is AES in CBC mode with PKCS #5 padding.
   */
  public static byte[] encrypt(byte[] data, byte[] key, byte[] iv)
      throws InvalidKeyException, IllegalBlockSizeException, BadPaddingException,
             NoSuchAlgorithmException, NoSuchPaddingException, InvalidAlgorithmParameterException {
    return getCipher(Cipher.ENCRYPT_MODE, key, iv).doFinal(data);
  }

  /**
   * Decrypts {@code data}, based on given {@code key} and {@code iv}.
   * The used algorithm is AES in CBC mode with PKCS #5 padding.
   */
  public static byte[] decrypt(byte[] data, byte[] key, byte[] iv)
      throws InvalidKeyException, IllegalBlockSizeException, BadPaddingException,
             NoSuchAlgorithmException, NoSuchPaddingException, InvalidAlgorithmParameterException {
    return getCipher(Cipher.DECRYPT_MODE, key, iv).doFinal(data);
  }

  /**
   * @returns an initialized crypting instance.
   */
  private static Cipher getCipher(int mode, byte[] key, byte[] iv)
      throws NoSuchAlgorithmException, NoSuchPaddingException, InvalidKeyException,
             InvalidAlgorithmParameterException {
    Cipher cipher = Cipher.getInstance(TRANSFORMATION_TYPE);
    cipher.init(mode, new SecretKeySpec(key, "AES"), new IvParameterSpec(iv));
    return cipher;
  }
}
