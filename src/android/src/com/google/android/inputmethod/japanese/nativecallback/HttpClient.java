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

package org.mozc.android.inputmethod.japanese.nativecallback;

import org.apache.http.Header;
import org.apache.http.HttpResponse;
import org.apache.http.client.ClientProtocolException;
import org.apache.http.client.methods.HttpGet;
import org.apache.http.client.methods.HttpHead;
import org.apache.http.client.methods.HttpPost;
import org.apache.http.client.methods.HttpUriRequest;
import org.apache.http.entity.ByteArrayEntity;
import org.apache.http.impl.client.AbstractHttpClient;
import org.apache.http.impl.client.DefaultHttpClient;
import org.apache.http.util.EntityUtils;

import java.io.IOException;
import java.nio.ByteBuffer;

/**
 * A http client class used by nativa layer instead of cURL.
 *
 */
public class HttpClient {
  private static volatile AbstractHttpClient httpClient;

  private HttpClient() {
  }

  private static AbstractHttpClient getHttpClient() {
    AbstractHttpClient client = httpClient;
    if (client == null) {
      synchronized (HttpClient.class) {
        if (httpClient == null) {
          client = httpClient = new DefaultHttpClient();
        }
      }
    }
    return client;
  }

  /**
   * Creates a request instance.
   *
   * All the parameters are the same of {@link #request(byte[], byte[], byte[])}'s.
   * @return a request instance
   * @throw IllegalArgumentException thrown if unacceptable method is passed
   */
  static HttpUriRequest createRequest(String method, String url, byte[] postData) {
    if (method.equals("GET")) {
      return new HttpGet(url);
    } else if (method.equals("HEAD")) {
      return new HttpHead(url);
    } else if (method.equals("POST")) {
      HttpPost postRequest = new HttpPost(url);
      postRequest.setHeader("Content-type", "text/plain");
      ByteArrayEntity entiry = new ByteArrayEntity(postData);
      postRequest.setEntity(entiry);
      return postRequest;
    }
    throw new IllegalArgumentException(
        "method " + method + " is invalid (must be GET, HEAD or POST).");
  }

  static class ResponseGetPostHandler
      implements org.apache.http.client.ResponseHandler<ByteBuffer> {
    // Use static initializer instead of double-check ideom
    // because this class is loaded when Request method is invoked.
    // This is typically not boot-up time frame
    // so lazy evaluation is not needed.
    private static final ResponseGetPostHandler instance = new ResponseGetPostHandler();

    private ResponseGetPostHandler() {
    }

    @Override
    public ByteBuffer handleResponse(HttpResponse response)
        throws ClientProtocolException, IOException {
      return ByteBuffer.wrap(EntityUtils.toByteArray(response.getEntity()));
    }

    public static ResponseGetPostHandler getInstance() {
      return instance;
    }
  }

  /**
  * For compatibility with cURL, we should do special thing on HEAD method.
  */
  static class ResponseHeadHandler implements org.apache.http.client.ResponseHandler<ByteBuffer> {
    private static final ResponseHeadHandler instance = new ResponseHeadHandler();

    private ResponseHeadHandler() {
    }

    @Override
    public ByteBuffer handleResponse(HttpResponse response) throws IOException {
      // cURL returns response header string as an entity for HEAD method's response.
      // Emulate here the behavior.
      StringBuilder result = new StringBuilder();
      result.append(response.getStatusLine().toString()).append('\n');
      for (Header header : response.getAllHeaders()) {
        result.append(header.getName()).append(": ").append(header.getValue()).append('\n');
      }
      result.append('\n');
      return ByteBuffer.wrap(result.toString().getBytes("UTF-8"));
    }

    public static ResponseHeadHandler getInstance() {
      return instance;
    }
  }

  /**
   * Processes a request (typically) given from native layer.
   * @param method "GET", "POST" or "HEAD".
   * @param url a URL string.
   * @param postData a byte array for data of POST method or null for GET or HEAD methods
   * @return an byte array of the result
   */
  public static byte[] request(byte[] method, byte[] url, byte[] postData)
      throws ClientProtocolException, IOException {
    AbstractHttpClient httpClient = getHttpClient();
    HttpUriRequest request =
        createRequest(new String(method, "utf-8"), new String(url, "utf-8"), postData);
    ByteBuffer response =
        httpClient.execute(request,
                           "HEAD".equals(request.getMethod())
                               ? ResponseHeadHandler.getInstance()
                               : ResponseGetPostHandler.getInstance());
    return response.array();
  }
}
