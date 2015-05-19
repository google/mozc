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

package org.mozc.android.inputmethod.japanese.userdictionary;

import static android.test.MoreAsserts.assertContentsInOrder;
import static android.test.MoreAsserts.assertEmpty;

import static org.easymock.EasyMock.expect;

import org.mozc.android.inputmethod.japanese.protobuf.ProtoUserDictionaryStorage.UserDictionary.Entry;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoUserDictionaryStorage.UserDictionary.PosType;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoUserDictionaryStorage.UserDictionaryCommand;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoUserDictionaryStorage.UserDictionaryStorage;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoUserDictionaryStorage.UserDictionaryCommand.CommandType;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoUserDictionaryStorage.UserDictionaryCommandStatus;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoUserDictionaryStorage.UserDictionaryCommandStatus.Status;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoUserDictionaryStorage.UserDictionary;
import org.mozc.android.inputmethod.japanese.session.SessionExecutor;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import org.mozc.android.inputmethod.japanese.testing.Parameter;
import org.mozc.android.inputmethod.japanese.testing.VisibilityProxy;

import android.net.Uri;
import android.test.suitebuilder.annotation.SmallTest;

import java.io.File;
import java.io.IOException;
import java.util.Arrays;
import java.util.List;
import java.util.zip.ZipFile;

/**
 */
public class UserDictionaryToolModelTest extends InstrumentationTestCaseWithMock {
  @SmallTest
  public void testCreateSession() {
    SessionExecutor executor = createMock(SessionExecutor.class);
    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.CREATE_SESSION)
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
            .setSessionId(100L)
            .build());
    replayAll();

    UserDictionaryToolModel model = new UserDictionaryToolModel(executor);
    model.createSession();

    verifyAll();
    assertEquals(100L, VisibilityProxy.getField(model, "sessionId"));
  }

  @SmallTest
  public void testDeleteSession() {
    SessionExecutor executor = createMock(SessionExecutor.class);
    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.DELETE_SESSION)
            .setSessionId(100L)
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
            .build());
    replayAll();

    UserDictionaryToolModel model = new UserDictionaryToolModel(executor);
    VisibilityProxy.setField(model, "sessionId", 100L);
    model.deleteSession();

    verifyAll();
    assertEquals(0L, VisibilityProxy.getField(model, "sessionId"));
  }

  @SmallTest
  public void testResumeSession_success() {
    SessionExecutor executor = createMock(SessionExecutor.class);
    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.NO_OPERATION)
            .setSessionId(100L)
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
            .build());
    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.SET_DEFAULT_DICTIONARY_NAME)
            .setSessionId(100L)
            .setDictionaryName("default dictionary name")
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
            .build());
    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.LOAD)
            .setSessionId(100L)
            .setEnsureNonEmptyStorage(true)
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
            .build());
    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.GET_USER_DICTIONARY_NAME_LIST)
            .setSessionId(100L)
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
            .setStorage(UserDictionaryStorage.newBuilder()
                .addDictionaries(UserDictionary.newBuilder()
                    .setName("User Dictionary 1")
                    .setId(1L))
                .addDictionaries(UserDictionary.newBuilder()
                    .setName("User Dictionary 2")
                    .setId(2L)))
            .build());
    replayAll();

    UserDictionaryToolModel model = new UserDictionaryToolModel(executor);
    VisibilityProxy.setField(model, "sessionId", 100L);

    assertEquals(Status.USER_DICTIONARY_COMMAND_SUCCESS,
                 model.resumeSession("default dictionary name"));
    verifyAll();

    assertContentsInOrder(model.getDictionaryNameList(), "User Dictionary 1", "User Dictionary 2");
    assertEquals(0, model.getSelectedDictionaryIndex());
  }

  @SmallTest
  public void testResumeSession_fileNotFound() {
    SessionExecutor executor = createMock(SessionExecutor.class);
    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.NO_OPERATION)
            .setSessionId(100L)
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
            .build());
    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.SET_DEFAULT_DICTIONARY_NAME)
            .setSessionId(100L)
            .setDictionaryName("default dictionary name")
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
            .build());
    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.LOAD)
            .setSessionId(100L)
            .setEnsureNonEmptyStorage(true)
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.FILE_NOT_FOUND)
            .build());
    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.GET_USER_DICTIONARY_NAME_LIST)
            .setSessionId(100L)
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
            .setStorage(UserDictionaryStorage.newBuilder()
                .addDictionaries(UserDictionary.newBuilder()
                    .setName("default dictionary name")
                    .setId(1L)))
            .build());
    replayAll();

    UserDictionaryToolModel model = new UserDictionaryToolModel(executor);
    VisibilityProxy.setField(model, "sessionId", 100L);

    // Even if the file is not found, the load should be succeeded.
    // This is because it is expected case, e.g., in the first time launching of the tool.
    assertEquals(Status.USER_DICTIONARY_COMMAND_SUCCESS,
                 model.resumeSession("default dictionary name"));
    verifyAll();

    assertContentsInOrder(model.getDictionaryNameList(), "default dictionary name");
    assertEquals(0, model.getSelectedDictionaryIndex());
  }

  @SmallTest
  public void testResumeSession_failLoad() {
    SessionExecutor executor = createMock(SessionExecutor.class);
    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.NO_OPERATION)
            .setSessionId(100L)
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
            .build());
    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.SET_DEFAULT_DICTIONARY_NAME)
            .setSessionId(100L)
            .setDictionaryName("default dictionary name")
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.UNKNOWN_ERROR)
            .build());

    // Regradless of whether the SET_DEFAULT_DICTIONARY_NAME is successfully done or not,
    // the load should be invoked.
    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.LOAD)
            .setSessionId(100L)
            .setEnsureNonEmptyStorage(true)
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.UNKNOWN_ERROR)
            .build());

    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.GET_USER_DICTIONARY_NAME_LIST)
            .setSessionId(100L)
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
            .setStorage(UserDictionaryStorage.newBuilder()
                .addDictionaries(UserDictionary.newBuilder()
                    .setName("default dictionary name")
                    .setId(1L)))
            .build());
    replayAll();

    UserDictionaryToolModel model = new UserDictionaryToolModel(executor);
    VisibilityProxy.setField(model, "sessionId", 100L);

    assertEquals(Status.UNKNOWN_ERROR, model.resumeSession("default dictionary name"));
  }

  @SmallTest
  public void testResumeSession_retry() {
    SessionExecutor executor = createMock(SessionExecutor.class);
    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.NO_OPERATION)
            .setSessionId(100L)
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.UNKNOWN_SESSION_ID)
            .build());

    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.CREATE_SESSION)
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
            .setSessionId(200L)
            .build());

    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.SET_DEFAULT_DICTIONARY_NAME)
            .setSessionId(200L)
            .setDictionaryName("default dictionary name")
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.UNKNOWN_ERROR)
            .build());

    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.LOAD)
            .setSessionId(200L)
            .setEnsureNonEmptyStorage(true)
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
            .build());

    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.GET_USER_DICTIONARY_NAME_LIST)
            .setSessionId(200L)
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
            .setStorage(UserDictionaryStorage.newBuilder()
                .addDictionaries(UserDictionary.newBuilder()
                    .setName("User Dictionary 1")
                    .setId(1L))
                .addDictionaries(UserDictionary.newBuilder()
                    .setName("User Dictionary 2")
                    .setId(2L)))
            .build());
    replayAll();

    UserDictionaryToolModel model = new UserDictionaryToolModel(executor);
    VisibilityProxy.setField(model, "sessionId", 100L);

    assertEquals(Status.USER_DICTIONARY_COMMAND_SUCCESS,
                 model.resumeSession("default dictionary name"));
    verifyAll();

    assertContentsInOrder(model.getDictionaryNameList(), "User Dictionary 1", "User Dictionary 2");
    assertEquals(0, model.getSelectedDictionaryIndex());
  }

  @SmallTest
  public void testResumeSession_failGetDictionaryNameList() {
    SessionExecutor executor = createMock(SessionExecutor.class);
    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.NO_OPERATION)
            .setSessionId(100L)
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
            .build());

    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.SET_DEFAULT_DICTIONARY_NAME)
            .setSessionId(100L)
            .setDictionaryName("default dictionary name")
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
            .build());
    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.LOAD)
            .setSessionId(100L)
            .setEnsureNonEmptyStorage(true)
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
            .build());
    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.GET_USER_DICTIONARY_NAME_LIST)
            .setSessionId(100L)
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.UNKNOWN_ERROR)
            .build());
    replayAll();

    UserDictionaryToolModel model = new UserDictionaryToolModel(executor);
    VisibilityProxy.setField(model, "sessionId", 100L);

    // The loading itself is succeeded.
    assertEquals(Status.USER_DICTIONARY_COMMAND_SUCCESS,
                 model.resumeSession("default dictionary name"));
    verifyAll();

    assertEmpty(model.getDictionaryNameList());
    assertEquals(0, model.getSelectedDictionaryIndex());
  }

  @SmallTest
  public void testPauseSession_clean() {
    SessionExecutor executor = createMock(SessionExecutor.class);
    // Do nothing for clean model.
    replayAll();

    UserDictionaryToolModel model = new UserDictionaryToolModel(executor);
    VisibilityProxy.setField(model, "sessionId", 100L);
    VisibilityProxy.setField(model, "dirty", false);  // Ensure it's clean.

    assertEquals(Status.USER_DICTIONARY_COMMAND_SUCCESS, model.pauseSession());
    verifyAll();
  }

  @SmallTest
  public void testPauseSession_dirtyAndSuccess() {
    SessionExecutor executor = createMock(SessionExecutor.class);
    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.SAVE)
            .setSessionId(100L)
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
            .build());
    executor.reload();
    replayAll();

    UserDictionaryToolModel model = new UserDictionaryToolModel(executor);
    VisibilityProxy.setField(model, "sessionId", 100L);
    VisibilityProxy.setField(model, "dirty", true);  // Ensure it's dirty.

    assertEquals(Status.USER_DICTIONARY_COMMAND_SUCCESS, model.pauseSession());
    verifyAll();
  }

  @SmallTest
  public void testPauseSession_dirtyAndFail() {
    SessionExecutor executor = createMock(SessionExecutor.class);
    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.SAVE)
            .setSessionId(100L)
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.UNKNOWN_ERROR)
            .build());
    replayAll();

    UserDictionaryToolModel model = new UserDictionaryToolModel(executor);
    VisibilityProxy.setField(model, "sessionId", 100L);
    VisibilityProxy.setField(model, "dirty", true);  // Ensure it's dirty.

    assertEquals(Status.UNKNOWN_ERROR, model.pauseSession());
    verifyAll();
  }

  @SmallTest
  public void testCheckUndoability() {
    SessionExecutor executor = createMock(SessionExecutor.class);
    UserDictionaryToolModel model = new UserDictionaryToolModel(executor);
    VisibilityProxy.setField(model, "sessionId", 100L);
    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.CHECK_UNDOABILITY)
            .setSessionId(100L)
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
            .build());
    replayAll();

    assertEquals(Status.USER_DICTIONARY_COMMAND_SUCCESS, model.checkUndoability());

    verifyAll();
  }

  @SmallTest
  public void testUndo() {
    SessionExecutor executor = createMock(SessionExecutor.class);
    UserDictionaryToolModel model = new UserDictionaryToolModel(executor);
    VisibilityProxy.setField(model, "sessionId", 100L);
    VisibilityProxy.setField(model, "selectedDictionaryId", 400L);
    VisibilityProxy.setField(
        model, "storage",
        UserDictionaryStorage.newBuilder()
            .addDictionaries(UserDictionary.newBuilder()
                .setId(400L))
            .addDictionaries(UserDictionary.newBuilder()
                .setId(500L))
            .build());

    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.UNDO)
            .setSessionId(100L)
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
            .build());

    // First case, returns the same dictionary name list.
    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.GET_USER_DICTIONARY_NAME_LIST)
            .setSessionId(100L)
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
            .setStorage(UserDictionaryStorage.newBuilder()
                .addDictionaries(UserDictionary.newBuilder()
                    .setId(400L))
                .addDictionaries(UserDictionary.newBuilder()
                    .setId(500L)))
            .build());
    replayAll();

    assertEquals(Status.USER_DICTIONARY_COMMAND_SUCCESS, model.undo());

    verifyAll();
    assertTrue(model.isDirty());
    assertEquals(0, model.getSelectedDictionaryIndex());

    VisibilityProxy.setField(model, "dirty", false);
    resetAll();
    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.UNDO)
            .setSessionId(100L)
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
            .build());

    // Second case, returns a (deleted) dictionary is prepended.
    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.GET_USER_DICTIONARY_NAME_LIST)
            .setSessionId(100L)
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
            .setStorage(UserDictionaryStorage.newBuilder()
                .addDictionaries(UserDictionary.newBuilder()
                    .setId(300L))
                .addDictionaries(UserDictionary.newBuilder()
                    .setId(400L))
                .addDictionaries(UserDictionary.newBuilder()
                    .setId(500L)))
            .build());
    replayAll();

    assertEquals(Status.USER_DICTIONARY_COMMAND_SUCCESS, model.undo());

    verifyAll();
    assertTrue(model.isDirty());
    assertEquals(1, model.getSelectedDictionaryIndex());

    VisibilityProxy.setField(model, "dirty", false);
    resetAll();
    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.UNDO)
            .setSessionId(100L)
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
            .build());

    // Third case, the current selected dictionary is deleted. Keep the selected dictionary index.
    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.GET_USER_DICTIONARY_NAME_LIST)
            .setSessionId(100L)
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
            .setStorage(UserDictionaryStorage.newBuilder()
                .addDictionaries(UserDictionary.newBuilder()
                    .setId(300L))
                .addDictionaries(UserDictionary.newBuilder()
                    .setId(500L)))
            .build());
    replayAll();

    assertEquals(Status.USER_DICTIONARY_COMMAND_SUCCESS, model.undo());

    verifyAll();
    assertTrue(model.isDirty());
    assertEquals(1, model.getSelectedDictionaryIndex());
  }

  @SmallTest
  public void testUndo_fail() {
    SessionExecutor executor = createMock(SessionExecutor.class);
    UserDictionaryToolModel model = new UserDictionaryToolModel(executor);
    VisibilityProxy.setField(model, "sessionId", 100L);
    VisibilityProxy.setField(model, "selectedDictionaryId", 400L);
    VisibilityProxy.setField(
        model, "storage",
        UserDictionaryStorage.newBuilder()
            .addDictionaries(UserDictionary.newBuilder()
                .setId(400L))
            .addDictionaries(UserDictionary.newBuilder()
                .setId(500L))
            .build());

    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.UNDO)
            .setSessionId(100L)
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.NO_UNDO_HISTORY)
            .build());

    replayAll();

    assertEquals(Status.NO_UNDO_HISTORY, model.undo());

    verifyAll();
    assertFalse(model.isDirty());
    assertEquals(0, model.getSelectedDictionaryIndex());
  }

  @SmallTest
  public void testCheckNewDictionaryAvailability() {
    SessionExecutor executor = createMock(SessionExecutor.class);
    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.CHECK_NEW_DICTIONARY_AVAILABILITY)
            .setSessionId(100L)
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
            .build());
    replayAll();

    UserDictionaryToolModel model = new UserDictionaryToolModel(executor);
    VisibilityProxy.setField(model, "sessionId", 100L);

    assertEquals(Status.USER_DICTIONARY_COMMAND_SUCCESS, model.checkNewDictionaryAvailability());
    verifyAll();
  }

  @SmallTest
  public void testCreateDictionary_success() {
    SessionExecutor executor = createMock(SessionExecutor.class);
    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.CREATE_DICTIONARY)
            .setSessionId(100L)
            .setDictionaryName("New User Dictionary")
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
            .setDictionaryId(500L)
            .build());

    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.GET_USER_DICTIONARY_NAME_LIST)
            .setSessionId(100L)
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
            .setStorage(UserDictionaryStorage.newBuilder()
                .addDictionaries(UserDictionary.newBuilder()
                    .setName("User Dictionary 1")
                    .setId(1L))
                .addDictionaries(UserDictionary.newBuilder()
                    .setName("User Dictionary 2")
                    .setId(2L))
                .addDictionaries(UserDictionary.newBuilder()
                    .setName("New User Dictionary")
                    .setId(500L)))
            .build());
    replayAll();

    UserDictionaryToolModel model = new UserDictionaryToolModel(executor);
    VisibilityProxy.setField(model, "sessionId", 100L);

    assertEquals(Status.USER_DICTIONARY_COMMAND_SUCCESS,
                 model.createDictionary("New User Dictionary"));
    verifyAll();

    assertEquals(2, model.getSelectedDictionaryIndex());
    assertEquals("New User Dictionary", model.getSelectedDictionaryName());
    assertContentsInOrder(model.getDictionaryNameList(),
                          "User Dictionary 1",
                          "User Dictionary 2",
                          "New User Dictionary");
  }

  @SmallTest
  public void testCreateDictionary_fail() {
    SessionExecutor executor = createMock(SessionExecutor.class);
    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.CREATE_DICTIONARY)
            .setSessionId(100L)
            .setDictionaryName("")
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.DICTIONARY_NAME_EMPTY)
            .build());

    replayAll();

    UserDictionaryToolModel model = new UserDictionaryToolModel(executor);
    VisibilityProxy.setField(model, "sessionId", 100L);

    assertEquals(Status.DICTIONARY_NAME_EMPTY, model.createDictionary(""));
    verifyAll();
  }

  @SmallTest
  public void testRenameSelectedDictionary_success() {
    SessionExecutor executor = createMock(SessionExecutor.class);
    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.RENAME_DICTIONARY)
            .setSessionId(100L)
            .setDictionaryId(500L)
            .setDictionaryName("Another User Dictionary")
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
            .build());

    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.GET_USER_DICTIONARY_NAME_LIST)
            .setSessionId(100L)
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
            .setStorage(UserDictionaryStorage.newBuilder()
                .addDictionaries(UserDictionary.newBuilder()
                    .setName("User Dictionary 1")
                    .setId(1L))
                .addDictionaries(UserDictionary.newBuilder()
                    .setName("User Dictionary 2")
                    .setId(2L))
                .addDictionaries(UserDictionary.newBuilder()
                    .setName("Another User Dictionary")
                    .setId(500L)))
            .build());
    replayAll();

    UserDictionaryToolModel model = new UserDictionaryToolModel(executor);
    VisibilityProxy.setField(model, "sessionId", 100L);
    VisibilityProxy.setField(model, "selectedDictionaryId", 500L);

    assertEquals(Status.USER_DICTIONARY_COMMAND_SUCCESS,
                 model.renameSelectedDictionary("Another User Dictionary"));
    verifyAll();

    assertEquals(2, model.getSelectedDictionaryIndex());
    assertEquals("Another User Dictionary", model.getSelectedDictionaryName());
    assertContentsInOrder(model.getDictionaryNameList(),
                          "User Dictionary 1",
                          "User Dictionary 2",
                          "Another User Dictionary");
  }

  @SmallTest
  public void testRenameSelectedDictionary_fail() {
    SessionExecutor executor = createMock(SessionExecutor.class);
    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.RENAME_DICTIONARY)
            .setSessionId(100L)
            .setDictionaryId(500L)
            .setDictionaryName("")
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.DICTIONARY_NAME_EMPTY)
            .build());

    replayAll();

    UserDictionaryToolModel model = new UserDictionaryToolModel(executor);
    VisibilityProxy.setField(model, "sessionId", 100L);
    VisibilityProxy.setField(model, "selectedDictionaryId", 500L);

    assertEquals(Status.DICTIONARY_NAME_EMPTY, model.renameSelectedDictionary(""));
    verifyAll();
  }

  @SmallTest
  public void testDeleteSelectedDictionary_success() {
    SessionExecutor executor = createMock(SessionExecutor.class);

    class TestData extends Parameter {
      final UserDictionaryStorage storage;
      final int expectedSelectedDictionaryIndex;
      TestData(UserDictionaryStorage storage, int expectedSelectedDictionaryIndex) {
        this.storage = storage;
        this.expectedSelectedDictionaryIndex = expectedSelectedDictionaryIndex;
      }
    }

    TestData[] testDataList = {
        new TestData(
            UserDictionaryStorage.newBuilder()
                .addDictionaries(UserDictionary.newBuilder()
                    .setId(500L)
                    .setName("Target dictionary"))
                .addDictionaries(UserDictionary.newBuilder()
                    .setId(1L)
                    .setName("User Dictionary 1"))
                .addDictionaries(UserDictionary.newBuilder()
                    .setId(2L)
                    .setName("User Dictionary 2"))
                .build(),
            0),
        new TestData(
            UserDictionaryStorage.newBuilder()
                .addDictionaries(UserDictionary.newBuilder()
                    .setId(1L)
                    .setName("User Dictionary 1"))
                .addDictionaries(UserDictionary.newBuilder()
                    .setId(500L)
                    .setName("Target dictionary"))
                .addDictionaries(UserDictionary.newBuilder()
                    .setId(2L)
                    .setName("User Dictionary 2"))
                .build(),
            1),
        new TestData(
            UserDictionaryStorage.newBuilder()
                .addDictionaries(UserDictionary.newBuilder()
                    .setId(1L)
                    .setName("User Dictionary 1"))
                .addDictionaries(UserDictionary.newBuilder()
                    .setId(2L)
                    .setName("User Dictionary 2"))
                .addDictionaries(UserDictionary.newBuilder()
                    .setId(500L)
                    .setName("Target dictionary"))
                .build(),
            1),
    };

    for (TestData testData : testDataList) {
      resetAll();
      expect(executor.sendUserDictionaryCommand(
          UserDictionaryCommand.newBuilder()
              .setType(CommandType.DELETE_DICTIONARY)
              .setSessionId(100L)
              .setDictionaryId(500L)
              .setEnsureNonEmptyStorage(true)
              .build()))
          .andReturn(UserDictionaryCommandStatus.newBuilder()
              .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
              .build());

      expect(executor.sendUserDictionaryCommand(
          UserDictionaryCommand.newBuilder()
              .setType(CommandType.GET_USER_DICTIONARY_NAME_LIST)
              .setSessionId(100L)
              .build()))
          .andReturn(UserDictionaryCommandStatus.newBuilder()
              .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
              .setStorage(UserDictionaryStorage.newBuilder()
                  .addDictionaries(UserDictionary.newBuilder()
                      .setName("User Dictionary 1")
                      .setId(1L))
                  .addDictionaries(UserDictionary.newBuilder()
                      .setName("User Dictionary 2")
                      .setId(2L)))
              .build());
      replayAll();

      UserDictionaryToolModel model = new UserDictionaryToolModel(executor);
      VisibilityProxy.setField(model, "sessionId", 100L);
      VisibilityProxy.setField(model, "selectedDictionaryId", 500L);
      VisibilityProxy.setField(model, "storage", testData.storage);

      assertEquals(Status.USER_DICTIONARY_COMMAND_SUCCESS,
                   model.deleteSelectedDictionary());
      verifyAll();

      assertEquals(testData.expectedSelectedDictionaryIndex, model.getSelectedDictionaryIndex());
      assertContentsInOrder(model.getDictionaryNameList(),
                            "User Dictionary 1",
                            "User Dictionary 2");
    }
  }

  @SmallTest
  public void testDeleteSelectedDictionary_fail() {
    SessionExecutor executor = createMock(SessionExecutor.class);
    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.DELETE_DICTIONARY)
            .setSessionId(100L)
            .setDictionaryId(500L)
            .setEnsureNonEmptyStorage(true)
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.UNKNOWN_DICTIONARY_ID)
            .build());

    replayAll();

    UserDictionaryToolModel model = new UserDictionaryToolModel(executor);
    VisibilityProxy.setField(model, "sessionId", 100L);
    VisibilityProxy.setField(model, "selectedDictionaryId", 500L);

    assertEquals(Status.UNKNOWN_DICTIONARY_ID, model.deleteSelectedDictionary());
    verifyAll();
  }

  @SmallTest
  public void testGetEntryList() {
    SessionExecutor executor = createMock(SessionExecutor.class);
    UserDictionaryToolModel model = new UserDictionaryToolModel(executor);
    VisibilityProxy.setField(model, "sessionId", 100L);
    List<Entry> entryList = model.getEntryList();

    // Test without selected dictionary.
    resetAll();
    replayAll();
    assertEquals(0, entryList.size());
    verifyAll();

    // Test with selected dictionary.
    VisibilityProxy.setField(model, "selectedDictionaryId", 500L);
    resetAll();
    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.GET_ENTRY_SIZE)
            .setSessionId(100L)
            .setDictionaryId(500L)
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
            .setEntrySize(10)
            .build());
    replayAll();
    assertEquals(10, entryList.size());
    verifyAll();

    resetAll();
    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.GET_ENTRY)
            .setSessionId(100L)
            .setDictionaryId(500L)
            .addEntryIndex(5)
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
            .setEntry(Entry.newBuilder()
                .setKey("key")
                .setValue("value")
                .setPos(PosType.NOUN))
            .build());
    replayAll();
    Entry entry = entryList.get(5);

    verifyAll();
    assertEquals("key", entry.getKey());
    assertEquals("value", entry.getValue());
    assertEquals(PosType.NOUN, entry.getPos());
  }

  @SmallTest
  public void testEditTargetIndex() {
    SessionExecutor executor = createMock(SessionExecutor.class);
    UserDictionaryToolModel model = new UserDictionaryToolModel(executor);
    VisibilityProxy.setField(model, "sessionId", 100L);
    VisibilityProxy.setField(model, "selectedDictionaryId", 500L);

    replayAll();
    model.setEditTargetIndex(10);
    assertEquals(10, model.getEditTargetIndex());
    verifyAll();

    resetAll();
    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.GET_ENTRY)
            .setSessionId(100L)
            .setDictionaryId(500L)
            .addEntryIndex(10)
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
            .setEntry(Entry.newBuilder()
                .setKey("key")
                .setValue("value")
                .setPos(PosType.NOUN))
            .build());
    replayAll();

    Entry entry = model.getEditTargetEntry();

    verifyAll();
    assertEquals("key", entry.getKey());
    assertEquals("value", entry.getValue());
    assertEquals(PosType.NOUN, entry.getPos());
  }

  @SmallTest
  public void testCheckNewEntryAvailability() {
    SessionExecutor executor = createMock(SessionExecutor.class);
    UserDictionaryToolModel model = new UserDictionaryToolModel(executor);
    VisibilityProxy.setField(model, "sessionId", 100L);
    VisibilityProxy.setField(model, "selectedDictionaryId", 500L);

    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.CHECK_NEW_ENTRY_AVAILABILITY)
            .setSessionId(100L)
            .setDictionaryId(500L)
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
            .build());
    replayAll();

    assertEquals(Status.USER_DICTIONARY_COMMAND_SUCCESS, model.checkNewEntryAvailability());

    verifyAll();
  }

  @SmallTest
  public void testAddEntry() {
    SessionExecutor executor = createMock(SessionExecutor.class);
    UserDictionaryToolModel model = new UserDictionaryToolModel(executor);
    VisibilityProxy.setField(model, "sessionId", 100L);
    VisibilityProxy.setField(model, "selectedDictionaryId", 500L);

    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.ADD_ENTRY)
            .setSessionId(100L)
            .setDictionaryId(500L)
            .setEntry(Entry.newBuilder()
                .setKey("reading")
                .setValue("word")
                .setPos(PosType.NOUN))
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
            .build());
    replayAll();

    assertEquals(Status.USER_DICTIONARY_COMMAND_SUCCESS,
                 model.addEntry("word", "reading", PosType.NOUN));

    verifyAll();
    assertTrue(model.isDirty());
  }

  @SmallTest
  public void testEditEntry() {
    SessionExecutor executor = createMock(SessionExecutor.class);
    UserDictionaryToolModel model = new UserDictionaryToolModel(executor);
    VisibilityProxy.setField(model, "sessionId", 100L);
    VisibilityProxy.setField(model, "selectedDictionaryId", 500L);

    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.EDIT_ENTRY)
            .setSessionId(100L)
            .setDictionaryId(500L)
            .addEntryIndex(5)
            .setEntry(Entry.newBuilder()
                .setKey("reading")
                .setValue("word")
                .setPos(PosType.NOUN))
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
            .build());
    replayAll();

    model.setEditTargetIndex(5);
    assertEquals(Status.USER_DICTIONARY_COMMAND_SUCCESS,
                 model.editEntry("word", "reading", PosType.NOUN));

    verifyAll();
    assertTrue(model.isDirty());
  }

  @SmallTest
  public void testDeleteEntry() {
    SessionExecutor executor = createMock(SessionExecutor.class);
    UserDictionaryToolModel model = new UserDictionaryToolModel(executor);
    VisibilityProxy.setField(model, "sessionId", 100L);
    VisibilityProxy.setField(model, "selectedDictionaryId", 500L);

    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.DELETE_ENTRY)
            .setSessionId(100L)
            .setDictionaryId(500L)
            .addEntryIndex(5)
            .addEntryIndex(10)
            .addEntryIndex(100)
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
            .build());
    replayAll();

    model.setEditTargetIndex(5);
    assertEquals(Status.USER_DICTIONARY_COMMAND_SUCCESS,
                 model.deleteEntry(Arrays.asList(5, 10, 100)));

    verifyAll();
    assertTrue(model.isDirty());
  }

  @SmallTest
  public void testResetImportState() {
    UserDictionaryToolModel model = new UserDictionaryToolModel(null);

    Uri uri = Uri.fromFile(new File("DUMMY/FILE"));
    String importData = "dummy import data";
    ZipFile zipFile = createMockBuilder(ZipFile.class)
        .withConstructor(String.class)
        .withArgs(getInstrumentation().getContext().getApplicationInfo().sourceDir)
        .createMock();

    try {
      zipFile.close();
    } catch (IOException e) {
      // Just ignore.
    }
    replayAll();

    model.setImportUri(uri);
    model.setImportData(importData);
    model.setZipFile(zipFile);

    assertSame(uri, model.getImportUri());
    assertSame(importData, model.getImportData());
    assertSame(zipFile, model.getZipFile());

    model.resetImportState();
    verifyAll();
    assertNull(model.getImportUri());
    assertNull(model.getImportData());
    assertNull(model.getZipFile());
  }

  @SmallTest
  public void testReleaseZipFile() {
    UserDictionaryToolModel model = new UserDictionaryToolModel(null);

    ZipFile zipFile = createMockBuilder(ZipFile.class)
        .withConstructor(String.class)
        .withArgs(getInstrumentation().getContext().getApplicationInfo().sourceDir)
        .createMock();

    replayAll();
    model.setZipFile(zipFile);
    assertSame(zipFile, model.getZipFile());
    assertSame(zipFile, model.releaseZipFile());
    assertNull(model.getZipFile());
    assertNull(model.releaseZipFile());
    verifyAll();

    resetAll();
    try {
      zipFile.close();
    } catch (IOException e) {
      // Just ignore.
    }
    replayAll();
    model.setZipFile(zipFile);
    model.setZipFile(null);
    verifyAll();
  }

  @SmallTest
  public void testImportData() {
    SessionExecutor executor = createMock(SessionExecutor.class);
    UserDictionaryToolModel model = new UserDictionaryToolModel(executor);
    VisibilityProxy.setField(model, "sessionId", 100L);
    VisibilityProxy.setField(
        model, "storage",
        UserDictionaryStorage.newBuilder()
            .addDictionaries(UserDictionary.newBuilder()
                .setId(400L))
            .addDictionaries(UserDictionary.newBuilder()
                .setId(500L))
            .build());

    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.IMPORT_DATA)
            .setSessionId(100L)
            .setDictionaryId(500L)
            .setData("dummy import data")
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
            .setDictionaryId(500L)
            .build());

    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.GET_USER_DICTIONARY_NAME_LIST)
            .setSessionId(100L)
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
            .setStorage(UserDictionaryStorage.newBuilder()
                .addDictionaries(UserDictionary.newBuilder()
                    .setId(400L))
                .addDictionaries(UserDictionary.newBuilder()
                    .setId(500L)))
            .build());
    replayAll();

    model.setImportUri(Uri.fromFile(new File("dummy/file")));
    model.setImportData("dummy import data");
    assertEquals(Status.USER_DICTIONARY_COMMAND_SUCCESS, model.importData(1));

    verifyAll();

    assertTrue(model.isDirty());
    assertEquals(1, model.getSelectedDictionaryIndex());
  }

  @SmallTest
  public void testImportDataToNewDictionary() {
    SessionExecutor executor = createMock(SessionExecutor.class);
    UserDictionaryToolModel model = new UserDictionaryToolModel(executor);
    VisibilityProxy.setField(model, "sessionId", 100L);
    VisibilityProxy.setField(
        model, "storage",
        UserDictionaryStorage.newBuilder()
            .addDictionaries(UserDictionary.newBuilder()
                .setId(400L))
            .addDictionaries(UserDictionary.newBuilder()
                .setId(500L))
            .build());

    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.IMPORT_DATA)
            .setSessionId(100L)
            .setDictionaryName("dummyfile")
            .setData("dummy import data")
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
            .setDictionaryId(600L)
            .build());

    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.GET_USER_DICTIONARY_NAME_LIST)
            .setSessionId(100L)
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
            .setStorage(UserDictionaryStorage.newBuilder()
                .addDictionaries(UserDictionary.newBuilder()
                    .setId(400L))
                .addDictionaries(UserDictionary.newBuilder()
                    .setId(500L))
                .addDictionaries(UserDictionary.newBuilder()
                    .setId(600L)))
            .build());
    replayAll();

    model.setImportUri(Uri.fromFile(new File("dummy_directory/dummyfile.zip")));
    model.setImportData("dummy import data");
    assertEquals(Status.USER_DICTIONARY_COMMAND_SUCCESS, model.importData(-1));

    verifyAll();

    assertTrue(model.isDirty());
    assertEquals(2, model.getSelectedDictionaryIndex());
  }

  @SmallTest
  public void testImportData_partiallySuccess() {
    SessionExecutor executor = createMock(SessionExecutor.class);
    UserDictionaryToolModel model = new UserDictionaryToolModel(executor);
    VisibilityProxy.setField(model, "sessionId", 100L);
    VisibilityProxy.setField(
        model, "storage",
        UserDictionaryStorage.newBuilder()
            .addDictionaries(UserDictionary.newBuilder()
                .setId(400L))
            .addDictionaries(UserDictionary.newBuilder()
                .setId(500L))
            .build());

    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.IMPORT_DATA)
            .setSessionId(100L)
            .setDictionaryName("dummyfile")
            .setData("dummy import data")
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.IMPORT_INVALID_ENTRIES)
            .setDictionaryId(600L)
            .build());

    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.GET_USER_DICTIONARY_NAME_LIST)
            .setSessionId(100L)
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.USER_DICTIONARY_COMMAND_SUCCESS)
            .setStorage(UserDictionaryStorage.newBuilder()
                .addDictionaries(UserDictionary.newBuilder()
                    .setId(400L))
                .addDictionaries(UserDictionary.newBuilder()
                    .setId(500L))
                .addDictionaries(UserDictionary.newBuilder()
                    .setId(600L)))
            .build());
    replayAll();

    model.setImportUri(Uri.fromFile(new File("dummy_directory/dummyfile.zip")));
    model.setImportData("dummy import data");
    assertEquals(Status.IMPORT_INVALID_ENTRIES, model.importData(-1));

    verifyAll();

    assertTrue(model.isDirty());
    assertEquals(2, model.getSelectedDictionaryIndex());
  }

  @SmallTest
  public void testImportData_failed() {
    SessionExecutor executor = createMock(SessionExecutor.class);
    UserDictionaryToolModel model = new UserDictionaryToolModel(executor);
    VisibilityProxy.setField(model, "sessionId", 100L);
    VisibilityProxy.setField(model, "selectedDictionaryId", 400L);
    VisibilityProxy.setField(model, "dirty", false);
    VisibilityProxy.setField(
        model, "storage",
        UserDictionaryStorage.newBuilder()
            .addDictionaries(UserDictionary.newBuilder()
                .setId(400L))
            .addDictionaries(UserDictionary.newBuilder()
                .setId(500L))
            .build());

    expect(executor.sendUserDictionaryCommand(
        UserDictionaryCommand.newBuilder()
            .setType(CommandType.IMPORT_DATA)
            .setSessionId(100L)
            .setDictionaryId(500L)
            .setData("dummy import data")
            .build()))
        .andReturn(UserDictionaryCommandStatus.newBuilder()
            .setStatus(Status.UNKNOWN_ERROR)
            .build());
    replayAll();

    model.setImportUri(Uri.fromFile(new File("dummy/file")));
    model.setImportData("dummy import data");
    assertEquals(Status.UNKNOWN_ERROR, model.importData(1));

    verifyAll();

    // Regardless of the result status, the dirty bit should be set.
    assertTrue(model.isDirty());
    assertEquals(0, model.getSelectedDictionaryIndex());
  }
}
