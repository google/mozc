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

import org.mozc.android.inputmethod.japanese.MozcUtil;
import org.mozc.android.inputmethod.japanese.testing.HttpTestServer;

import junit.framework.TestCase;

import org.apache.http.client.methods.HttpGet;
import org.apache.http.client.methods.HttpHead;
import org.apache.http.client.methods.HttpPost;
import org.apache.http.client.methods.HttpUriRequest;
import org.apache.http.util.EntityUtils;

import java.io.IOException;
import java.net.URI;

/**
 */
public class HttpClientTest extends TestCase {

  public void testCreateRequest() throws IOException {
    URI url = URI.create("http://URI");
    {
      // GET
      HttpUriRequest result = HttpClient.createRequest("GET", url.toString(), null);
      assertEquals(HttpGet.class, result.getClass());
      assertEquals("GET", result.getMethod());
      assertEquals(url, result.getURI());
    }
    {
      // HEAD
      HttpUriRequest result = HttpClient.createRequest("HEAD", url.toString(), null);
      assertEquals(HttpHead.class, result.getClass());
      assertEquals("HEAD", result.getMethod());
      assertEquals(url, result.getURI());
    }
    {
      // POST
      byte[] postData = new byte[] {1};
      HttpUriRequest result = HttpClient.createRequest("POST", url.toString(), postData);
      assertEquals(HttpPost.class, result.getClass());
      assertEquals("POST", result.getMethod());
      assertEquals(url, result.getURI());
      byte[] resultArray = EntityUtils.toByteArray(HttpPost.class.cast(result).getEntity());
      assertEquals(postData.length, resultArray.length);
      assertEquals(postData[0], resultArray[0]);
    }
    {
      // Unsupported method
      try {
        HttpUriRequest result = HttpClient.createRequest("PUT", url.toString(), null);
        fail("IllegalArgumentException is expected for PUT method.");
      } catch (IllegalArgumentException e) {
        // Expected.
      }
    }
  }

  public void testRequest() throws IOException {
    HttpTestServer server = HttpTestServer.startServer();
    int port = server.getPort();
    try {
      {
        byte[] result = HttpClient.request("GET".getBytes("utf-8"),
                                           ("http://localhost:" + port).getBytes("utf-8"),
                                           null);
        String s = new String(result, "utf-8");
        assertEquals("GET\n/", s);
      }
      {
        byte[] result = HttpClient.request("HEAD".getBytes("utf-8"),
                                           ("http://localhost:" + port).getBytes("utf-8"),
                                           null);
        String s = new String(result, "utf-8");
        assertEquals("HTTP/1.0 200 OK\nContent-Type: text/plain\n\n", s);
      }
      {
        byte[] result = HttpClient.request("POST".getBytes("utf-8"),
                                           ("http://localhost:" + port).getBytes("utf-8"),
                                           new byte[] {'a', 'b', 'c'});
        String s = new String(result, "utf-8");
        assertEquals("abc", s);
      }
    } finally {
      MozcUtil.close(server, true);
    }
  }

}
