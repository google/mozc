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

import org.mozc.android.inputmethod.japanese.KeycodeConverter.KeyEventInterface;
import org.mozc.android.inputmethod.japanese.MozcLog;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Capability;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Capability.TextDeletionCapabilityType;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Command;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.CompositionMode;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Context.InputFieldType;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.GenericStorageEntry;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.GenericStorageEntry.StorageType;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.CommandType;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchEvent;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.InputOrBuilder;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.KeyEvent.SpecialKey;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Output;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Request;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.SessionCommand;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoConfig.Config;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoUserDictionaryStorage.UserDictionaryCommand;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoUserDictionaryStorage.UserDictionaryCommandStatus;
import com.google.protobuf.ByteString;

import android.content.Context;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;

import java.util.Collections;
import java.util.EnumSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.CountDownLatch;

/**
 * This class handles asynchronous and synchronous execution of command evaluation based on
 * {@link SessionHandler}.
 *
 * All execution is done on the single worker thread (not the UI thread). For asynchronous
 * execution, an evaluation task is sent to the worker thread, and the method returns immediately.
 * For synchronous execution, a task is sent to the worker thread, too, but it waits the
 * execution of the task (and pending tasks which have been sent before the task),
 * and then returns.
 *
 */
public class SessionExecutor {
  // At the moment we call mozc server via JNI interface directly,
  // while other platforms, e.g. Win/Mac/Linux etc, use IPC.
  // In order to keep the call order correctly, we call it from the single worker thread.
  // Note that we use well-known double check lazy initialization,
  // so that we can inject instances via reflections for testing purposes.
  private static volatile SessionExecutor instance;
  private static SessionExecutor getInstanceInternal(
      SessionHandlerFactory factory, Context applicationContext) {
    SessionExecutor result = instance;
    if (result == null) {
      synchronized (SessionExecutor.class) {
        result = instance;
        if (result == null) {
          result = instance = new SessionExecutor();
          if (factory != null) {
            result.reset(factory, applicationContext);
          }
        }
      }
    }
    return result;
  }

  /**
   * Replaces the singleton instance to {@code executor} so {@code getInstance} family
   * returns the new instance.
   * @param executor the new instance to replace the old one.
   * @return the old instance.
   */
  public static SessionExecutor setInstanceForTest(SessionExecutor executor) {
    synchronized (SessionExecutor.class) {
      SessionExecutor old = instance;
      instance = executor;
      return old;
    }
  }

  /**
   * Returns an instance of {@link SessionExecutor}.
   * This method may return an instance without initialization, assuming it will be initialized
   * by client in appropriate timing.
   */
  public static SessionExecutor getInstance(Context applicationContext) {
    return getInstanceInternal(null, applicationContext);
  }

  /**
   * Returns an instance of {@link SessionExecutor}. At first invocation, the instance will be
   * initialized by using given {@code factory}. Otherwise, the {@code factory} is simply ignored.
   */
  public static SessionExecutor getInstanceInitializedIfNecessary(
      SessionHandlerFactory factory, Context applicationContext) {
    return getInstanceInternal(factory, applicationContext);
  }

  private static volatile HandlerThread sessionHandlerThread;
  private static HandlerThread getHandlerThread() {
    HandlerThread result = sessionHandlerThread;
    if (result == null) {
      synchronized (SessionExecutor.class) {
        result = sessionHandlerThread;
        if (result == null) {
          result = new HandlerThread("Session worker thread");
          result.setDaemon(true);
          result.start();
          sessionHandlerThread = result;
        }
      }
    }
    return result;
  }

  /**
   * An interface to accept the result of asynchronous evaluation.
   */
  public interface EvaluationCallback {
    void onCompleted(Command command, KeyEventInterface triggeringKeyEvent);
  }

  // Context class during each evaluation. Package private for testing purpose.
  static class AsynchronousEvaluationContext {
    // For asynchronous evaluation, we set the sessionId in the callback running on the worker
    // thread. So, this class has Input.Builer as an argument for an evaluation, while
    // SynchronousEvaluationContext has Input because it is not necessary to be set sessionId.
    final long timeStamp;
    final Input.Builder inputBuilder;
    volatile Command outCommand;
    final KeyEventInterface triggeringKeyEvent;
    final EvaluationCallback callback;
    final Handler callbackHandler;

    AsynchronousEvaluationContext(long timeStamp,
                                  Input.Builder inputBuilder,
                                  KeyEventInterface triggeringKeyEvent,
                                  EvaluationCallback callback,
                                  Handler callbackHandler) {
      this.timeStamp = timeStamp;
      this.inputBuilder = inputBuilder;
      this.triggeringKeyEvent = triggeringKeyEvent;
      this.callback = callback;
      this.callbackHandler = callbackHandler;
    }
  }

  static class SynchronousEvaluationContext {
    final Input input;
    volatile Command outCommand;
    final CountDownLatch evaluationSynchronizer;

    SynchronousEvaluationContext(Input input, CountDownLatch evaluationSynchronizer) {
      this.input = input;
      this.evaluationSynchronizer = evaluationSynchronizer;
    }
  }

  // Context class just lines handler queue.
  static class KeyEventCallBackContext {
    final KeyEventInterface triggeringKeyEvent;
    final EvaluationCallback callback;
    final Handler callbackHandler;

    KeyEventCallBackContext(KeyEventInterface triggeringKeyEvent,
                            EvaluationCallback callback,
                            Handler callbackHandler) {
      this.triggeringKeyEvent = triggeringKeyEvent;
      this.callback = callback;
      this.callbackHandler = callbackHandler;
    }
  }


  /**
   * A core implementation of evaluation executing process.
   *
   * <p>This class takes messages from the UI thread. By using {@link SessionHandler},
   * it evaluates the {@link Input} in a message, and then returns the result with notifying
   * the UI thread if necessary.
   * All evaluations should be done with this class in order to keep evaluation in the incoming
   * order.
   * Package private for testing purpose.
   */
  static class ExecutorMainCallback implements Handler.Callback {

    /**
     * Initializes the currently connected sesion handler.
     * We need to initialize the current session handler in the executor thread due to
     * performance reason. This message should be run before any other messages.
     */
    static final int INITIALIZE_SESSION_HANDLER = 0;

    /**
     * Creates a new session.
     */
    static final int CREATE_SESSION = 1;

    /**
     * Evaluates the command asynchronously.
     */
    static final int EVALUATE_ASYNCHRONOUSLY = 2;

    /**
     * Evaluates the command asynchronously as similar to {@code EVALUATE_ASYNCHRONOUSLY}.
     * The difference from it is this should be used for the actual "key" event, such as
     * "hit 'A' key," "hit 'BACKSPACE'", "hit 'SPACEKEY'" or something.
     *
     * Note: this is used to figure out whether it is ok to skip some heavier instruction
     * in the server.
     */
    static final int EVALUATE_KEYEVENT_ASYNCHRONOUSLY = 3;

    /**
     * Evaluates the command synchronously.
     */
    static final int EVALUATE_SYNCHRONOUSLY = 4;

    /**
     * Updates the current request, and notify it to the server.
     */
    static final int UPDATE_REQUEST = 5;

    /**
     * Just pass a message to callback.
     */
    static final int PASS_TO_CALLBACK = 6;

    private static final long INVALID_SESSION_ID = 0;

    /**
     * A set of CommandType which don't need session id.
     */
    private static final Set<CommandType> SESSION_INDEPENDENT_COMMAND_TYPE_SET =
        Collections.unmodifiableSet(EnumSet.of(
            CommandType.SET_CONFIG,
            CommandType.GET_CONFIG,
            CommandType.SET_IMPOSED_CONFIG,
            CommandType.CLEAR_USER_HISTORY,
            CommandType.CLEAR_USER_PREDICTION,
            CommandType.CLEAR_UNUSED_USER_PREDICTION,
            CommandType.CLEAR_STORAGE,
            CommandType.READ_ALL_FROM_STORAGE,
            CommandType.RELOAD,
            CommandType.SEND_USER_DICTIONARY_COMMAND));

    private final SessionHandler sessionHandler;

    // Mozc session's ID.
    // Set on CREATE_SESSION and will not be updated.
    private long sessionId = INVALID_SESSION_ID;
    private Request.Builder request;

    // The logging for debugging is disabled by default.
    boolean isLogging = false;

    ExecutorMainCallback(SessionHandler sessionHandler) {
      if (sessionHandler == null) {
        throw new NullPointerException("sessionHandler is null.");
      }
      this.sessionHandler = sessionHandler;
    }

    @Override
    public boolean handleMessage(Message message) {
      // Dispatch the message.
      switch (message.what) {
        case INITIALIZE_SESSION_HANDLER:
          sessionHandler.initialize(Context.class.cast(message.obj));
          break;
        case CREATE_SESSION:
          MozcLog.d("start SessionExecutor#handleMessage CREATE_SESSION " + System.nanoTime());
          createSession();
          MozcLog.d("end SessionExecutor#handleMessage CREATE_SESSION " + System.nanoTime());
          break;
        case EVALUATE_ASYNCHRONOUSLY:
        case EVALUATE_KEYEVENT_ASYNCHRONOUSLY:
          evaluateAsynchronously(
              AsynchronousEvaluationContext.class.cast(message.obj), message.getTarget());
          break;
        case EVALUATE_SYNCHRONOUSLY:
          evaluateSynchronously(SynchronousEvaluationContext.class.cast(message.obj));
          break;
        case UPDATE_REQUEST: {
          updateRequest(Input.Builder.class.cast(message.obj));
          break;
        }
        case PASS_TO_CALLBACK:
          passToCallBack(KeyEventCallBackContext.class.cast(message.obj));
          break;
        default:
          // We don't process unknown messages.
          return false;
      }

      return true;
    }

    private Command evaluate(Input input) {
      Command inCommand = Command.newBuilder()
          .setInput(input)
          .setOutput(Output.getDefaultInstance())
          .build();
      // Note that toString() of ProtocolBuffers' message is very heavy.
      if (isLogging) {
        MozcCommandDebugger.inLog(inCommand);
      }
      Command outCommand = sessionHandler.evalCommand(inCommand);
      if (isLogging) {
        MozcCommandDebugger.outLog(outCommand);
      }
      return outCommand;
    }

    void createSession() {
      // Send CREATE_SESSION command and keep the returned sessionId.
      Input input = Input.newBuilder()
          .setType(CommandType.CREATE_SESSION)
          .setCapability(Capability.newBuilder()
              .setTextDeletion(TextDeletionCapabilityType.DELETE_PRECEDING_TEXT))
          .build();
      sessionId = evaluate(input).getOutput().getId();

      // Just after session creation, we send the default "request" to the server,
      // with ignoring its result.
      // Set mobile dedicated fields, which will not be changed.
      // Other fields may be set when the input view is changed.
      request = Request.newBuilder()
          .setMixedConversion(true)
          .setZeroQuerySuggestion(true)
          .setUpdateInputModeFromSurroundingText(false)
          .setAutoPartialSuggestion(true);
      evaluate(Input.newBuilder()
          .setId(sessionId)
          .setType(CommandType.SET_REQUEST)
          .setRequest(request)
          .build());
    }

    /**
     * Returns {@code true} iff the given {@code output} is squashable by following output.
     *
     * <p>Here, a squashable output A may be dropped if the next output B is sent before the
     * UI thread processes A (for performance reason).
     *
     * <p>{@link Output} consists from both fields which can be overwritten by following outputs
     * (e.g. composing text, a candidate list), and/or ones which cannot be (e.g. conversion
     * result). Squashing will happen when an output consists from only overwritable fields and
     * then another output, which will overwrite the UI change caused by the older output, comes.
     */
    static boolean isSquashableOutput(Output output) {
      // - If the output contains a result, it is not squashable because it is necessary
      //   to commit the string to the client application.
      // - If it has deletion_range, it is not squashable either because it is necessary
      //   to notify the editing to the client application.
      // - If the key is not consumed, it is not squashable because it is necessary to pass through
      //   the event to the client application.
      // - If the event doesn't have candidates, it is not squashable because we need to ensure
      //   the keyboard is visible.
      // - Otherwise squashable. An example is toggling a character. It contains neither
      //   result string nor deletion_range, has candidates (by suggestion) and the keyevent
      //   is consumed.
      return (output.getResult().getValue().length() == 0)
          && !output.hasDeletionRange()
          && output.getConsumed()
          && (output.getAllCandidateWords().getCandidatesCount() > 0);
    }

    /**
     * @return {@code true} if the given {@code inputBuilder} needs to be set session id.
     *   Otherwise {@code false}.
     */
    static boolean isSessionIdRequired(InputOrBuilder input) {
      return !SESSION_INDEPENDENT_COMMAND_TYPE_SET.contains(input.getType());
    }

    void evaluateAsynchronously(AsynchronousEvaluationContext context,
                                Handler sessionExecutorHandler) {
      // Before the evaluation, we remove all pending squashable result callbacks for performance
      // reason of less powerful devices.
      Input.Builder inputBuilder = context.inputBuilder;
      Handler callbackHandler = context.callbackHandler;
      if (callbackHandler != null &&
          inputBuilder.getCommand().getType() != SessionCommand.CommandType.EXPAND_SUGGESTION) {
        // Do not squash by EXPAND_SUGGESTION request, because the result of EXPAND_SUGGESTION
        // won't affect the inputConnection in MozcService, as the result should update
        // only candidates conceptually.
        callbackHandler.removeMessages(CallbackHandler.SQUASHABLE_OUTPUT);
      }

      if (inputBuilder.hasKey() &&
          (!inputBuilder.getKey().hasSpecialKey() ||
           inputBuilder.getKey().getSpecialKey() == SpecialKey.BACKSPACE) &&
          sessionExecutorHandler.hasMessages(EVALUATE_KEYEVENT_ASYNCHRONOUSLY)) {
        // Do not request suggestion result, due to performance reason, when:
        // - the key is normal key or backspace, and
        // - there is (at least one) following event.
        inputBuilder.setRequestSuggestion(false);
      }

      // We set the session id to the input in asynchronous evaluation before the evaluation,
      // if necessary.
      if (isSessionIdRequired(inputBuilder)) {
        // Assume this evaluation is called after CREATE_SESSION defined above.
        // So, set sessionId to the input, and then evaluate it.
        if (sessionId == INVALID_SESSION_ID) {
          StringBuilder message =
              new StringBuilder("Session should be created before asynchronous evaluation.");
          // Don't include inputBuilder.toString()
          // because inputBuilder might contain key event, which might cause privacy issue.
          if (inputBuilder.getType() != null) {
            message.append(" Input.CommandType=")
                   .append(inputBuilder.getType().toString());
          }
          if (inputBuilder.getCommand() != null && inputBuilder.getCommand().getType() != null) {
            message.append(" SessionCommand.CommandType=")
                   .append(inputBuilder.getCommand().getType().toString());
          }
          throw new IllegalStateException(message.toString());
        }
        inputBuilder.setId(sessionId);
      }
      context.outCommand = evaluate(inputBuilder.build());

      // Invoke callback handler if necessary.
      if (callbackHandler != null) {
        // For performance reason of, especially, less powerful devices, we want to skip
        // rendering whose effect will be overwritten by following (pending) rendering.
        // We annotate if the output can be overwritten or not here, so that we can remove
        // only those messages in later evaluation.
        Output output = context.outCommand.getOutput();
        Message message = callbackHandler.obtainMessage(
            isSquashableOutput(output) ? CallbackHandler.SQUASHABLE_OUTPUT
                                       : CallbackHandler.UNSQUASHABLE_OUTPUT,
            context);
        callbackHandler.sendMessage(message);
      }
    }

    void evaluateSynchronously(SynchronousEvaluationContext context) {
      Input input = context.input;
      if (isSessionIdRequired(input)) {
        // We expect only non-session-id-related input for synchronous evaluation.
        throw new IllegalArgumentException(
            "session id is required in evaluateSynchronously, but we don't expect it: " + input);
      }

      context.outCommand = evaluate(input);

       // The client thread is waiting for the evaluation by evaluationSynchronizer,
       // so notify the thread via the lock.
       context.evaluationSynchronizer.countDown();
    }

    void updateRequest(Input.Builder inputBuilder) {
      request.mergeFrom(inputBuilder.getRequest());
      Input input = inputBuilder
          .setId(sessionId)
          .setType(CommandType.SET_REQUEST)
          .setRequest(request)
          .build();
      // Do not render the result because the result does not have preedit.
      evaluate(input);
    }

    void passToCallBack(KeyEventCallBackContext context) {
      Handler callbackHandler = context.callbackHandler;
      Message message = callbackHandler.obtainMessage(CallbackHandler.UNSQUASHABLE_OUTPUT, context);
      callbackHandler.sendMessage(message);
    }
  }

  /**
   * A handler to process callback for asynchronous evaluation on the UI thread.
   */
  static class CallbackHandler extends Handler {
    /**
     * The message with this {@code what} cannot be overwritten by following evaluation.
     */
    static final int UNSQUASHABLE_OUTPUT = 0;
    /**
     * The message with this {@code what} may be cancelled by following evaluation
     * because the result of the message would be overwritten soon.
     */
    static final int SQUASHABLE_OUTPUT = 1;

    long cancelTimeStamp = System.nanoTime();

    CallbackHandler(Looper looper) {
      super(looper);
    }

    @Override
    public void handleMessage(Message message) {
      if (message.obj.getClass() == KeyEventCallBackContext.class) {
        KeyEventCallBackContext keyEventContext = KeyEventCallBackContext.class.cast(message.obj);
        keyEventContext.callback.onCompleted(null, keyEventContext.triggeringKeyEvent);
        return;
      }
      AsynchronousEvaluationContext context =
          AsynchronousEvaluationContext.class.cast(message.obj);
      // Note that this method should be run on the UI thread, where removePendingEvaluations runs,
      // so we don't need to take a lock here.
      if (context.timeStamp - cancelTimeStamp > 0) {
        context.callback.onCompleted(context.outCommand, context.triggeringKeyEvent);
      }
    }
  }

  private Handler handler;
  private ExecutorMainCallback mainCallback;
  private final CallbackHandler callbackHandler;

  // Note that theoretically the constructor should be private in order to keep this singleton,
  // but we use protected for testing purpose.
  protected SessionExecutor() {
    handler = null;
    mainCallback = null;
    callbackHandler = new CallbackHandler(Looper.getMainLooper());
  }

  /**
   * Resets the instance by setting {@code SessionHandler} created by the given {@code factory}.
   */
  public void reset(SessionHandlerFactory factory, Context applicationContext) {
    HandlerThread thread = getHandlerThread();
    mainCallback = new ExecutorMainCallback(factory.create());
    handler = new Handler(thread.getLooper(), mainCallback);
    handler.sendMessage(handler.obtainMessage(ExecutorMainCallback.INITIALIZE_SESSION_HANDLER,
                                              applicationContext));
  }

  /**
   * @param isLogging Set {@code true} if logging of evaluations is needed.
   */
  public void setLogging(boolean isLogging) {
    if (mainCallback != null) {
      mainCallback.isLogging = isLogging;
    }
  }

  /**
   * Remove pending evaluations from the pending queue.
   */
  public void removePendingEvaluations() {
    callbackHandler.cancelTimeStamp = System.nanoTime();
    if (handler != null) {
      handler.removeMessages(ExecutorMainCallback.EVALUATE_ASYNCHRONOUSLY);
      handler.removeMessages(ExecutorMainCallback.EVALUATE_KEYEVENT_ASYNCHRONOUSLY);
      handler.removeMessages(ExecutorMainCallback.EVALUATE_SYNCHRONOUSLY);
      handler.removeMessages(ExecutorMainCallback.UPDATE_REQUEST);
    }
    callbackHandler.removeMessages(CallbackHandler.UNSQUASHABLE_OUTPUT);
    callbackHandler.removeMessages(CallbackHandler.SQUASHABLE_OUTPUT);
  }

  /**
   * Sends a create session message to the JNI working handler, and returns immediately.
   * The actual evaluation (session creation) will be done eventually, before the next
   * evaluation request is handled.
   */
  public void createSession() {
    handler.sendMessage(handler.obtainMessage(ExecutorMainCallback.CREATE_SESSION));
  }

  /**
   * Evaluates the input caused by triggeringKeyEvent on the JNI worker thread.
   * When the evaluation is done, callbackHandler will receive the message with the evaluation
   * context.
   *
   * This method returns immediately, i.e., even after this method's invocation,
   * it shouldn't be assumed that the evaluation is done.
   *
   * Note that this method is exposed as package private for testing purpose.
   *
   * @param inputBuilder the input data
   * @param triggeringKeyEvent a key event which triggers this evaluation
   * @param callback a callback handler if needed
   */
  void evaluateAsynchronously(Input.Builder inputBuilder, KeyEventInterface triggeringKeyEvent,
                                     EvaluationCallback callback) {
    AsynchronousEvaluationContext context = new AsynchronousEvaluationContext(
        System.nanoTime(), inputBuilder, triggeringKeyEvent,
        callback, callback == null ? null : callbackHandler);
    int type = (triggeringKeyEvent != null)
        ? ExecutorMainCallback.EVALUATE_KEYEVENT_ASYNCHRONOUSLY
        : ExecutorMainCallback.EVALUATE_ASYNCHRONOUSLY;
    handler.sendMessage(handler.obtainMessage(type, context));
  }

  /**
   * Sends {@code SEND_KEY} command to the server asynchronously.
   */
  public void sendKey(ProtoCommands.KeyEvent mozcKeyEvent, KeyEventInterface triggeringKeyEvent,
                      List<? extends TouchEvent> touchEventList, EvaluationCallback callback) {
    Input.Builder inputBuilder = Input.newBuilder()
        .setType(CommandType.SEND_KEY)
        .setKey(mozcKeyEvent)
        .addAllTouchEvents(touchEventList);
    evaluateAsynchronously(inputBuilder, triggeringKeyEvent, callback);
  }

  /**
   * Sends {@code SUBMIT} command to the server asynchronously.
   */
  public void submit(EvaluationCallback callback) {
    Input.Builder inputBuilder = Input.newBuilder()
        .setType(CommandType.SEND_COMMAND)
        .setCommand(SessionCommand.newBuilder()
            .setType(SessionCommand.CommandType.SUBMIT));
    evaluateAsynchronously(inputBuilder, null, callback);
  }

  /**
   * Sends {@code SWITCH_INPUT_MODE} command to the server asynchronously.
   */
  public void switchInputMode(CompositionMode mode) {
    Input.Builder inputBuilder = Input.newBuilder()
        .setType(CommandType.SEND_COMMAND)
        .setCommand(SessionCommand.newBuilder()
            .setType(SessionCommand.CommandType.SWITCH_INPUT_MODE)
            .setCompositionMode(mode));
    evaluateAsynchronously(inputBuilder, null, null);
  }

  /**
   * Sends {@code SUBMIT_CANDIDATE} command to the server asynchronously.
   */
  public void submitCandidate(int candidateId, EvaluationCallback callback) {
    Input.Builder inputBuilder = Input.newBuilder()
        .setType(CommandType.SEND_COMMAND)
        .setCommand(SessionCommand.newBuilder()
            .setType(SessionCommand.CommandType.SUBMIT_CANDIDATE)
            .setId(candidateId));
    evaluateAsynchronously(inputBuilder, null, callback);
  }

  /**
   * Sends {@code RESET_CONTEXT} command to the server asynchronously.
   */
  public void resetContext() {
    Input.Builder inputBuilder = Input.newBuilder()
        .setType(CommandType.SEND_COMMAND)
        .setCommand(SessionCommand.newBuilder()
            .setType(SessionCommand.CommandType.RESET_CONTEXT));
    evaluateAsynchronously(inputBuilder, null, null);
  }

  /**
   * Sends {@code MOVE_CURSOR} command to the server asynchronously.
   */
  public void moveCursor(int cursorPosition, EvaluationCallback callback) {
    Input.Builder inputBuilder = Input.newBuilder()
        .setType(CommandType.SEND_COMMAND)
        .setCommand(SessionCommand.newBuilder()
            .setType(ProtoCommands.SessionCommand.CommandType.MOVE_CURSOR)
            .setCursorPosition(cursorPosition));
    evaluateAsynchronously(inputBuilder, null, callback);
  }

  /**
   * Sends {@code SWITCH_INPUT_FIELD_TYPE} command to the server asynchronously.
   */
  public void switchInputFieldType(InputFieldType inputFieldType) {
    Input.Builder inputBuilder = Input.newBuilder()
        .setType(CommandType.SEND_COMMAND)
        .setCommand(SessionCommand.newBuilder()
            .setType(ProtoCommands.SessionCommand.CommandType.SWITCH_INPUT_FIELD_TYPE))
        .setContext(ProtoCommands.Context.newBuilder()
            .setInputFieldType(inputFieldType));
    evaluateAsynchronously(inputBuilder, null, null);
  }

  /**
   * Sends {@code UNDO_OR_REWIND} command to the server asynchronously.
   */
  public void undoOrRewind(List<? extends TouchEvent> touchEventList,
                           EvaluationCallback callback) {
    Input.Builder inputBuilder = Input.newBuilder()
        .setType(CommandType.SEND_COMMAND)
        .setCommand(SessionCommand.newBuilder()
            .setType(SessionCommand.CommandType.UNDO_OR_REWIND))
        .addAllTouchEvents(touchEventList);
    evaluateAsynchronously(inputBuilder, null, callback);
  }

  /**
   * Sends {@code INSERT_TO_STORAGE} with given {@code type}, {@code key} and {@code values}
   * to the server asynchronously.
   */
  public void insertToStorage(StorageType type, String key, List<ByteString> values) {
    Input.Builder inputBuilder = Input.newBuilder()
        .setType(CommandType.INSERT_TO_STORAGE)
        .setStorageEntry(GenericStorageEntry.newBuilder()
            .setType(type)
            .setKey(key)
            .addAllValue(values));
    evaluateAsynchronously(inputBuilder, null, null);
  }

  public void expandSuggestion(EvaluationCallback callback) {
    Input.Builder inputBuilder = Input.newBuilder()
        .setType(CommandType.SEND_COMMAND)
        .setCommand(
            SessionCommand.newBuilder().setType(SessionCommand.CommandType.EXPAND_SUGGESTION));
    evaluateAsynchronously(inputBuilder, null, callback);
  }

  public void usageStatsEvent(List<? extends TouchEvent> touchEventList) {
    if (touchEventList.isEmpty()) {
      return;
    }

    Input.Builder inputBuilder = Input.newBuilder()
        .setType(CommandType.SEND_COMMAND)
        .setCommand(
            SessionCommand.newBuilder().setType(SessionCommand.CommandType.USAGE_STATS_EVENT))
        .addAllTouchEvents(touchEventList);
    evaluateAsynchronously(inputBuilder, null, null);
  }

  public void syncData() {
    Input.Builder inputBuilder = Input.newBuilder()
        .setType(CommandType.SYNC_DATA);
    evaluateAsynchronously(inputBuilder, null, null);
  }

  /**
   * Evaluates the input on the JNI worker thread, and wait that the evaluation is done.
   * This method blocks (typically <30ms).
   *
   * Note that this method is exposed as package private for testing purpose.
   */
  Output evaluateSynchronously(Input input) {
    CountDownLatch evaluationSynchronizer = new CountDownLatch(1);
    SynchronousEvaluationContext context =
          new SynchronousEvaluationContext(input, evaluationSynchronizer);
    handler.sendMessage(handler.obtainMessage(
        ExecutorMainCallback.EVALUATE_SYNCHRONOUSLY, context));

    try {
      evaluationSynchronizer.await();
    } catch (InterruptedException exception) {
      MozcLog.w("Session thread is interrupted during evaluation.");
    }

    return context.outCommand.getOutput();
  }

  /**
   * Gets a config from the server.
   */
  public Config getConfig() {
    Input input = Input.newBuilder().setType(Input.CommandType.GET_CONFIG).build();
    return evaluateSynchronously(input).getConfig();
  }

  /**
   * Sets the given {@code config} to the server.
   */
  public void setConfig(Config config) {
    // Ignore output.
    evaluateAsynchronously(
        Input.newBuilder().setType(Input.CommandType.SET_CONFIG).setConfig(config), null, null);
  }

  /**
   * Sets the given {@code config} to the server as imposed config.
   */
  public void setImposedConfig(Config config) {
    // Ignore output.
    evaluateAsynchronously(
        Input.newBuilder().setType(Input.CommandType.SET_IMPOSED_CONFIG).setConfig(config),
        null, null);
  }

  /**
   * Clears the unused user prediction from the server synchronously.
   */
  public void clearUnusedUserPrediction() {
    evaluateSynchronously(
        Input.newBuilder().setType(CommandType.CLEAR_UNUSED_USER_PREDICTION).build());
  }

  /**
   * Clears the user's history from the server synchronously.
   */
  public void clearUserHistory() {
    evaluateSynchronously(
        Input.newBuilder().setType(CommandType.CLEAR_USER_HISTORY).build());
  }

  /**
   * Clears user's prediction from the server synchronously.
   */
  public void clearUserPrediction() {
    evaluateSynchronously(
        Input.newBuilder().setType(CommandType.CLEAR_USER_PREDICTION).build());
  }

  /**
   * Clears a generic storage, which is used typically by symbol history.
   * @param storageType the storage to be cleared
   */
  public void clearStorage(StorageType storageType) {
    evaluateSynchronously(
        Input.newBuilder()
            .setType(CommandType.CLEAR_STORAGE)
            .setStorageEntry(GenericStorageEntry.newBuilder().setType(storageType))
            .build());
  }

  /**
   * Reads stored values of the given {@code type} from the server, and returns it.
   */
  public List<ByteString> readAllFromStorage(StorageType type) {
    Input input = Input.newBuilder()
        .setType(CommandType.READ_ALL_FROM_STORAGE)
        .setStorageEntry(GenericStorageEntry.newBuilder()
            .setType(type))
        .build();
    Output output = evaluateSynchronously(input);
    return output.getStorageEntry().getValueList();
  }

  /**
   * Sends 'Reload' request to the server, typically for reloading user dictionaries.
   */
  public void reload() {
    // Ignore output.
    evaluateAsynchronously(
        Input.newBuilder().setType(Input.CommandType.RELOAD), null, null);
  }

  /**
   * Sends SEND_USER_DICTIONARY_COMMAND to edit user dictionaries.
   */
  public UserDictionaryCommandStatus sendUserDictionaryCommand(UserDictionaryCommand command) {
    Output output = evaluateSynchronously(Input.newBuilder()
        .setType(CommandType.SEND_USER_DICTIONARY_COMMAND)
        .setUserDictionaryCommand(command)
        .build());
    return output.getUserDictionaryCommandStatus();
  }

  /**
   * Sends an UPDATE_REQUEST command to the evaluation thread.
   */
  public void updateRequest(Request update, List<? extends TouchEvent> touchEventList) {
    Input.Builder inputBuilder = Input.newBuilder()
        .setRequest(update)
        .addAllTouchEvents(touchEventList);
    handler.sendMessage(handler.obtainMessage(ExecutorMainCallback.UPDATE_REQUEST, inputBuilder));
  }

  public void sendKeyEvent(KeyEventInterface triggeringKeyEvent, EvaluationCallback callback) {
    KeyEventCallBackContext context = new KeyEventCallBackContext(
        triggeringKeyEvent, callback, callbackHandler);
    handler.sendMessage(handler.obtainMessage(ExecutorMainCallback.PASS_TO_CALLBACK, context));
  }
}
