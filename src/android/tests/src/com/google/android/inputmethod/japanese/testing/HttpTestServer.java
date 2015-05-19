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

package org.mozc.android.inputmethod.japanese.testing;

import org.mozc.android.inputmethod.japanese.MozcUtil;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.ByteArrayOutputStream;
import java.io.Closeable;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.HashMap;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Simple HTTP server for testing.
 *
 * Many restrictions because this is for testing purpose.
 * <ul>
 * <li>Only GET, HEAD and POST methods are supported.
 * <li>Duplicate header entities are not acceptable.
 * </ul>
 * For GET request, the returned content would be
 * <pre>
 * method
 * path
 * </pre>
 * For example the result of "http://localhost/aaa?bbb=ccc" will be
 * <pre>
 * GET
 * /aaa?bbb=ccc
 * </pre>
 *
 * HEAD is the same as GET but no content will return.
 * POST's result content is the same as the request's content.
 *
 */
public class HttpTestServer implements Closeable, Runnable {
  private static final Pattern HEADER_PATTERN = Pattern.compile("^(.*?): (.*)$");
  public static final String COMMON_HEADER = "HTTP/1.0 200 OK\nContent-Type: text/plain";
  private static final int BUFFER_SIZE = 1024;
  private ServerSocket server;
  private boolean shutdownRequested = false;

  public HttpTestServer() throws IOException {
    server = new ServerSocket(0);
  }

  public int getPort() {
    return server.getLocalPort();
  }

  @Override
  public void close() {
    shutdownRequested = true;
    try {
      MozcUtil.close(server, true);
    } catch (IOException e) {
      // Never reach here because MozcUtil.close() doesn't throw IOException.
    }
  }

  private Map<String, String> parseHeader(BufferedReader reader) throws IOException {
    String line;
    Map<String, String> result = new HashMap<String, String>(10);
    while((line = reader.readLine()) != null && !line.equals("")) {
      Matcher matcher = HEADER_PATTERN.matcher(line);
      if (matcher.find() && result.put(matcher.group(1), matcher.group(2)) != null) {
        throw new AssertionError("Duplicate headers are not acceptable. Fix test case.");
      }
    }
    return result;
  }

  private String[] parseFirstLine(BufferedReader reader) throws IOException {
    String firstLine = reader.readLine();
    String[] chunks = firstLine.split(" ");
    String method = chunks[0];
    String path = chunks[1];
    return new String[] {method, path};
  }

  private byte[] getContent(
      BufferedReader reader, String method, String path, Map<String, String> headers)
          throws IOException {
    if (method.equals("GET")) {
      ByteArrayOutputStream byteOutputStream = new ByteArrayOutputStream();
      BufferedWriter writer =
          new BufferedWriter(new OutputStreamWriter(byteOutputStream, "utf-8"));
      try {
        writer.write(method);
        writer.newLine();
        writer.write(path);
      } finally {
        MozcUtil.close(writer, true);
      }
      return byteOutputStream.toByteArray();
    } else if (method.equals("POST")) {
      int contentLength = Integer.parseInt(headers.get("Content-Length"));
      byte[] content = new byte[contentLength];
      for (int i = 0; i < contentLength; ++i) {
        content[i] = (byte) reader.read();
      }
      return content;
    }
    return new byte[0];
  }

  private void writeHeaders(
      BufferedWriter writer, String method, String path, Map<String, String> headers)
          throws IOException {
    if (method.equals("GET") || method.equals("HEAD")) {
      writer.write(COMMON_HEADER);
      writer.newLine();
    } else if (method.equals("POST")) {
      writer.write(COMMON_HEADER);
      writer.newLine();
      writer.write("Content-Length: " + headers.get("Content-Length"));
      writer.newLine();
    }
    writer.newLine();
    writer.flush();
  }

  /**
   * @param outputStream
   * @param method
   * @throws IOException
   */
  private void writeContent(OutputStream outputStream, String method, String path, byte[] content)
      throws IOException {
    if (method.equals("GET")) {
      ByteArrayOutputStream byteOutputStream = new ByteArrayOutputStream();
      BufferedWriter writer =
          new BufferedWriter(new OutputStreamWriter(byteOutputStream, "utf-8"));
      try {
        writer.write(method);
        writer.newLine();
        writer.write(path);
      } finally {
        MozcUtil.close(writer, true);
      }
      outputStream.write(byteOutputStream.toByteArray());
    } else if (method.equals("POST")) {
      outputStream.write(content);
    }
  }

  @Override
  public void run() {
    while (true) {
      Socket client = null;
      InputStream inputStream = null;
      OutputStream outputStream = null;
      BufferedReader reader = null;
      BufferedWriter writer = null;
      byte[] content = null;
      try {
        try {
          client = server.accept();
        } catch (IOException e) {
          if (shutdownRequested) {
            // Normal path to close the server.
            return;
          }
          // Unexpected closure.
          throw new AssertionError(e);
        }
        inputStream = client.getInputStream();
        outputStream = client.getOutputStream();
        // Default buffer size (16KB) is rather large so we apply smaller buffer size.
        reader = new BufferedReader(new InputStreamReader(inputStream), BUFFER_SIZE);
        writer = new BufferedWriter(new OutputStreamWriter(outputStream), BUFFER_SIZE);

        String[] chunks = parseFirstLine(reader);
        String method = chunks[0];
        String path = chunks[1];
        Map<String, String> headers = parseHeader(reader);
        content = getContent(reader, method, path, headers);

        writeHeaders(writer, method, path, headers);
        writeContent(outputStream, method, path, content);
        outputStream.flush();
      } catch (IOException e) {
        throw new AssertionError(e);
      } finally {
        for (Closeable closable : new Closeable[] {reader, writer, inputStream, outputStream}) {
          if (closable != null) {
            try {
              MozcUtil.close(closable, true);
            } catch (IOException e) {
              // Never reach here because MozcUtil.close() doesn't throw IOException.
            }
          }
        }
        if (client != null) {
          try {
            MozcUtil.close(client, true);
          } catch (IOException e) {
            // Never reach here because MozcUtil.close() doesn't throw IOException.
          }
        }
      }
    }
  }

  /**
   * Starts the server on newly created daemon thread.
   *
   * @return newly created instance
   * @throws IOException
   */
  public static HttpTestServer startServer() throws IOException {
    HttpTestServer testServer = new HttpTestServer();
    Thread serverThread = new Thread(testServer);
    serverThread.setDaemon(true);
    serverThread.start();
    return testServer;
  }
}
