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

import static org.mozc.android.inputmethod.japanese.testing.MozcMatcher.matchesBuilder;
import static org.easymock.EasyMock.anyLong;
import static org.easymock.EasyMock.capture;
import static org.easymock.EasyMock.expect;
import static org.easymock.EasyMock.getCurrentArguments;
import static org.easymock.EasyMock.isNull;
import static org.easymock.EasyMock.same;

import org.mozc.android.inputmethod.japanese.KeycodeConverter;
import org.mozc.android.inputmethod.japanese.KeycodeConverter.KeyEventInterface;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateList;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateWord;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Capability;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Capability.TextDeletionCapabilityType;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Command;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.CompositionMode;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Context.InputFieldType;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.DeletionRange;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.GenericStorageEntry;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.GenericStorageEntry.StorageType;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.CommandType;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchEvent;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.KeyEvent.SpecialKey;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Output;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Request;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Result;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Result.ResultType;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.SessionCommand;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoConfig.Config;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoUserDictionaryStorage.UserDictionaryCommand;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoUserDictionaryStorage.UserDictionaryCommandStatus;
import org.mozc.android.inputmethod.japanese.session.SessionExecutor.AsynchronousEvaluationContext;
import org.mozc.android.inputmethod.japanese.session.SessionExecutor.CallbackHandler;
import org.mozc.android.inputmethod.japanese.session.SessionExecutor.EvaluationCallback;
import org.mozc.android.inputmethod.japanese.session.SessionExecutor.ExecutorMainCallback;
import org.mozc.android.inputmethod.japanese.session.SessionExecutor.SynchronousEvaluationContext;
import org.mozc.android.inputmethod.japanese.stresstest.StressTest;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import org.mozc.android.inputmethod.japanese.testing.MemoryLogger;
import org.mozc.android.inputmethod.japanese.testing.Parameter;
import com.google.common.base.Strings;
import com.google.protobuf.ByteString;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.test.suitebuilder.annotation.LargeTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.KeyEvent;

import org.easymock.Capture;
import org.easymock.IAnswer;

import java.util.Collections;
import java.util.List;
import java.util.Random;
import java.util.concurrent.CountDownLatch;

/**
 *
 */
public class SessionExecutorTest extends InstrumentationTestCaseWithMock {
  private SessionExecutor createSessionExecutorMock() {
    return createMockBuilder(SessionExecutor.class)
        .addMockedMethods("evaluateSynchronously", "evaluateAsynchronously")
        .createMock();
  }

  @Override
  protected void tearDown() throws Exception {
    SessionExecutor.instance = null;
    super.tearDown();
  }

  @SmallTest
  public void testGetInstanceInitializedIfNecessary() {
    SessionHandlerFactory factory = createMockBuilder(SessionHandlerFactory.class)
        .withConstructor(Context.class)
        .withArgs(getInstrumentation().getTargetContext())
        .createMock();
    SessionHandler mockHandler = createNiceMock(SessionHandler.class);
    expect(factory.create()).andReturn(mockHandler);
    replayAll();

    SessionExecutor instance = SessionExecutor.getInstanceInitializedIfNecessary(factory, null);

    verifyAll();

    // We don't expect re-setting up for the second time.
    resetAll();
    replayAll();

    SessionExecutor instance2 = SessionExecutor.getInstanceInitializedIfNecessary(factory, null);

    verifyAll();

    // The same instance should be returned.
    assertSame(instance, instance2);
  }

  @SmallTest
  public void testCallbackInitializeSessionHandler() {
    SessionHandler sessionHandler = createMock(SessionHandler.class);
    sessionHandler.initialize(null);
    replayAll();

    ExecutorMainCallback callback = new ExecutorMainCallback(sessionHandler);
    Message message = new Message();
    message.what = ExecutorMainCallback.INITIALIZE_SESSION_HANDLER;
    assertTrue(callback.handleMessage(message));

    verifyAll();
  }

  @SmallTest
  public void testCallbackEnsureSession() {
    SessionHandler sessionHandler = createMock(SessionHandler.class);
    long sessionId = 1;
    Request initialRequest = Request.newBuilder()
        .setMixedConversion(true)
        .setZeroQuerySuggestion(true)
        .setAutoPartialSuggestion(true)
        .setUpdateInputModeFromSurroundingText(false)
        .build();
    {
      // Create session.
      Input input = Input.newBuilder()
          .setType(CommandType.CREATE_SESSION)
          .setCapability(Capability.newBuilder()
              .setTextDeletion(TextDeletionCapabilityType.DELETE_PRECEDING_TEXT))
          .build();
      Command inputCommand = Command.newBuilder()
          .setInput(input).setOutput(Output.getDefaultInstance()).build();
      Command outputCommand = Command.newBuilder()
          .setInput(input).setOutput(Output.newBuilder().setId(sessionId)).build();
      expect(sessionHandler.evalCommand(inputCommand)).andReturn(outputCommand);
    }
    {
      // Set request.
      Command inputCommand = Command.newBuilder()
          .setInput(Input.newBuilder()
              .setId(sessionId)
              .setType(CommandType.SET_REQUEST)
              .setRequest(initialRequest))
          .setOutput(Output.getDefaultInstance())
          .build();
      expect(sessionHandler.evalCommand(inputCommand)).andReturn(Command.getDefaultInstance());
    }
    replayAll();

    ExecutorMainCallback callback = new ExecutorMainCallback(sessionHandler);
    callback.ensureSession();

    verifyAll();

    resetAll();
    {
      // After createSession, sessionId and request should be automatically filled, for update()
      // method.
      Command inputCommand = Command.newBuilder()
          .setInput(Input.newBuilder()
              .setId(sessionId)
              .setType(CommandType.SET_REQUEST)
              .setRequest(initialRequest))
           .setOutput(Output.getDefaultInstance())
           .build();
      expect(sessionHandler.evalCommand(inputCommand)).andReturn(Command.getDefaultInstance());
    }
    replayAll();

    callback.updateRequest(Input.newBuilder());

    verifyAll();
  }

  @SmallTest
  public void testCallbackIsSquashable() {
    Output squashableOutput = Output.newBuilder()
        .setConsumed(true)
        .setAllCandidateWords(CandidateList.newBuilder()
            .addCandidates(CandidateWord.getDefaultInstance()))
        .build();

    assertTrue(ExecutorMainCallback.isSquashableOutput(squashableOutput));
    assertFalse(ExecutorMainCallback.isSquashableOutput(squashableOutput.toBuilder()
        .setResult(Result.newBuilder().setType(ResultType.NONE).setValue("abc"))
        .build()));
    assertFalse(ExecutorMainCallback.isSquashableOutput(squashableOutput.toBuilder()
        .setDeletionRange(DeletionRange.getDefaultInstance())
        .build()));
    assertFalse(ExecutorMainCallback.isSquashableOutput(squashableOutput.toBuilder()
        .setConsumed(false)
        .build()));
    assertFalse(ExecutorMainCallback.isSquashableOutput(squashableOutput.toBuilder()
        .clearAllCandidateWords()
        .build()));
    assertFalse(ExecutorMainCallback.isSquashableOutput(Output.getDefaultInstance()));
  }

  @SmallTest
  public void testIsSessionIdRequired() {
    assertFalse(ExecutorMainCallback.isSessionIdRequired(Input.newBuilder()
        .setType(CommandType.SET_CONFIG)));
    assertFalse(ExecutorMainCallback.isSessionIdRequired(Input.newBuilder()
        .setType(CommandType.SET_IMPOSED_CONFIG)));
    assertFalse(ExecutorMainCallback.isSessionIdRequired(Input.newBuilder()
        .setType(CommandType.GET_CONFIG)));

    assertTrue(ExecutorMainCallback.isSessionIdRequired(Input.newBuilder()
        .setType(CommandType.SEND_KEY)));
    assertTrue(ExecutorMainCallback.isSessionIdRequired(Input.newBuilder()
        .setType(CommandType.SEND_COMMAND)));
  }

  @SmallTest
  public void testCallbackEvaluateAsynchronously() {
    class TestData extends Parameter {
      final Output output;
      final boolean expectRequestSuggestion;
      final int expectedWhat;
      TestData(Output output, boolean expectRequestSuggestion, int expectedWhat) {
        this.output = output;
        this.expectRequestSuggestion = expectRequestSuggestion;
        this.expectedWhat = expectedWhat;
      }
    }
    TestData[] testData = new TestData[] {
        new TestData(Output.newBuilder()
            .setConsumed(true)
            .setAllCandidateWords(CandidateList.newBuilder()
                .addCandidates(CandidateWord.getDefaultInstance()))
            .build(),
            true,
            CallbackHandler.SQUASHABLE_OUTPUT),
        new TestData(Output.getDefaultInstance(), true, CallbackHandler.UNSQUASHABLE_OUTPUT),
        new TestData(Output.newBuilder()
            .setConsumed(true)
            .setAllCandidateWords(CandidateList.newBuilder()
                .addCandidates(CandidateWord.getDefaultInstance()))
            .build(),
            false,
            CallbackHandler.SQUASHABLE_OUTPUT),
        new TestData(Output.getDefaultInstance(), false, CallbackHandler.UNSQUASHABLE_OUTPUT),
    };
    Handler callbackHandler = createMock(Handler.class);
    Handler currentHandler = new Handler();
    SessionHandler sessionHandler = createMock(SessionHandler.class);
    for (TestData data : testData) {
      resetAll();

      Input.Builder inputBuilder = Input.newBuilder()
          .setType(CommandType.SEND_KEY)
          .setKey(ProtoCommands.KeyEvent.newBuilder()
              .setSpecialKey(SpecialKey.BACKSPACE));

      // Unfortunately we cannot mock the removeMessages because it is final method. Just skip it.

      // Note that Handler#obtainMessage and Handler#sendMessage cannot be mocked.
      // So, we just mock sendMessageAtTime method instead.
      Capture<Message> messageCapture = new Capture<Message>();
      expect(callbackHandler.sendMessageAtTime(capture(messageCapture), anyLong())).andReturn(true);

      AsynchronousEvaluationContext context =
          new AsynchronousEvaluationContext(0, inputBuilder, null, null, callbackHandler);
      long sessionId = 1;

      {
        Input.Builder builder = inputBuilder.clone();
        builder.setId(sessionId);
        if (!data.expectRequestSuggestion) {
          builder.setRequestSuggestion(false);
        }
        Command inputCommand = Command.newBuilder()
            .setInput(builder)
            .setOutput(Output.getDefaultInstance())
            .build();
        expect(sessionHandler.evalCommand(inputCommand)).andReturn(Command.newBuilder()
            .setInput(inputCommand.getInput())
            .setOutput(data.output)
            .build());
      }

      if (!data.expectRequestSuggestion) {
        currentHandler.sendEmptyMessage(ExecutorMainCallback.EVALUATE_KEYEVENT_ASYNCHRONOUSLY);
      }
      replayAll();

      ExecutorMainCallback callback = new ExecutorMainCallback(sessionHandler);
      callback.sessionId = sessionId;

      callback.evaluateAsynchronously(context, currentHandler);

      verifyAll();
      Message message = messageCapture.getValue();
      assertEquals(data.expectedWhat, message.what);
      assertSame(context, message.obj);
      assertNotNull(context.outCommand);
      currentHandler.removeMessages(ExecutorMainCallback.EVALUATE_KEYEVENT_ASYNCHRONOUSLY);
    }
  }

  @SmallTest
  public void testCallbackEvaluateSynchronously() {
    Input input = Input.newBuilder().setType(Input.CommandType.GET_CONFIG).build();
    CountDownLatch countDownLatch = new CountDownLatch(1);

    SynchronousEvaluationContext context = new SynchronousEvaluationContext(input, countDownLatch);

    SessionHandler sessionHandler = createMock(SessionHandler.class);
    {
      Command inputCommand = Command.newBuilder()
          .setInput(input).setOutput(Output.getDefaultInstance()).build();
      expect(sessionHandler.evalCommand(inputCommand)).andReturn(Command.getDefaultInstance());
    }
    replayAll();

    ExecutorMainCallback callback = new ExecutorMainCallback(sessionHandler);

    callback.evaluateSynchronously(context);

    verifyAll();
    assertEquals(0, countDownLatch.getCount());
    assertNotNull(context.outCommand);
  }

  @SmallTest
  public void testCallbackUpdateRequest() {
    Request.Builder initialRequest =
        Request.newBuilder().setMixedConversion(true).setZeroQuerySuggestion(true);
    Request diffRequest = Request.newBuilder().setZeroQuerySuggestion(false).build();
    Request updatedRequest = initialRequest.clone().mergeFrom(diffRequest).build();
    long sessionId = 1;
    List<TouchEvent> touchEventList = Collections.singletonList(TouchEvent.getDefaultInstance());

    SessionHandler sessionHandler = createMock(SessionHandler.class);
    {
      Command inputCommand = Command.newBuilder()
          .setInput(Input.newBuilder()
              .setId(sessionId)
              .setType(CommandType.SET_REQUEST)
              .setRequest(updatedRequest)
              .addAllTouchEvents(touchEventList))
          .setOutput(Output.getDefaultInstance())
          .build();
      expect(sessionHandler.evalCommand(inputCommand)).andReturn(Command.getDefaultInstance());
    }
    replayAll();

    ExecutorMainCallback callback = new ExecutorMainCallback(sessionHandler);
    callback.sessionId = sessionId;
    callback.request = initialRequest;

    callback.updateRequest(
        Input.newBuilder().setRequest(diffRequest).addAllTouchEvents(touchEventList));

    verifyAll();
    resetAll();
    {
      // Make sure that internal request is also updated by invoking updateRequest again.
      Command inputCommand = Command.newBuilder()
          .setInput(Input.newBuilder()
              .setId(sessionId)
              .setType(CommandType.SET_REQUEST)
              .setRequest(initialRequest))
           .setOutput(Output.getDefaultInstance())
           .build();
      expect(sessionHandler.evalCommand(inputCommand)).andReturn(Command.getDefaultInstance());
    }
    replayAll();
    callback.updateRequest(Input.newBuilder());
    verifyAll();
  }

  @SmallTest
  public void testCallbackHandler() {
    CallbackHandler callbackHandler = new CallbackHandler(Looper.myLooper());
    callbackHandler.cancelTimeStamp = 0;

    EvaluationCallback evaluationCallback = createMock(EvaluationCallback.class);
    evaluationCallback.onCompleted(null, null);
    replayAll();

    AsynchronousEvaluationContext context =
        new AsynchronousEvaluationContext(1, null, null, evaluationCallback, callbackHandler);
    Message message = callbackHandler.obtainMessage(CallbackHandler.UNSQUASHABLE_OUTPUT, context);
    callbackHandler.handleMessage(message);

    verifyAll();

    resetAll();
    replayAll();

    callbackHandler.cancelTimeStamp = 1;
    callbackHandler.handleMessage(message);

    verifyAll();
  }

  @SmallTest
  public void testDeleteSession() {
    Capture<Message> messageCapture = new Capture<Message>();
    Handler handler = createMock(Handler.class);
    expect(handler.sendMessageAtTime(capture(messageCapture), anyLong())).andReturn(true);
    replayAll();

    SessionExecutor executor = new SessionExecutor();
    executor.handler = handler;

    executor.deleteSession();

    verifyAll();
    assertEquals(ExecutorMainCallback.DELETE_SESSION, messageCapture.getValue().what);
  }

  @SmallTest
  public void testEvaluateAsynchronously() {
    Capture<Message> messageCapture = new Capture<Message>();
    Handler handler = createMock(Handler.class);
    expect(handler.sendMessageAtTime(capture(messageCapture), anyLong())).andReturn(true);
    replayAll();

    SessionExecutor executor = new SessionExecutor();
    executor.handler = handler;

    Input.Builder inputBuilder = Input.newBuilder();
    KeyEventInterface keyEvent = KeycodeConverter.getKeyEventInterface(KeyEvent.KEYCODE_A);

    executor.evaluateAsynchronously(inputBuilder, keyEvent, null);

    verifyAll();
    Message message = messageCapture.getValue();
    assertEquals(ExecutorMainCallback.EVALUATE_KEYEVENT_ASYNCHRONOUSLY, message.what);
    AsynchronousEvaluationContext context = AsynchronousEvaluationContext.class.cast(message.obj);
    assertSame(inputBuilder, context.inputBuilder);
    assertSame(keyEvent, context.triggeringKeyEvent);
    assertNull(context.callback);
    assertNull(context.callbackHandler);
  }

  @SmallTest
  public void testEvaluateAsynchronously_withoutKeyEvent() {
    Capture<Message> messageCapture = new Capture<Message>();
    Handler handler = createMock(Handler.class);
    expect(handler.sendMessageAtTime(capture(messageCapture), anyLong())).andReturn(true);
    replayAll();

    SessionExecutor executor = new SessionExecutor();
    executor.handler = handler;

    Input.Builder inputBuilder = Input.newBuilder();

    executor.evaluateAsynchronously(inputBuilder, null, null);

    verifyAll();
    Message message = messageCapture.getValue();
    assertEquals(ExecutorMainCallback.EVALUATE_ASYNCHRONOUSLY, message.what);
    AsynchronousEvaluationContext context = AsynchronousEvaluationContext.class.cast(message.obj);
    assertSame(inputBuilder, context.inputBuilder);
    assertNull(context.triggeringKeyEvent);
    assertNull(context.callback);
    assertNull(context.callbackHandler);
  }

  @SmallTest
  public void testEvaluateSynchronously() {
    Capture<Message> messageCapture = new Capture<Message>();
    Handler handler = createMock(Handler.class);

    // This emulates "accept message sent via handler, pseudo-execute of
    // SessionHandlerCallback#evaluateSynchronously on this thread (instead of the worker thread)
    // for synchronization purpose, and then return true as sendMessage's return value".
    IAnswer<Boolean> mockExecutor = new IAnswer<Boolean>() {
      @Override
      public Boolean answer() throws Throwable {
        // Emulate synchronous evaluation.
        Message message = Message.class.cast(getCurrentArguments()[0]);
        SynchronousEvaluationContext context =
            SynchronousEvaluationContext.class.cast(message.obj);
        context.outCommand = Command.newBuilder()
            .setInput(Input.newBuilder().setType(CommandType.NO_OPERATION).build())
            .setOutput(Output.getDefaultInstance())
            .build();
        context.evaluationSynchronizer.countDown();
        return Boolean.TRUE;
      }
    };
    expect(handler.sendMessageAtTime(capture(messageCapture), anyLong())).andAnswer(mockExecutor);
    replayAll();

    SessionExecutor executor = new SessionExecutor();
    executor.handler = handler;

    executor.evaluateSynchronously(Input.getDefaultInstance());

    verifyAll();
    assertEquals(ExecutorMainCallback.EVALUATE_SYNCHRONOUSLY, messageCapture.getValue().what);
  }

  @SmallTest
  public void testSendKey() {
    SessionExecutor executor = createSessionExecutorMock();
    ProtoCommands.KeyEvent mozcKeyEvent = ProtoCommands.KeyEvent.getDefaultInstance();
    KeyEventInterface triggeringKeyEvent =
        KeycodeConverter.getKeyEventInterface(KeyEvent.KEYCODE_A);
    EvaluationCallback evaluationCallback = createNiceMock(EvaluationCallback.class);
    List<TouchEvent> touchEventList = Collections.singletonList(TouchEvent.getDefaultInstance());
    executor.evaluateAsynchronously(
        matchesBuilder(Input.newBuilder()
            .setType(CommandType.SEND_KEY)
            .setKey(mozcKeyEvent)
            .addAllTouchEvents(touchEventList)),
        same(triggeringKeyEvent),
        same(evaluationCallback));
    replayAll();

    executor.sendKey(mozcKeyEvent, triggeringKeyEvent, touchEventList, evaluationCallback);

    verifyAll();
  }

  @SmallTest
  public void testSubmit() {
    SessionExecutor executor = createSessionExecutorMock();
    EvaluationCallback evaluationCallback = createNiceMock(EvaluationCallback.class);
    executor.evaluateAsynchronously(
        matchesBuilder(Input.newBuilder()
            .setType(CommandType.SEND_COMMAND)
            .setCommand(SessionCommand.newBuilder()
                .setType(SessionCommand.CommandType.SUBMIT))),
        isNull(KeyEventInterface.class),
        same(evaluationCallback));
    replayAll();

    executor.submit(evaluationCallback);

    verifyAll();
  }

  @SmallTest
  public void testSwitchInputMode() {
    SessionExecutor executor = createSessionExecutorMock();
    executor.evaluateAsynchronously(
        matchesBuilder(Input.newBuilder()
            .setType(CommandType.SEND_COMMAND)
            .setCommand(SessionCommand.newBuilder()
                .setType(SessionCommand.CommandType.SWITCH_INPUT_MODE)
                .setCompositionMode(CompositionMode.DIRECT))),
        isNull(KeyEventInterface.class),
        isNull(EvaluationCallback.class));
    replayAll();

    executor.switchInputMode(CompositionMode.DIRECT);

    verifyAll();
  }

  @SmallTest
  public void testSubmitCandidate() {
    SessionExecutor executor = createSessionExecutorMock();
    EvaluationCallback evaluationCallback = createNiceMock(EvaluationCallback.class);
    executor.evaluateAsynchronously(
        matchesBuilder(Input.newBuilder()
            .setType(CommandType.SEND_COMMAND)
            .setCommand(SessionCommand.newBuilder()
                .setType(SessionCommand.CommandType.SUBMIT_CANDIDATE)
                .setId(3))),
        isNull(KeyEventInterface.class),
        same(evaluationCallback));
    replayAll();

    executor.submitCandidate(3, evaluationCallback);

    verifyAll();
  }

  @SmallTest
  public void testResetContext() {
    SessionExecutor executor = createSessionExecutorMock();
    executor.evaluateAsynchronously(
        matchesBuilder(Input.newBuilder()
            .setType(CommandType.SEND_COMMAND)
            .setCommand(SessionCommand.newBuilder()
                .setType(SessionCommand.CommandType.RESET_CONTEXT))),
        isNull(KeyEventInterface.class),
        isNull(EvaluationCallback.class));
    replayAll();

    executor.resetContext();

    verifyAll();
  }

  @SmallTest
  public void testMoveCursor() {
    SessionExecutor executor = createSessionExecutorMock();
    EvaluationCallback evaluationCallback = createNiceMock(EvaluationCallback.class);
    executor.evaluateAsynchronously(
        matchesBuilder(Input.newBuilder()
            .setType(CommandType.SEND_COMMAND)
            .setCommand(
                SessionCommand.newBuilder()
                .setType(ProtoCommands.SessionCommand.CommandType.MOVE_CURSOR)
                .setCursorPosition(5))),
        isNull(KeyEventInterface.class),
        same(evaluationCallback));
    replayAll();

    executor.moveCursor(5, evaluationCallback);

    verifyAll();
  }

  @SmallTest
  public void testSwitchInputFieldType() {
    SessionExecutor executor = createSessionExecutorMock();
    executor.evaluateAsynchronously(
        matchesBuilder(Input.newBuilder()
            .setType(CommandType.SEND_COMMAND)
            .setCommand(SessionCommand.newBuilder()
                .setType(SessionCommand.CommandType.SWITCH_INPUT_FIELD_TYPE))
            .setContext(ProtoCommands.Context.newBuilder()
                .setInputFieldType(InputFieldType.NORMAL))),
        isNull(KeyEventInterface.class),
        isNull(EvaluationCallback.class));
    replayAll();

    executor.switchInputFieldType(InputFieldType.NORMAL);

    verifyAll();
  }

  @SmallTest
  public void testUndoOrRewind() {
    SessionExecutor executor = createSessionExecutorMock();
    EvaluationCallback evaluationCallback = createNiceMock(EvaluationCallback.class);
    List<TouchEvent> touchEventList = Collections.singletonList(TouchEvent.getDefaultInstance());
    executor.evaluateAsynchronously(
        matchesBuilder(Input.newBuilder()
            .setType(CommandType.SEND_COMMAND)
            .setCommand(SessionCommand.newBuilder()
                .setType(SessionCommand.CommandType.UNDO_OR_REWIND))
            .addAllTouchEvents(touchEventList)),
        isNull(KeyEventInterface.class),
        same(evaluationCallback));
    replayAll();

    executor.undoOrRewind(touchEventList, evaluationCallback);

    verifyAll();
  }

  @SmallTest
  public void testInsertToStorage() {
    SessionExecutor executor = createSessionExecutorMock();
    executor.evaluateAsynchronously(
        matchesBuilder(Input.newBuilder()
            .setType(CommandType.INSERT_TO_STORAGE)
            .setStorageEntry(GenericStorageEntry.newBuilder()
                .setType(StorageType.EMOTICON_HISTORY)
                .setKey("TEST_KEY")
                .addAllValue(Collections.singletonList(ByteString.copyFromUtf8("TEST_VALUE"))))),
        isNull(KeyEventInterface.class),
        isNull(EvaluationCallback.class));
    replayAll();

    executor.insertToStorage(
        StorageType.EMOTICON_HISTORY, "TEST_KEY",
        Collections.singletonList(ByteString.copyFromUtf8("TEST_VALUE")));

    verifyAll();
  }

  @SmallTest
  public void testExpandSuggestion() {
    SessionExecutor executor = createSessionExecutorMock();
    EvaluationCallback evaluationCallback = createNiceMock(EvaluationCallback.class);
    executor.evaluateAsynchronously(
        matchesBuilder(Input.newBuilder()
            .setType(CommandType.SEND_COMMAND)
            .setCommand(
                SessionCommand.newBuilder().setType(SessionCommand.CommandType.EXPAND_SUGGESTION))),
        isNull(KeyEventInterface.class),
        same(evaluationCallback));
    replayAll();

    executor.expandSuggestion(evaluationCallback);

    verifyAll();
  }

  @SmallTest
  public void testUsageStatsEvent() {
    SessionExecutor executor = createSessionExecutorMock();

    List<TouchEvent> touchEventList = Collections.singletonList(TouchEvent.getDefaultInstance());
    EvaluationCallback evaluationCallback = createNiceMock(EvaluationCallback.class);
    executor.evaluateAsynchronously(
        matchesBuilder(Input.newBuilder()
            .setType(CommandType.SEND_COMMAND)
            .setCommand(
                SessionCommand.newBuilder().setType(SessionCommand.CommandType.USAGE_STATS_EVENT))
            .addAllTouchEvents(touchEventList)),
        isNull(KeyEventInterface.class),
        isNull(EvaluationCallback.class));
    replayAll();

    executor.usageStatsEvent(touchEventList);

    verifyAll();
  }

  @SmallTest
  public void testSyncData() {
    SessionExecutor executor = createSessionExecutorMock();
    executor.evaluateAsynchronously(
        matchesBuilder(Input.newBuilder()
            .setType(CommandType.SYNC_DATA)),
        isNull(KeyEventInterface.class),
        isNull(EvaluationCallback.class));
    replayAll();

    executor.syncData();

    verifyAll();
  }

  @SmallTest
  public void testGetConfig() {
    SessionExecutor executor = createSessionExecutorMock();

    Config outputConfig = Config.newBuilder().build();
    expect(executor.evaluateSynchronously(
        Input.newBuilder().setType(Input.CommandType.GET_CONFIG).build()))
        .andReturn(Output.newBuilder().setConfig(outputConfig).build());
    replayAll();

    assertSame(outputConfig, executor.getConfig());

    verifyAll();
  }

  @SmallTest
  public void testSetConfig() {
    SessionExecutor executor = createSessionExecutorMock();

    Config inputConfig = Config.newBuilder().build();
    executor.evaluateAsynchronously(
        matchesBuilder(Input.newBuilder()
            .setType(CommandType.SET_CONFIG)
            .setConfig(inputConfig)),
        isNull(KeyEventInterface.class),
        isNull(EvaluationCallback.class));
    replayAll();

    executor.setConfig(inputConfig);

    verifyAll();
  }

  @SmallTest
  public void testSetImposedConfig() {
    SessionExecutor executor = createSessionExecutorMock();

    Config inputConfig = Config.newBuilder().build();
    executor.evaluateAsynchronously(
        matchesBuilder(Input.newBuilder()
            .setType(CommandType.SET_IMPOSED_CONFIG)
            .setConfig(inputConfig)),
        isNull(KeyEventInterface.class),
        isNull(EvaluationCallback.class));
    replayAll();

    executor.setImposedConfig(inputConfig);

    verifyAll();
  }

  @SmallTest
  public void testClearUnusedUserPrediction() {
    SessionExecutor executor = createSessionExecutorMock();
    expect(executor.evaluateSynchronously(
        Input.newBuilder()
            .setType(Input.CommandType.CLEAR_UNUSED_USER_PREDICTION)
            .build()))
        .andReturn(Output.getDefaultInstance());
    replayAll();

    executor.clearUnusedUserPrediction();

    verifyAll();
  }

  @SmallTest
  public void testClearUserHistory() {
    SessionExecutor executor = createSessionExecutorMock();
    expect(executor.evaluateSynchronously(
        Input.newBuilder()
            .setType(Input.CommandType.CLEAR_USER_HISTORY)
            .build()))
        .andReturn(Output.getDefaultInstance());
    replayAll();

    executor.clearUserHistory();

    verifyAll();
  }

  @SmallTest
  public void testClearUserPrediction() {
    SessionExecutor executor = createSessionExecutorMock();
    expect(executor.evaluateSynchronously(
        Input.newBuilder()
            .setType(Input.CommandType.CLEAR_USER_PREDICTION)
            .build()))
        .andReturn(Output.getDefaultInstance());
    replayAll();

    executor.clearUserPrediction();

    verifyAll();
  }

  @SmallTest
  public void testClearStorage() {
    SessionExecutor executor = createSessionExecutorMock();
    expect(executor.evaluateSynchronously(
        Input.newBuilder()
            .setType(Input.CommandType.CLEAR_STORAGE)
            .setStorageEntry(GenericStorageEntry.newBuilder().setType(StorageType.EMOJI_HISTORY))
            .build()))
        .andReturn(Output.getDefaultInstance());
    replayAll();

    executor.clearStorage(StorageType.EMOJI_HISTORY);

    verifyAll();
  }

  @SmallTest
  public void testReadAllFromStorage() {
    SessionExecutor executor = createSessionExecutorMock();
    expect(executor.evaluateSynchronously(
        Input.newBuilder()
            .setType(CommandType.READ_ALL_FROM_STORAGE)
            .setStorageEntry(GenericStorageEntry.newBuilder()
                .setType(StorageType.EMOTICON_HISTORY))
            .build()))
        .andReturn(Output.newBuilder()
            .setStorageEntry(GenericStorageEntry.newBuilder()
                .addAllValue(Collections.<ByteString>emptyList()))
            .build());
    replayAll();

    assertTrue(executor.readAllFromStorage(StorageType.EMOTICON_HISTORY).isEmpty());

    verifyAll();
  }

  @SmallTest
  public void testReload() {
    SessionExecutor executor = createSessionExecutorMock();

    executor.evaluateAsynchronously(
        matchesBuilder(Input.newBuilder()
            .setType(CommandType.RELOAD)),
        isNull(KeyEventInterface.class),
        isNull(EvaluationCallback.class));
    replayAll();

    executor.reload();

    verifyAll();
  }

  @SmallTest
  public void testSendUserDictionaryCommand() {
    SessionExecutor executor = createSessionExecutorMock();

    UserDictionaryCommand command = UserDictionaryCommand.newBuilder()
        .setType(UserDictionaryCommand.CommandType.NO_OPERATION)
        .build();

    expect(executor.evaluateSynchronously(
        Input.newBuilder()
            .setType(CommandType.SEND_USER_DICTIONARY_COMMAND)
            .setUserDictionaryCommand(command)
            .build()))
        .andReturn(Output.getDefaultInstance());
    replayAll();

    assertEquals(UserDictionaryCommandStatus.getDefaultInstance(),
                 executor.sendUserDictionaryCommand(command));

    verifyAll();
  }

  @LargeTest
  @StressTest
  public void testStressConversion() throws InterruptedException {
    MemoryLogger memoryLogger = new MemoryLogger("testStressConversion", 100);

    SessionExecutor session = SessionExecutor.getInstance(null);
    Context context = getInstrumentation().getTargetContext();
    session.reset(new SessionHandlerFactory(context), context);

    int actions = 1200;
    char minHiragana = 0x3041;
    char maxHiragana = 0x3096;
    int compositionLength = 20;
    Random random = new Random(0);  // Make the result deterministic.
    StringBuilder composition = new StringBuilder();
    memoryLogger.logMemory("start");
    for (int i = 0; i < actions; ++i) {
      composition.setLength(0);
      for (int j = 0; j < compositionLength; ++j) {
        composition.append((char) (random.nextInt(maxHiragana - minHiragana + 1) + minHiragana));
      }
      session.sendKey(ProtoCommands.KeyEvent.newBuilder()
                          .setKeyString(composition.toString()).build(),
                      null, Collections.<TouchEvent>emptyList(), null);
      session.sendKey(ProtoCommands.KeyEvent.newBuilder()
                          .setSpecialKey(SpecialKey.SPACE).build(),
                      null, Collections.<TouchEvent>emptyList(), null);
      final CountDownLatch latch = new CountDownLatch(1);
      session.submit(new EvaluationCallback() {
        @Override
        public void onCompleted(Command command, KeyEventInterface triggeringKeyEvent) {
          latch.countDown();
        }
      });
      latch.await();
      session.deleteSession();
      memoryLogger.logMemoryInterval();
    }
    memoryLogger.logMemory("end");

    Thread.sleep(30 * 1000);
    memoryLogger.logMemory("waited");
  }

  @LargeTest
  @StressTest
  public void testStressConversionKao() throws InterruptedException {
    MemoryLogger memoryLogger = new MemoryLogger("testStressConversionKao", 10);

    SessionExecutor session = SessionExecutor.getInstance(null);
    Context context = getInstrumentation().getTargetContext();
    session.reset(new SessionHandlerFactory(context), context);

    String composition = Strings.repeat("かお、", 100);
    memoryLogger.logMemory("start");
    session.sendKey(ProtoCommands.KeyEvent.newBuilder()
                        .setKeyString(composition.toString()).build(),
                    null, Collections.<TouchEvent>emptyList(), null);
    session.sendKey(ProtoCommands.KeyEvent.newBuilder()
                        .setSpecialKey(SpecialKey.SPACE).build(),
                    null, Collections.<TouchEvent>emptyList(), null);
    final CountDownLatch latch = new CountDownLatch(1);
    session.submit(new EvaluationCallback() {
      @Override
      public void onCompleted(Command command, KeyEventInterface triggeringKeyEvent) {
        latch.countDown();
      }
    });
    latch.await();
    memoryLogger.logMemory("beforedelete");
    session.deleteSession();
    memoryLogger.logMemory("end");

    Thread.sleep(60 * 1000);
    memoryLogger.logMemory("waited");
  }
}
