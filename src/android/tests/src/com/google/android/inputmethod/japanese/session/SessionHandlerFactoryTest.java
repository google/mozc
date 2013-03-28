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

package org.mozc.android.inputmethod.japanese.session;

import org.mozc.android.inputmethod.japanese.testing.VisibilityProxy;

import android.app.Activity;
import android.content.SharedPreferences;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import java.io.IOException;
import java.net.ServerSocket;
import java.net.Socket;

/**
 */
public class SessionHandlerFactoryTest extends InstrumentationTestCase {
  private SharedPreferences sharedPreferences;
  private ServerSocket serverSocket;
  private static final int PORT = 18181;
  private static final String HOST_ADDRESS = "localhost";

  static class ServerThread extends Thread {
    private final ServerSocket serverSocket;
    public ServerThread(ServerSocket serverSocket) {
      this.serverSocket = serverSocket;
    }
    @Override
    public void run() {
      try {
        Socket socket = serverSocket.accept();
      } catch (IOException e) {
        // Do nothing.
      }
    }
  }

  @Override
  protected void setUp() throws Exception {
    super.setUp();
    sharedPreferences = getInstrumentation().getContext().getSharedPreferences(
        "TEST_SESSION_HANLDER_FACTORY", Activity.MODE_PRIVATE);
    sharedPreferences.edit().clear().commit();
    serverSocket = new ServerSocket(PORT);
    // Prepare stub socket server.
    Thread handlerThread = new ServerThread(serverSocket);
    handlerThread.setDaemon(true);
    handlerThread.start();
  }

  @Override
  protected void tearDown() throws Exception {
    serverSocket.close();
    sharedPreferences.edit().clear().commit();
    super.tearDown();
  }

  @SmallTest
  public void testCreateSessionHandler_nullParameter() {
    // SessionHandler instance for null parameter.
    assertEquals(
        LocalSessionHandler.class,
        new SessionHandlerFactory(SharedPreferences.class.cast(null)).create().getClass());
  }

  @SmallTest
  public void testCreateSessionHandler_appropriateParameter() {
      // SessionHandlerSocketClient for appropriate preference.
    SharedPreferences.Editor editor = sharedPreferences.edit();
    editor.putBoolean(
        VisibilityProxy.<String>getStaticField(SessionHandlerFactory.class,
            "PREF_TWEAK_USE_SOCKET_SESSION_HANDLER_KEY"),
        true);
    editor.putString(
        VisibilityProxy.<String>getStaticField(SessionHandlerFactory.class,
            "PREF_TWEAK_SOCKET_SESSION_HANDLER_ADDRESS_KEY"),
        HOST_ADDRESS);
    editor.putString(
        VisibilityProxy.<String>getStaticField(SessionHandlerFactory.class,
            "PREF_TWEAK_SOCKET_SESSION_HANDLER_PORT_KEY"),
        Integer.toString(PORT));
    editor.commit();
    assertEquals(SocketSessionHandler.class,
                 new SessionHandlerFactory(sharedPreferences).create().getClass());
  }

  @SmallTest
  public void testCreateSessionHandler_nonexistentServer() {
    // SessionHandler instance if the server does not exist.
    SharedPreferences.Editor editor = sharedPreferences.edit();
    editor.putBoolean(
        VisibilityProxy.<String>getStaticField(SessionHandlerFactory.class,
            "PREF_TWEAK_USE_SOCKET_SESSION_HANDLER_KEY"),
        true);
    editor.putString(
        VisibilityProxy.<String>getStaticField(SessionHandlerFactory.class,
            "PREF_TWEAK_SOCKET_SESSION_HANDLER_ADDRESS_KEY"),
        "NEVER.EXIST");
    editor.putString(
        VisibilityProxy.<String>getStaticField(SessionHandlerFactory.class,
            "PREF_TWEAK_SOCKET_SESSION_HANDLER_PORT_KEY"),
        Integer.toString(PORT));
    editor.commit();
    assertEquals(LocalSessionHandler.class,
                 new SessionHandlerFactory(sharedPreferences).create().getClass());
  }

  @SmallTest
  public void testCreateSessionHandler_malformedPort() {
    // SessionHandler instance for malformed port number.
    SharedPreferences.Editor editor = sharedPreferences.edit();
    editor.putBoolean(
        VisibilityProxy.<String>getStaticField(SessionHandlerFactory.class,
            "PREF_TWEAK_USE_SOCKET_SESSION_HANDLER_KEY"),
        true);
    editor.putString(
        VisibilityProxy.<String>getStaticField(SessionHandlerFactory.class,
            "PREF_TWEAK_SOCKET_SESSION_HANDLER_ADDRESS_KEY"),
        HOST_ADDRESS);
    editor.putString(
        VisibilityProxy.<String>getStaticField(SessionHandlerFactory.class,
            "PREF_TWEAK_SOCKET_SESSION_HANDLER_PORT_KEY"),
        "MALFORMED");
    editor.commit();
    assertEquals(LocalSessionHandler.class,
                 new SessionHandlerFactory(sharedPreferences).create().getClass());
  }
}
