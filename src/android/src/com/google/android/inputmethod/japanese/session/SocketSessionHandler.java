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

import org.mozc.android.inputmethod.japanese.MozcUtil;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Command;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Output;

import android.content.Context;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.Socket;

/**
 * A session handler which connect the Mozc server by using socket.
 *
 * The background of this class is described in server/mozc_rpc_server_main.cc
 *
 */
class SocketSessionHandler implements SessionHandler {
  // Time out duration for connection in milliseconds.
  private static final int CONNECTING_TIMEOUT = 1 * 1000;

  // Time out duration for reachability check in milliseconds.
  private static final int REACHABLITY_TIMEOUT = 1 * 100;

  private final InetAddress host;
  private final int port;
  SocketSessionHandler(InetAddress host, int port) {
    this.host = host;
    this.port = port;
  }

  @Override
  public void initialize(Context context) {
    // No operation.
  }

  @Override
  public Command evalCommand(Command command) {
    // We should be in a worker thread so below invocation is not needed. Just in case.
    MozcUtil.relaxMainthreadStrictMode();
    try {
      Socket socket = new Socket();
      boolean succeeded = false;
      try {
        socket.connect(new InetSocketAddress(host, port), CONNECTING_TIMEOUT);
        byte[] inBytes = command.getInput().toByteArray();
        DataOutputStream outputStream = new DataOutputStream(socket.getOutputStream());
        outputStream.writeInt(inBytes.length);  // Network order.
        outputStream.write(inBytes);
        DataInputStream inputStream = new DataInputStream(socket.getInputStream());
        byte[] outBytes = new byte[inputStream.readInt()];  // Network order.
        inputStream.readFully(outBytes);
        Command result = Command.newBuilder(command).setOutput(Output.parseFrom(outBytes)).build();
        succeeded = true;
        return result;
      } finally {
        // We just want to throw an IOException from close() method iff no method invocations
        // in this try block throws (another) IOException.
        MozcUtil.close(socket, !succeeded);
      }
    } catch (IOException e) {
      // Abort if an exception is thrown and re-launch the service.
      throw new RuntimeException(e);
    }
  }

  boolean isReachable() {
    MozcUtil.relaxMainthreadStrictMode();
    try {
      return host.isReachable(REACHABLITY_TIMEOUT);
    } catch (IOException e) {
      return false;
    }
  }
}
