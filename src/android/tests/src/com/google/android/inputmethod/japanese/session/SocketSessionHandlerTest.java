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

import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Command;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.CommandType;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Output;

import junit.framework.TestCase;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.UnknownHostException;

/**
 */
public class SocketSessionHandlerTest extends TestCase {
  public void testSessionHandlerSocket()
      throws InterruptedException, UnknownHostException, IOException {
    final InetAddress host = InetAddress.getByName("localhost");
    final ServerSocket serverSocket = new ServerSocket(0);
    final int port = serverSocket.getLocalPort();
    try {
      Thread handlerThread = new Thread(new Runnable() {
        @Override
        public void run() {
          try {
            // Reply for 1st message.
            {
              Socket socket = serverSocket.accept();
              DataInputStream inputStream = new DataInputStream(socket.getInputStream());
              byte[] inBytes = new byte[inputStream.readInt()];
              inputStream.readFully(inBytes);
              Input input = Input.parseFrom(inBytes);
              assertEquals(CommandType.CREATE_SESSION, input.getType());
              Output output = Output.newBuilder().setId(1).build();
              DataOutputStream outStream = new DataOutputStream(socket.getOutputStream());
              byte[] outBytes = output.toByteArray();
              outStream.writeInt(outBytes.length);
              outStream.write(outBytes);
              socket.close();
            }
            // Reply for 2nd message.
            {
              Socket socket = serverSocket.accept();
              DataInputStream inputStream = new DataInputStream(socket.getInputStream());
              byte[] inBytes = new byte[inputStream.readInt()];
              inputStream.readFully(inBytes);
              Input input = Input.parseFrom(inBytes);
              assertEquals(CommandType.RELOAD, input.getType());
              Output output = Output.newBuilder().setId(2).build();
              DataOutputStream outStream = new DataOutputStream(socket.getOutputStream());
              byte[] outBytes = output.toByteArray();
              outStream.writeInt(outBytes.length);
              outStream.write(outBytes);
              socket.close();
            }
            // Disconnect before arriving 3rd message.
            serverSocket.close();
          } catch (IOException e) {
            // Do nothing.
          }
        }
      });
      handlerThread.setDaemon(true);
      handlerThread.start();
      SocketSessionHandler handler = new SocketSessionHandler(host, port);
      // Send 1st message.
      {
        Command inCommand = Command.newBuilder()
            .setInput(Input.newBuilder().setType(CommandType.CREATE_SESSION))
            .setOutput(Output.getDefaultInstance())
            .build();
        Command outCommand = handler.evalCommand(inCommand);
        assertTrue(outCommand.getOutput().getId() == 1);
      }
      // Send 2nd message.
      {
        Command inCommand = Command.newBuilder()
            .setInput(Input.newBuilder().setType(CommandType.RELOAD))
            .setOutput(Output.getDefaultInstance())
            .build();
        Command outCommand = handler.evalCommand(inCommand);
        assertTrue(outCommand.getOutput().getId() == 2);
      }
      // Send 3rd message but socket client (mozc server) is disconnected.
      {
        try {
          handler.evalCommand(Command.getDefaultInstance());
          fail("An exception has to be thrown because the socket is closed.");
        } catch (RuntimeException e) {
          // Expected behavior.
        }
      }
    } finally {
      serverSocket.close();
    }
  }
}
