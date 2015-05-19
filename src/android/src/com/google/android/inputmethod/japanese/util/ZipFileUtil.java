// Copyright 2010-2013, Google Inc.
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

package org.mozc.android.inputmethod.japanese.util;

import org.mozc.android.inputmethod.japanese.MozcUtil;

import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.ByteBuffer;
import java.nio.MappedByteBuffer;
import java.nio.channels.Channels;
import java.nio.channels.FileChannel;
import java.nio.channels.FileChannel.MapMode;
import java.nio.channels.ReadableByteChannel;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

/**
 * This class is used to access the flatten image in a zip file via {@link ByteBuffer}.
 *
 * If the target entry is compressed, this class allocates required memory block and
 * uncompresses the entry into the memory block. Therefore {@link #getBuffer(ZipFile, String)}
 * method blocks a moment.
 * If the target entry in "uncompressed", this class returns {@link MappedByteBuffer}
 * of which back-end is the uncompressed target entry.
 * In this case {@link #getBuffer(ZipFile, String)} blocks very little.
 *
 * We implemented this class instead of using ZipFile class because
 * ZipFile lacks the API which can return the offset value of each entry.
 * Such API is mandatory to implement the behavior for uncompressed entry described above.
 *
 */
public class ZipFileUtil {
  // Following constants come from ZipFile class.
  // Android framework's ZipFile class lacks these constants so we define them by ourselves.
  private static final long LOCSIG = 0x0000000004034b50;
  private static final int LOCNAM = 26;
  private static final int LOCEXT = 28;
  private static final int LOCHDR = 30;
  private static final long CENSIG = 0x0000000002014b50;
  private static final int CENSIZ = 20;
  private static final int CENNAM = 28;
  private static final int CENEXT = 30;
  private static final int CENCOM = 32;
  private static final int CENOFF = 42;
  private static final int CENHDR = 46;
  private static final long ENDSIG = 0x0000000006054b50;
  private static final int ENDOFF = 16;
  private static final int ENDHDR = 22;

  static class RandomAccessFileUtility {
    private RandomAccessFileUtility() {
      // Explicitly throw an exception because "private constructor" idiom cannot
      // be applicable here.
      throw new IllegalAccessError("This class should not instantiated.");
    }

    static short readPositiveShort(RandomAccessFile file, long offset) throws IOException {
      file.seek(offset);
      short result = Short.reverseBytes(file.readShort());
      if (result < 0) {
        throw new IOException(
            "Unexpected negative value ; offset = " + offset + ", value = " + result);
      }
      return result;
    }

    static int readPositiveInt(RandomAccessFile file, long offset) throws IOException {
      int result = readInt(file, offset);
      if (result < 0) {
        throw new IOException(
            "Unexpected negative value ; offset = " + offset + ", value = " + result);
      }
      return result;
    }

    static int readInt(RandomAccessFile file, long offset) throws IOException {
      file.seek(offset);
      return Integer.reverseBytes(file.readInt());
    }

    static void readBytes(RandomAccessFile file, long offset, byte[] result, int length)
        throws IOException {
      file.seek(offset);
      file.read(result, 0, length);
    }
  }

  /**
   * See http://www.pkware.com/documents/casestudies/APPNOTE.TXT
   * I.  End of central directory record
   */
  private static class EndOfCentralDirectory {
    private final RandomAccessFile file;
    EndOfCentralDirectory(RandomAccessFile file) throws IOException {
      this.file = file;
      // Check the signature.
      if (RandomAccessFileUtility.readInt(file, file.length() - ENDHDR) != ENDSIG) {
        throw new IOException("Invalid ENDHDR signature at " + (file.length() - ENDHDR));
      }
    }

    CentralDirectory getCentralDirectory() throws IOException {
      // NOTE : Expects that there is no zip-file-comment at the tail of the file
      //        becase aapt tool does not create such file which has "zip-file-comment" entry.
      int centralDirectoryOffset =
          RandomAccessFileUtility.readPositiveInt(file, file.length() - ENDHDR + ENDOFF);
      return new CentralDirectory(file, centralDirectoryOffset);
    }
  }

  /**
   * See http://www.pkware.com/documents/casestudies/APPNOTE.TXT
   * F.  Central directory structure
   */
  private static class CentralDirectory {
    private final RandomAccessFile file;
    private final int centralDirectoryOffset;
    // The length is intentionally 256 (not 2^15) in order to prevent from creating large array.
    private static final int MAX_FILE_NAME_LENGTH = 256;
    CentralDirectory(RandomAccessFile file, int offset) {
      this.file = file;
      this.centralDirectoryOffset = offset;
    }

    LocalDirectory getLocalDirectory(String fileName) throws IOException {
      long offset = this.centralDirectoryOffset;
      byte[] temp = new byte[MAX_FILE_NAME_LENGTH];
      while (true) {
        // Check the signature.
        if (RandomAccessFileUtility.readInt(file, offset) != CENSIG) {
          throw new IOException("Invalid CENSIG signature at " + offset);
        }
        int fileNameLength = RandomAccessFileUtility.readPositiveShort(file, offset + CENNAM);
        if (fileNameLength > MAX_FILE_NAME_LENGTH) {
          throw new IOException("Too long file name length ;" + fileNameLength);
        }
        RandomAccessFileUtility.readBytes(file, offset + CENHDR, temp, fileNameLength);
        if (new String(temp, 0, fileNameLength, "UTF-8").equals(fileName)) {
          int localOffset = RandomAccessFileUtility.readPositiveInt(file, offset + CENOFF);
          int rawLength = RandomAccessFileUtility.readPositiveInt(file, offset + CENSIZ);
          return new LocalDirectory(file, localOffset, rawLength);
        } else {
          int extLength = RandomAccessFileUtility.readPositiveShort(file, offset + CENEXT);
          int commentLength = RandomAccessFileUtility.readPositiveShort(file, offset + CENCOM);
          offset += CENHDR + fileNameLength + extLength + commentLength;
        }
      }
    }
  }

  /**
   * See http://www.pkware.com/documents/casestudies/APPNOTE.TXT
   * A.  Local file header
   */
  private static class LocalDirectory {
    private final RandomAccessFile file;
    private final long offset;
    private final int size;

    LocalDirectory(RandomAccessFile file, long offset, int size) throws IOException {
      this.file = file;
      this.offset = offset;
      this.size = size;
      // Check the signature.
      if (RandomAccessFileUtility.readInt(file, offset) != LOCSIG) {
        throw new IOException("Invalid LOCSIG signature at " + offset);
      }
    }

    MappedByteBuffer getReadOnlyMappedByteBuffer() throws IOException {
      int fileNameLength = RandomAccessFileUtility.readPositiveShort(file, offset + LOCNAM);
      int extLength = RandomAccessFileUtility.readPositiveShort(file, offset + LOCEXT);
      FileChannel channel = file.getChannel();
      boolean succeeded = false;
      try {
        MappedByteBuffer result =
            channel.map(MapMode.READ_ONLY,
                        offset + LOCHDR + fileNameLength + extLength, size);
        succeeded = true;
        return result;
      } finally {
        // MappedByteBuffer is valid even after back-end Channel and/or RandomAccessFile
        // is/are closed (documented spec).
        MozcUtil.close(channel, !succeeded);
      }
    }
  }

  /**
   * Gets a {@link ByteBuffer} to the entry's content.
   *
   * If the entry is compressed, returned buffer contains uncompressed data.
   * See the class's document.
   *
   * @param zipFile the ZipFile to use
   * @param fileName the file name to access
   * @return the ByteBuffer
   * @throws IOException
   */
  public static ByteBuffer getBuffer(ZipFile zipFile, String fileName) throws IOException {
    ZipEntry zipEntry = zipFile.getEntry(fileName);
    if (zipEntry == null) {
      throw new IOException(fileName + " is not found.");
    }
    switch (zipEntry.getMethod()) {
      case ZipEntry.DEFLATED:
        return getBufferDelfated(zipFile, zipEntry);
      case ZipEntry.STORED:
        return getBufferStored(zipFile, zipEntry);
      default:
        throw new IOException("Unsuppoerted storing method.");
    }
  }

  private static ByteBuffer getBufferStored(ZipFile zipFile, ZipEntry entry) throws IOException {
    RandomAccessFile apkFile = new RandomAccessFile(zipFile.getName(), "r");
    boolean succeeded = false;
    try {
      CentralDirectory centralDirectory = new EndOfCentralDirectory(apkFile).getCentralDirectory();
      LocalDirectory localDirectory = centralDirectory.getLocalDirectory(entry.getName());
      ByteBuffer result = localDirectory.getReadOnlyMappedByteBuffer();
      succeeded = true;
      return result;
    } finally {
      // MappedByteBuffer is valid even after back-end Channel and/or RandomAccessFile
      // is/are closed (documented spec).
      MozcUtil.close(apkFile, !succeeded);
    }
  }

  private static ByteBuffer getBufferDelfated(ZipFile zipFile, ZipEntry entry) throws IOException {
    int size = (int) entry.getSize();
    ByteBuffer buffer = ByteBuffer.allocateDirect(size);
    ReadableByteChannel inputChannel = Channels.newChannel(zipFile.getInputStream(entry));
    boolean succeeded = false;
    try {
      while (buffer.position() != size) {
        inputChannel.read(buffer);
      }
      succeeded = true;
    } finally {
      MozcUtil.close(inputChannel, !succeeded);
    }
    return buffer;
  }
}
