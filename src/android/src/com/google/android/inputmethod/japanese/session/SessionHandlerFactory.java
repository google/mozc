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

package org.mozc.android.inputmethod.japanese.session;

import org.mozc.android.inputmethod.japanese.MozcLog;
import com.google.common.annotations.VisibleForTesting;

import android.content.Context;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;

import java.net.InetAddress;
import java.net.UnknownHostException;

/**
 * Factory class for SessionHandlerInterface.
 *
 * Basically SessionHandler is used as a session handler, which uses JNI.
 * But if SharedPreference needs to use RPC Mozc,
 * SessionHandlerSocketClient may be returned.
 *
 */
public class SessionHandlerFactory {
  @VisibleForTesting static final String PREF_TWEAK_USE_SOCKET_SESSION_HANDLER_KEY =
      "pref_tweak_use_socket_session_handler";
  @VisibleForTesting static final String PREF_TWEAK_SOCKET_SESSION_HANDLER_ADDRESS_KEY =
      "pref_tweak_socket_session_handler_address";
  @VisibleForTesting static final String PREF_TWEAK_SOCKET_SESSION_HANDLER_PORT_KEY =
      "pref_tweak_socket_session_handler_port";

  private final SharedPreferences sharedPreferences;

  public SessionHandlerFactory(Context context) {
    this(context == null ? null : PreferenceManager.getDefaultSharedPreferences(context));
  }

  /**
   * @param sharedPreferences the preferences. The type to be created is based on the preference.
   */
  public SessionHandlerFactory(SharedPreferences sharedPreferences) {
    this.sharedPreferences = sharedPreferences;
  }

  /**
   * Creates a session handler.
   */
  public SessionHandler create() {
    if (sharedPreferences != null &&
        sharedPreferences.getBoolean(PREF_TWEAK_USE_SOCKET_SESSION_HANDLER_KEY, false)) {
      try {
        MozcLog.i("Trying to connect to Mozc server via network");
        // If PREF_TWEAK_USE_SOCKET_SESSION_HANDLER_KEY is enabled,
        // try to use SessionHandlerSocketClient.
        // "10.0.2.2" is the host PC's address in emulator environment.
        // 8000 is the server's default port.
        int port =
            Integer.parseInt(sharedPreferences.getString(
                PREF_TWEAK_SOCKET_SESSION_HANDLER_PORT_KEY, "8000"));
        InetAddress hostAddress =
            InetAddress.getByName(sharedPreferences.getString(
                PREF_TWEAK_SOCKET_SESSION_HANDLER_ADDRESS_KEY, "10.0.2.2"));
        SocketSessionHandler socketSessionHandler = new SocketSessionHandler(hostAddress, port);
        if (socketSessionHandler.isReachable()) {
          MozcLog.i("We can reach " + hostAddress.getHostAddress() + ":" + port +
                    " so let's communicate via network");
          return socketSessionHandler;
        }
      } catch (NumberFormatException e) {
        // Do nothing.
        // SessionHandler instance will be used.
        MozcLog.i("Port number is malformed.");
      } catch (UnknownHostException e) {
        // Do nothing.
        // SessionHandler instance will be used.
        MozcLog.i("We cannot reach the host ");
      }
    }

    MozcLog.i("We use local Mozc engine.");
    return new LocalSessionHandler();
  }
}
