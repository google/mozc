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

package org.mozc.android.inputmethod.japanese.util;

import static org.easymock.EasyMock.eq;
import static org.easymock.EasyMock.expect;
import static org.easymock.EasyMock.isA;
import static org.easymock.EasyMock.same;

import org.mozc.android.inputmethod.japanese.testing.ApiLevel;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import org.mozc.android.inputmethod.japanese.testing.Parameter;
import org.mozc.android.inputmethod.japanese.util.ImeSwitcherFactory.ImeSwitcher;
import org.mozc.android.inputmethod.japanese.util.ImeSwitcherFactory.SubtypeImeSwitcher;
import org.mozc.android.inputmethod.japanese.util.ImeSwitcherFactory.SubtypeImeSwitcher.InputMethodManagerProxy;

import android.content.ComponentName;
import android.inputmethodservice.InputMethodService;
import android.os.Binder;
import android.os.IBinder;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.inputmethod.InputMethodInfo;
import android.view.inputmethod.InputMethodSubtype;

import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 */
public class ImeSwitcherFactoryTest extends InstrumentationTestCaseWithMock {

  @SmallTest
  @ApiLevel(ImeSwitcherFactory.SUBTYPE_TARGTET_API_LEVEL)
  public void testIsVoiceImeAvailable() {
    class TestData extends Parameter {
      final Map<InputMethodInfo, List<InputMethodSubtype>> availableImes;
      final String locale;
      final boolean expectIsVoiceImeAvailable;
      final String expectSwitchId;
      final InputMethodSubtype expectSwitchSubtype;

      public TestData(Map<InputMethodInfo, List<InputMethodSubtype>> availableImes,
          String locale,
          boolean expectIsVoiceImeAvailable,
          String expectSwitchId,
          InputMethodSubtype expectSwitchSubtype) {
        this.availableImes = availableImes;
        this.locale = locale;
        this.expectIsVoiceImeAvailable = expectIsVoiceImeAvailable;
        this.expectSwitchId = expectSwitchId;
        this.expectSwitchSubtype = expectSwitchSubtype;
      }
    }
    InputMethodInfo thirdPatyIme = new InputMethodInfo("3rd.party", "", "", "");
    InputMethodInfo googleIme1 = new InputMethodInfo("com.google.android.testime1", "", "", "");
    InputMethodInfo googleIme2 = new InputMethodInfo("com.google.android.testime2", "", "", "");
    String googleIme1Id =
        new ComponentName(googleIme1.getPackageName(), googleIme1.getServiceName())
            .flattenToShortString();
    String googleIme2Id =
        new ComponentName(googleIme2.getPackageName(), googleIme2.getServiceName())
            .flattenToShortString();

    InputMethodSubtype jaKeyboardAux =
        new InputMethodSubtype(0, 0, "ja", "keyboard", "", true, false);
    InputMethodSubtype jaVoiceNonaux =
        new InputMethodSubtype(0, 0, "ja", "voice", "", false, false);
    InputMethodSubtype jaVoiceAux =
        new InputMethodSubtype(0, 0, "ja", "voice", "", true, false);
    InputMethodSubtype enKeyboardAux =
        new InputMethodSubtype(0, 0, "en", "keyboard", "", true, false);
    InputMethodSubtype enVoiceNonaux =
        new InputMethodSubtype(0, 0, "en", "voice", "", false, false);
    InputMethodSubtype enVoiceAux =
        new InputMethodSubtype(0, 0, "en", "voice", "", true, false);

    Map<InputMethodInfo, List<InputMethodSubtype>> noImes = Collections.emptyMap();
    Map<InputMethodInfo, List<InputMethodSubtype>> only3rdParty =
        Collections.singletonMap(thirdPatyIme, Collections.singletonList(jaVoiceAux));
    Map<InputMethodInfo, List<InputMethodSubtype>> noSubtype =
        Collections.singletonMap(googleIme1, Collections.<InputMethodSubtype>emptyList());
    Map<InputMethodInfo, List<InputMethodSubtype>> noEligibleSubtype =
        Collections.singletonMap(googleIme1,
                                 Arrays.asList(jaKeyboardAux, jaVoiceNonaux,
                                               enKeyboardAux, enVoiceNonaux));
    Map<InputMethodInfo, List<InputMethodSubtype>> jaVoice =
        Collections.singletonMap(googleIme1, Collections.singletonList(jaVoiceAux));
    Map<InputMethodInfo, List<InputMethodSubtype>> jaEnVoice =
        Collections.singletonMap(googleIme1, Arrays.asList(jaVoiceAux, enVoiceAux));
    Map<InputMethodInfo, List<InputMethodSubtype>> jaVoiceIn1EnVoiceIn2 =
        new HashMap<InputMethodInfo, List<InputMethodSubtype>>();
    jaVoiceIn1EnVoiceIn2.put(googleIme1, Collections.singletonList(jaVoiceAux));
    jaVoiceIn1EnVoiceIn2.put(googleIme2, Collections.singletonList(enVoiceAux));

    TestData[] testDataList = {
      new TestData(noImes, "ja", false, null, null),
      new TestData(only3rdParty, "ja", false, null, null),
      new TestData(noSubtype, "ja", false, null, null),
      new TestData(noEligibleSubtype, "ja", false, null, null),
      new TestData(jaVoice, "ja", true, googleIme1Id, jaVoiceAux),
      new TestData(jaVoice, "en", true, googleIme1Id, jaVoiceAux),
      new TestData(jaEnVoice, "ja", true, googleIme1Id, jaVoiceAux),
      new TestData(jaEnVoice, "en", true, googleIme1Id, enVoiceAux),
      new TestData(jaEnVoice, "invalid", true, googleIme1Id, enVoiceAux),
      new TestData(jaVoiceIn1EnVoiceIn2, "ja", true, googleIme1Id, jaVoiceAux),
      new TestData(jaVoiceIn1EnVoiceIn2, "en", true, googleIme2Id, enVoiceAux),
    };

    for (TestData testData : testDataList) {
      resetAll();
      InputMethodManagerProxy subtypeProxy = createMock(InputMethodManagerProxy.class);
      expect(subtypeProxy.getShortcutInputMethodsAndSubtypes())
          .andStubReturn(testData.availableImes);
      if (testData.expectIsVoiceImeAvailable) {
        subtypeProxy.setInputMethodAndSubtype(isA(IBinder.class),
                                              eq(testData.expectSwitchId),
                                              same(testData.expectSwitchSubtype));
      }
      replayAll();
      ImeSwitcher switcher = new SubtypeImeSwitcher(new InputMethodService(), subtypeProxy) {
        @Override
        protected IBinder getToken() {
          return new Binder();
        }
      };
      assertEquals(testData.toString(),
                   testData.expectIsVoiceImeAvailable, switcher.isVoiceImeAvailable());
      switcher.switchToVoiceIme(testData.locale);
      verifyAll();
    }
  }

  @SmallTest
  public void testSmokeTest() {
    ImeSwitcherFactory.getImeSwitcher(new InputMethodService());
  }
}
