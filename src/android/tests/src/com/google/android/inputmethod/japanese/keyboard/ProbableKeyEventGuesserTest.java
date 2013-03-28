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

package org.mozc.android.inputmethod.japanese.keyboard;

import static org.easymock.EasyMock.anyFloat;
import static org.easymock.EasyMock.anyObject;
import static org.easymock.EasyMock.capture;
import static org.easymock.EasyMock.expect;
import static org.easymock.EasyMock.expectLastCall;

import org.mozc.android.inputmethod.japanese.JapaneseKeyboard;
import org.mozc.android.inputmethod.japanese.JapaneseKeyboard.KeyboardSpecification;
import org.mozc.android.inputmethod.japanese.JapaneseKeyboardTest;
import org.mozc.android.inputmethod.japanese.keyboard.ProbableKeyEventGuesser.StaticticsLoader;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchAction;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchEvent;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchPosition;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.KeyEvent.ProbableKeyEvent;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import org.mozc.android.inputmethod.japanese.testing.Parameter;

import android.content.res.AssetManager;
import android.content.res.Configuration;
import android.os.Handler;
import android.os.Message;
import android.os.SystemClock;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.FloatMath;
import android.util.SparseArray;

import org.easymock.Capture;
import org.easymock.EasyMock;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;

/**
 */
public class ProbableKeyEventGuesserTest extends InstrumentationTestCaseWithMock {

  static String getFormattedKeyboardName(JapaneseKeyboard japaneseKeyboard,
                                         Configuration configuration) {
    return japaneseKeyboard.getSpecification().getKeyboardSpecificationName()
               .formattedKeyboardName(configuration);
  }

  @SmallTest
  public void testStaticticsLoader_constructor() {
    JapaneseKeyboard japaneseKeyboard = JapaneseKeyboardTest.createJapaneseKeyboard(
        KeyboardSpecification.GODAN_KANA, getInstrumentation());
    Configuration configuration = new Configuration();
    AssetManager assetManger = getInstrumentation().getTargetContext().getAssets();
    Map<String, SparseArray<float[]>> map = Collections.emptyMap();

    new StaticticsLoader(assetManger, japaneseKeyboard, configuration, map);
    new StaticticsLoader(assetManger, null, configuration, map);
    new StaticticsLoader(assetManger, japaneseKeyboard, null, map);
    try {
      new StaticticsLoader(null, japaneseKeyboard, configuration, map);
      fail("Non-null assetManager shouldn't be accepted.");
    } catch (NullPointerException e){
      // expected
    }
    try {
      new StaticticsLoader(assetManger, japaneseKeyboard, configuration, null);
      fail("Non-null mapToBeUpdated shouldn't be accepted.");
    } catch (NullPointerException e){
      // expected
    }
  }

  @SmallTest
  public void testIsAssetFileAvailable() throws IOException {
    Configuration configuration = new Configuration();
    configuration.orientation = Configuration.ORIENTATION_LANDSCAPE;
    StaticticsLoader loader = createMockBuilder(StaticticsLoader.class)
        .addMockedMethod("getAssetList")
        .withConstructor(AssetManager.class, JapaneseKeyboard.class, Configuration.class, Map.class)
        .withArgs(getInstrumentation().getTargetContext().getAssets(),
                  JapaneseKeyboardTest.createJapaneseKeyboard(
                      KeyboardSpecification.GODAN_KANA, getInstrumentation()),
                  configuration,
                  Collections.emptyMap())
        .createMock();

    resetAll();
    expect(loader.getAssetList())
        .andReturn(new String[] {"1", "2"});
    replayAll();
    assertTrue(loader.isAssetFileAvailable("1"));
    verifyAll();

    resetAll();
    expect(loader.getAssetList())
        .andReturn(new String[] {"1", "2"});
    replayAll();
    assertFalse(loader.isAssetFileAvailable("3"));
    verifyAll();

    try {
      loader.isAssetFileAvailable(String.format("3%s4", File.separator));
      fail("Separator character is not acceptable.");
    } catch (IllegalArgumentException e) {
      // Expected
    }
  }

  @SmallTest
  public void testStaticticsLoader_getStream() throws IOException {
    Configuration configuration = new Configuration();
    configuration.orientation = Configuration.ORIENTATION_LANDSCAPE;
    StaticticsLoader loader = createMockBuilder(StaticticsLoader.class)
        .addMockedMethod("isAssetFileAvailable")
        .addMockedMethod("openAssetStream")
        .withConstructor(AssetManager.class, JapaneseKeyboard.class, Configuration.class, Map.class)
        .withArgs(getInstrumentation().getTargetContext().getAssets(),
                  JapaneseKeyboardTest.createJapaneseKeyboard(
                      KeyboardSpecification.GODAN_KANA, getInstrumentation()),
                  configuration,
                  Collections.emptyMap())
        .createMock();
    InputStream expectedInputStream = getTestStream();
    expect(loader.isAssetFileAvailable("GODAN_KANA_LANDSCAPE.touch_stats"))
        .andReturn(true);
    expect(loader.openAssetStream("GODAN_KANA_LANDSCAPE.touch_stats"))
        .andReturn(expectedInputStream);

    replayAll();

    loader.getStream();

    verifyAll();
  }

  static final int[] SOURCE_ID_LIST = {10, 20};
  static final float[][] STATS_LIST = {
      {1f, 2f, 3f, 4f, 5f, 6f, 7f, 8f},
      {11f, 12f, 13f, 14f, 15f, 16f, 17f, 18f}};

  InputStream getTestStream() {
    try {
      ByteArrayOutputStream bytearrayOutputStream = new ByteArrayOutputStream();
      DataOutputStream outputStream = new DataOutputStream(bytearrayOutputStream);
      outputStream.writeInt(SOURCE_ID_LIST.length);  // Length
      for (int i = 0; i < SOURCE_ID_LIST.length; ++i) {
        outputStream.writeInt(SOURCE_ID_LIST[i]);   // Source Id
        for (float f : STATS_LIST[i]) {
          outputStream.writeFloat(f);
        }
      }
      return new ByteArrayInputStream(bytearrayOutputStream.toByteArray());
    } catch (IOException e) {
      throw new RuntimeException(e);
    }
  }

  @SmallTest
  public void testStaticticsLoader_readStream() throws IOException {
    JapaneseKeyboard japaneseKeyboard = JapaneseKeyboardTest.createJapaneseKeyboard(
        KeyboardSpecification.GODAN_KANA, getInstrumentation());
    Configuration configuration = new Configuration();
    configuration.orientation = Configuration.ORIENTATION_PORTRAIT;
    AssetManager assetManger = getInstrumentation().getTargetContext().getAssets();
    Map<String, SparseArray<float[]>> map = Collections.emptyMap();

    StaticticsLoader loader =
        new StaticticsLoader(assetManger, japaneseKeyboard, configuration, map);
    SparseArray<float[]> result = new SparseArray<float[]>(64);
    loader.readStream(new DataInputStream(getTestStream()), result);
    verifyReadStats(result);
  }

  /**
   * @param result
   */
  void verifyReadStats(SparseArray<float[]> result) {
    assertEquals(SOURCE_ID_LIST.length, result.size());
    for (int i = 0; i < SOURCE_ID_LIST.length; ++i) {
      float[] expectation = STATS_LIST[i];
      float[] actual = result.get(SOURCE_ID_LIST[i]);
      assertNotNull(actual);
      for (int j = 0; j < expectation.length; ++j) {
        assertEquals(expectation[j], actual[j]);
      }
      assertEquals(StaticticsLoader.getPrecalculatedDenominator(expectation),
                   actual[ProbableKeyEventGuesser.PRECALCULATED_DENOMINATOR]);
    }
  }

  @SmallTest
  public void testStaticticsLoader_getPrecalculatedDenominator() {
    float result = StaticticsLoader.getPrecalculatedDenominator(
        new float[] {Float.NaN, Float.NaN, 1, 2, Float.NaN, Float.NaN, 3, 4});
    assertEquals(FloatMath.sqrt(1 * 2 * 3 * 4), result);
  }

  @SmallTest
  public void testStaticticsLoader_run() {
    Configuration configuration = new Configuration();
    configuration.orientation = Configuration.ORIENTATION_LANDSCAPE;
    StaticticsLoader loader = createMockBuilder(StaticticsLoader.class)
        .addMockedMethod("getStream")
        .addMockedMethod("updateStats")
        .withConstructor(AssetManager.class, JapaneseKeyboard.class, Configuration.class, Map.class)
        .withArgs(getInstrumentation().getTargetContext().getAssets(),
                  JapaneseKeyboardTest.createJapaneseKeyboard(
                      KeyboardSpecification.GODAN_KANA, getInstrumentation()),
                  configuration,
                  Collections.emptyMap())
        .createMock();
    expect(loader.getStream()).andReturn(getTestStream());
    Capture<SparseArray<float[]>> capture = new Capture<SparseArray<float[]>>();
    loader.updateStats(capture(capture));

    replayAll();

    loader.run();

    verifyAll();

    verifyReadStats(capture.getValue());
  }

  @SuppressWarnings("unchecked")
  @SmallTest
  public void testStaticticsLoader_run_invalid() throws IOException {
    Configuration configuration = new Configuration();
    configuration.orientation = Configuration.ORIENTATION_LANDSCAPE;
    {
      // If japaneseKeyboard is null, no stream access should happen.
      StaticticsLoader loader = createMockBuilder(StaticticsLoader.class)
          .addMockedMethod("getStream")
          .addMockedMethod("updateStats")
          .withConstructor(AssetManager.class,
                           JapaneseKeyboard.class,
                           Configuration.class,
                           Map.class)
          .withArgs(getInstrumentation().getTargetContext().getAssets(),
                    JapaneseKeyboard.class.cast(null),
                    configuration,
                    Collections.emptyMap())
          .createMock();

      replayAll();

      loader.run();

      verifyAll();
    }
    {
      // If configuration is null, no stream access should happen.
      resetAll();
      StaticticsLoader loader = createMockBuilder(StaticticsLoader.class)
          .addMockedMethod("getStream")
          .addMockedMethod("updateStats")
          .withConstructor(AssetManager.class,
                           JapaneseKeyboard.class,
                           Configuration.class,
                           Map.class)
          .withArgs(getInstrumentation().getTargetContext().getAssets(),
                    JapaneseKeyboardTest.createJapaneseKeyboard(
                        KeyboardSpecification.GODAN_KANA, getInstrumentation()),
                    Configuration.class.cast(null),
                    Collections.emptyMap())
          .createMock();

      replayAll();

      loader.run();

      verifyAll();
    }
    {
      // If null stream is gotten, no update should happen.
      resetAll();
      StaticticsLoader loader = createMockBuilder(StaticticsLoader.class)
          .addMockedMethod("getStream")
          .addMockedMethod("updateStats")
          .withConstructor(AssetManager.class,
                           JapaneseKeyboard.class,
                           Configuration.class,
                           Map.class)
          .withArgs(getInstrumentation().getTargetContext().getAssets(),
                    JapaneseKeyboardTest.createJapaneseKeyboard(
                        KeyboardSpecification.GODAN_KANA, getInstrumentation()),
                    configuration,
                    Collections.emptyMap())
          .createMock();
      expect(loader.getStream()).andReturn(null);

      replayAll();

      loader.run();

      verifyAll();
    }
    {
      // If IOException is thrown on stream access, no update should happen.
      resetAll();
      StaticticsLoader loader = createMockBuilder(StaticticsLoader.class)
          .addMockedMethod("getStream")
          .addMockedMethod("readStream")
          .addMockedMethod("updateStats")
          .withConstructor(AssetManager.class,
                           JapaneseKeyboard.class,
                           Configuration.class,
                           Map.class)
          .withArgs(getInstrumentation().getTargetContext().getAssets(),
                    JapaneseKeyboardTest.createJapaneseKeyboard(
                        KeyboardSpecification.GODAN_KANA, getInstrumentation()),
                    configuration,
                    Collections.emptyMap())
          .createMock();
      expect(loader.getStream()).andReturn(getTestStream());
      loader.readStream(anyObject(DataInputStream.class),
                        anyObject(SparseArray.class));
      expectLastCall().andThrow(new IOException());

      replayAll();

      loader.run();

      verifyAll();
    }
  }

  @SmallTest
  public void testStaticticsLoader_updateStats() {
    StaticticsLoader loader = createMockBuilder(StaticticsLoader.class)
        .addMockedMethod("getMainLooperHandler")
        .addMockedMethod("createUpdateRunnable")
        .withConstructor(AssetManager.class,
                         JapaneseKeyboard.class,
                         Configuration.class,
                         Map.class)
        .withArgs(getInstrumentation().getTargetContext().getAssets(),
                  JapaneseKeyboardTest.createJapaneseKeyboard(
                      KeyboardSpecification.GODAN_KANA, getInstrumentation()),
                  new Configuration(),
                  Collections.emptyMap())
        .createMock();
    final Runnable runnable = new Runnable() {
      @Override
      public void run() {
      }
    };
    SparseArray<float[]> stats = new SparseArray<float[]>();
    expect(loader.createUpdateRunnable(stats)).andReturn(runnable);
    expect(loader.getMainLooperHandler()).andReturn(new Handler() {
      @Override
      public boolean sendMessageAtTime(Message msg, long uptimeMillis) {
        assertSame(runnable, msg.getCallback());
        assertTrue(uptimeMillis <= SystemClock.uptimeMillis());
        return true;
      }
    });

    replayAll();

    loader.updateStats(stats);

    verifyAll();
  }

  @SmallTest
  public void testStaticticsLoader_getUpdateRunnable() {
    JapaneseKeyboard japaneseKeyboard = JapaneseKeyboardTest.createJapaneseKeyboard(
        KeyboardSpecification.GODAN_KANA, getInstrumentation());
    Configuration configuration = new Configuration();
    configuration.orientation = Configuration.ORIENTATION_LANDSCAPE;
    AssetManager assetManger = getInstrumentation().getTargetContext().getAssets();
    Map<String, SparseArray<float[]>> map = new HashMap<String, SparseArray<float[]>>();

    StaticticsLoader loader =
        new StaticticsLoader(assetManger, japaneseKeyboard, configuration, map);
    assertTrue(loader.formattedKeyboardNameToStats.isEmpty());

    SparseArray<float[]> stats = new SparseArray<float[]>();
    loader.createUpdateRunnable(stats).run();
    assertEquals(1, loader.formattedKeyboardNameToStats.size());
    assertSame(stats,
               loader.formattedKeyboardNameToStats.get(
                   getFormattedKeyboardName(japaneseKeyboard, configuration)));
  }

  @SmallTest
  public void testGetLikelihood() {
    ProbableKeyEventGuesser guesser =
        new ProbableKeyEventGuesser(getInstrumentation().getTargetContext().getAssets());
    // The result itself is not tested.
    // Here we test "more likely touch point returns higher likelihood".
    float[] probableEvent = {0f, 0f, 1f, 1f, 0f, 0f, 1f, 1f, 1f};
    // <- high (near)        low (far) ->
    float[] testData = {0f, +0.1f, -0.2f, +0.4f, -0.8f, +1f, -1.2f};
    for (int i = 0; i < testData.length; ++i) {
      for (int j = i; j < testData.length; ++j) {
        String msg = i + " vs " + j;
        float valueI = (float) (testData[i] * ProbableKeyEventGuesser.LIKELIHOOD_THRESHOLD);
        float valueJ = (float) (testData[j] * ProbableKeyEventGuesser.LIKELIHOOD_THRESHOLD);
        assertTrue(msg + ", sx",
            guesser.getLikelihood(valueI, 0f, 0f, 0f, probableEvent)
            >= guesser.getLikelihood(valueJ, 0f, 0f, 0f, probableEvent));
        assertTrue(msg + ", sy",
            guesser.getLikelihood(0f, valueI, 0f, 0f, probableEvent)
            >= guesser.getLikelihood(0f, valueJ, 0f, 0f, probableEvent));
        assertTrue(msg + ", dx",
            guesser.getLikelihood(0f, 0f, valueI, 0f, probableEvent)
            >= guesser.getLikelihood(0f, 0f, valueJ, 0f, probableEvent));
        assertTrue(msg + ", dy",
            guesser.getLikelihood(0f, 0f, 0f, valueI, probableEvent)
            >= guesser.getLikelihood(0f, 0f, 0f, valueJ, probableEvent));
      }
    }
  }

  @SmallTest
  public void testConstructor() {
    try {
      ProbableKeyEventGuesser guesser = new ProbableKeyEventGuesser(null);
      fail("Non-null assetManager shouldn't be accepted.");
    } catch (NullPointerException e) {
      // Expected.
    }
    new ProbableKeyEventGuesser(getInstrumentation().getTargetContext().getAssets());
  }

  @SmallTest
  public void testSetJapaneseKeyboard() {
    JapaneseKeyboard godanKana = JapaneseKeyboardTest.createJapaneseKeyboard(
        KeyboardSpecification.GODAN_KANA, getInstrumentation());
    Configuration configuration = new Configuration();
    configuration.orientation = Configuration.ORIENTATION_LANDSCAPE;
    ProbableKeyEventGuesser guesser = createMockBuilder(ProbableKeyEventGuesser.class)
        .addMockedMethod("maybeUpdateEventStatistics")
        .withConstructor(AssetManager.class)
        .withArgs(getInstrumentation().getTargetContext().getAssets())
        .createMock();

    // JapaneseKeyboard == godanKana
    // Configuration == null
    // expected formattedKeyboardName == null
    guesser.maybeUpdateEventStatistics();
    expectLastCall().once();

    replayAll();

    guesser.setJapaneseKeyboard(godanKana);

    verifyAll();

    assertSame(godanKana, guesser.japaneseKeyboard);
    assertNull(guesser.formattedKeyboardName);

    resetAll();

    // JapaneseKeyboard == godanKana
    // Configuration == non-null
    // expected formattedKeyboardName == non-null
    guesser.maybeUpdateEventStatistics();
    expectLastCall().once();
    guesser.configuration = configuration;

    replayAll();

    guesser.setJapaneseKeyboard(godanKana);

    verifyAll();

    assertSame(godanKana, guesser.japaneseKeyboard);
    assertEquals(getFormattedKeyboardName(godanKana, configuration),
                 guesser.formattedKeyboardName);
  }

  @SmallTest
  public void testSetConfiguration() {
    JapaneseKeyboard godanKana = JapaneseKeyboardTest.createJapaneseKeyboard(
        KeyboardSpecification.GODAN_KANA, getInstrumentation());
    Configuration configuration = new Configuration();
    configuration.orientation = Configuration.ORIENTATION_LANDSCAPE;
    ProbableKeyEventGuesser guesser = createMockBuilder(ProbableKeyEventGuesser.class)
        .addMockedMethod("maybeUpdateEventStatistics")
        .withConstructor(AssetManager.class)
        .withArgs(getInstrumentation().getTargetContext().getAssets())
        .createMock();

    // JapaneseKeyboard == null
    // Configuration == non-null
    // expected formattedKeyboardName == null
    guesser.maybeUpdateEventStatistics();
    expectLastCall().once();

    replayAll();

    guesser.setConfiguration(configuration);

    verifyAll();

    assertSame(configuration, guesser.configuration);
    assertNull(guesser.japaneseKeyboard);

    resetAll();

    // JapaneseKeyboard == godanKana
    // Configuration == non-null
    // expected formattedKeyboardName == non-null
    guesser.maybeUpdateEventStatistics();
    expectLastCall().once();
    guesser.japaneseKeyboard = godanKana;

    replayAll();

    guesser.setConfiguration(configuration);

    verifyAll();

    assertSame(configuration, guesser.configuration);
    assertEquals(getFormattedKeyboardName(godanKana, configuration),
                 guesser.formattedKeyboardName);
  }

  @SmallTest
  public void testUpdateFormattedKeyboardName() {
    class TestData extends Parameter {
      final JapaneseKeyboard japaneseKeyboard;
      final Configuration configuration;
      final String expectation;
      TestData(JapaneseKeyboard japaneseKeyboard, Configuration configuration, String expectation) {
        this.japaneseKeyboard = japaneseKeyboard;
        this.configuration = configuration;
        this.expectation = expectation;
      }
    }

    JapaneseKeyboard japaneseKeyboard = JapaneseKeyboardTest.createJapaneseKeyboard(
        KeyboardSpecification.GODAN_KANA, getInstrumentation());
    Configuration configuration = new Configuration();
    configuration.orientation = Configuration.ORIENTATION_LANDSCAPE;

    TestData[] testDataList = {
        new TestData(null, null, null),
        new TestData(japaneseKeyboard, null, null),
        new TestData(null, configuration, null),
        new TestData(japaneseKeyboard, configuration,
                     getFormattedKeyboardName(japaneseKeyboard, configuration)),
    };
    ProbableKeyEventGuesser guesser =
        new ProbableKeyEventGuesser(getInstrumentation().getTargetContext().getAssets());

    for (TestData testData : testDataList) {
      guesser.japaneseKeyboard = testData.japaneseKeyboard;
      guesser.configuration = testData.configuration;
      guesser.updateFormattedKeyboardName();
      assertEquals(testData.expectation, guesser.formattedKeyboardName);
    }
  }

  @SmallTest
  public void testMaybeUpdateEventStatistics() {
    @SuppressWarnings("unchecked")
    BlockingQueue<Runnable> queue = createMock(BlockingQueue.class);
    ThreadPoolExecutor executorService =
        createMockBuilder(ThreadPoolExecutor.class)
            .withConstructor(Integer.TYPE, Integer.TYPE,
                             Long.TYPE, TimeUnit.class, BlockingQueue.class)
        .withArgs(0, 1, 60L, TimeUnit.SECONDS, new ArrayBlockingQueue<Runnable>(1)).createMock();
    ProbableKeyEventGuesser guesser =
        new ProbableKeyEventGuesser(getInstrumentation().getTargetContext().getAssets(),
                                    executorService);
    JapaneseKeyboard godanKana = JapaneseKeyboardTest.createJapaneseKeyboard(
        KeyboardSpecification.GODAN_KANA, getInstrumentation());
    JapaneseKeyboard qwertyAlphabet = JapaneseKeyboardTest.createJapaneseKeyboard(
        KeyboardSpecification.QWERTY_ALPHABET, getInstrumentation());
    JapaneseKeyboard twelveKeyFlickKana = JapaneseKeyboardTest.createJapaneseKeyboard(
        KeyboardSpecification.TWELVE_KEY_FLICK_KANA, getInstrumentation());
    Configuration configuration = new Configuration();
    configuration.orientation = Configuration.ORIENTATION_LANDSCAPE;
    SparseArray<float[]> valueGodanKana = new SparseArray<float[]>();
    guesser.formattedKeyboardNameToStats
        .put(getFormattedKeyboardName(godanKana, configuration), valueGodanKana);
    guesser.formattedKeyboardNameToStats
        .put(getFormattedKeyboardName(qwertyAlphabet, configuration), null);

    // godanKana - stats exists. Do nothing.
    guesser.formattedKeyboardName = getFormattedKeyboardName(godanKana, configuration);

    replayAll();

    guesser.maybeUpdateEventStatistics();

    verifyAll();

    // qwertyAlphabet - null stats exists. Do nothing.
    resetAll();
    guesser.formattedKeyboardName = getFormattedKeyboardName(qwertyAlphabet, configuration);

    replayAll();

    guesser.maybeUpdateEventStatistics();

    verifyAll();

    // twelveKeyFlickKana - no stats exists. Must be updated.
    resetAll();
    guesser.formattedKeyboardName = getFormattedKeyboardName(twelveKeyFlickKana, configuration);

    expect(executorService.getQueue()).andReturn(queue);
    expect(queue.iterator()).andReturn(new Iterator<Runnable>() {
      @Override
      public void remove() {
      }
      @Override
      public Runnable next() {
        return null;
      }
      @Override
      public boolean hasNext() {
        return false;
      }
    });
    executorService.execute(anyObject(StaticticsLoader.class));

    replayAll();

    guesser.maybeUpdateEventStatistics();

    verifyAll();

    // If the executor's queue is not empty, remove all the items in it and update.
    resetAll();
    guesser.formattedKeyboardName = getFormattedKeyboardName(twelveKeyFlickKana, configuration);
    expect(queue.iterator()).andReturn(new Iterator<Runnable>() {
      int count = 0;
      @Override
      public void remove() {
      }
      @Override
      public Runnable next() {
        ++count;
        return new Runnable(){
          @Override
          public void run() {
          }};
      }
      @Override
      public boolean hasNext() {
        return count == 0;
      }
    });
    expect(executorService.getQueue()).andReturn(queue);
    expect(executorService.remove(EasyMock.anyObject(Runnable.class))).andReturn(true);
    executorService.execute(anyObject(StaticticsLoader.class));

    replayAll();

    guesser.maybeUpdateEventStatistics();

    verifyAll();
  }

  @SuppressWarnings("unchecked")
  @SmallTest
  public void testGetProbableKeyEvents() {
    class TestData extends Parameter {
      final JapaneseKeyboard japaneseKeyboard;
      final Configuration configuration;
      final List<TouchEvent> touchEventList;
      final Map<String, SparseArray<float[]>> formattedKeyboardNameToStats;
      final boolean expectUpdateEventStatistics;
      final boolean expectNonNullResult;
      TestData(JapaneseKeyboard japaneseKeyboard,
          Configuration configuration,
          List<TouchEvent> touchEventList,
          Map<String, SparseArray<float[]>> formattedKeyboardNameToStats,
          boolean expectUpdateEventStatistics,
          boolean expectNonNullResult) {
        this.japaneseKeyboard = japaneseKeyboard;
        this.configuration = configuration;
        this.touchEventList = touchEventList;
        this.formattedKeyboardNameToStats = formattedKeyboardNameToStats;
        this.expectUpdateEventStatistics = expectUpdateEventStatistics;
        this.expectNonNullResult = expectNonNullResult;
      }
    }
    JapaneseKeyboard godanKeyboard = JapaneseKeyboardTest.createJapaneseKeyboard(
        KeyboardSpecification.GODAN_KANA, getInstrumentation());
    JapaneseKeyboard qwertyKeyboard = JapaneseKeyboardTest.createJapaneseKeyboard(
        KeyboardSpecification.QWERTY_KANA, getInstrumentation());
    Configuration configuration = new Configuration();
    configuration.orientation = Configuration.ORIENTATION_LANDSCAPE;
    List<TouchEvent> touchEventList =
        Collections.singletonList(
            TouchEvent.newBuilder()
                      .addStroke(TouchPosition.newBuilder().setAction(TouchAction.TOUCH_DOWN))
                      .addStroke(TouchPosition.newBuilder().setAction(TouchAction.TOUCH_UP))
                      .build());
    Map<String, SparseArray<float[]>> formattedKeyboardNameToStats =
        Collections.singletonMap(getFormattedKeyboardName(godanKeyboard, configuration),
                                                          new SparseArray<float[]>());
    List<ProbableKeyEvent> nonNullResult = Collections.<ProbableKeyEvent>emptyList();
    TestData[] testDataList = {
        new TestData(null, configuration, touchEventList, formattedKeyboardNameToStats,
                     false, false),
        new TestData(godanKeyboard, null, touchEventList, formattedKeyboardNameToStats,
                     false, false),
        new TestData(godanKeyboard, configuration, null, formattedKeyboardNameToStats,
                     false, false),
        new TestData(godanKeyboard, configuration, Collections.<TouchEvent>emptyList(),
                     formattedKeyboardNameToStats,
                     false, false),
        new TestData(godanKeyboard, configuration, touchEventList, formattedKeyboardNameToStats,
                     false, true),
        new TestData(qwertyKeyboard, configuration, touchEventList, formattedKeyboardNameToStats,
                     true, false),
        new TestData(godanKeyboard, configuration, touchEventList,
                     Collections.<String, SparseArray<float[]>>emptyMap(),
                     true, false),
    };

    ProbableKeyEventGuesser guesser = createMockBuilder(ProbableKeyEventGuesser.class)
        .addMockedMethod("getProbableKeyEventsInternal")
        .addMockedMethod("maybeUpdateEventStatistics")
        .withConstructor(AssetManager.class)
        .withArgs(getInstrumentation().getTargetContext().getAssets())
        .createMock();

    for (TestData testData : testDataList) {
      resetAll();
      guesser.japaneseKeyboard = testData.japaneseKeyboard;
      guesser.configuration = testData.configuration;
      if (testData.japaneseKeyboard != null && testData.configuration != null) {
        guesser.formattedKeyboardName =
            getFormattedKeyboardName(testData.japaneseKeyboard, testData.configuration);
      } else {
        guesser.formattedKeyboardName = null;
      }
      guesser.formattedKeyboardNameToStats.clear();
      guesser.formattedKeyboardNameToStats.putAll(testData.formattedKeyboardNameToStats);
      if (testData.expectUpdateEventStatistics) {
        guesser.maybeUpdateEventStatistics();
      }
      if (testData.expectNonNullResult) {
        expect(guesser.getProbableKeyEventsInternal(
                   anyObject(SparseArray.class),
                   anyFloat(),
                   anyFloat(),
                   anyFloat(),
                   anyFloat())).andReturn(nonNullResult);
      }
      replayAll();
      assertEquals(testData.expectNonNullResult ? nonNullResult : null,
                   guesser.getProbableKeyEvents(testData.touchEventList));
      verifyAll();
    }
  }

  @SmallTest
  public void testGetProbableKeyEventsInternal() {
    class TestData extends Parameter {
      double[] likelihood;
      List<ProbableKeyEvent> expectation;
      TestData(double[] likelihood, List<ProbableKeyEvent> expectation) {
        this.likelihood = likelihood;
        this.expectation = expectation;
      }
    }

    TestData[] testDataList = {
        new TestData(new double[] {}, null),
        new TestData(new double[] {0} , null),
        new TestData(new double[] {0, 0} , null),
        new TestData(new double[] {Double.NaN} , null),
        new TestData(new double[] {0.1d} ,
                     Arrays.asList(ProbableKeyEvent.newBuilder()
                                                   .setKeyCode(0)
                                                   .setProbability(1d)
                                                   .build())),
        new TestData(new double[] {100d, 100d} ,
                     Arrays.asList(ProbableKeyEvent.newBuilder()
                                                   .setKeyCode(0)
                                                   .setProbability(0.5d)
                                                   .build(),
                                   ProbableKeyEvent.newBuilder()
                                                   .setKeyCode(1)
                                                   .setProbability(0.5d)
                                                   .build())),
        new TestData(new double[] {100d, 100d} ,
                     Arrays.asList(ProbableKeyEvent.newBuilder()
                                                   .setKeyCode(0)
                                                   .setProbability(0.5d)
                                                   .build(),
                                   ProbableKeyEvent.newBuilder()
                                                   .setKeyCode(1)
                                                   .setProbability(0.5d)
                                                   .build())),
        new TestData(new double[] {1d, 3d} ,
                     Arrays.asList(ProbableKeyEvent.newBuilder()
                                                   .setKeyCode(0)
                                                   .setProbability(1d / 4d)
                                                   .build(),
                                   ProbableKeyEvent.newBuilder()
                                                   .setKeyCode(1)
                                                   .setProbability(3d / 4d)
                                                   .build())),
    };

    ProbableKeyEventGuesser guesser = createMockBuilder(ProbableKeyEventGuesser.class)
        .addMockedMethod("getLikelihoodArray")
        .withConstructor(AssetManager.class)
        .withArgs(getInstrumentation().getTargetContext().getAssets())
        .createMock();

    SparseArray<float[]> eventStatistics = new SparseArray<float[]>();
    float firstX = 1f;
    float firstY = 2f;
    float deltaX = 3f;
    float deltaY = 4f;

    for (TestData testData : testDataList) {
      resetAll();
      SparseArray<Double> likelihoodArray = new SparseArray<Double>();
      for (int i = 0; i < testData.likelihood.length; ++i) {
        likelihoodArray.put(i, testData.likelihood[i]);
      }

      expect(guesser.getLikelihoodArray(eventStatistics, firstX, firstY, deltaX, deltaY))
          .andReturn(likelihoodArray);

      replayAll();

      List<ProbableKeyEvent> result =
          guesser.getProbableKeyEventsInternal(eventStatistics, firstX, firstY, deltaX, deltaY);

      verifyAll();

      assertEquals(testData.toString(), testData.expectation, result);
    }
  }

  @SmallTest
  public void testGetKeycodeMapper() {
    ProbableKeyEventGuesser guesser =
        new ProbableKeyEventGuesser(getInstrumentation().getTargetContext().getAssets());
    Configuration configuration = new Configuration();
    configuration.orientation = Configuration.ORIENTATION_LANDSCAPE;
    guesser.setConfiguration(configuration);

    JapaneseKeyboard godanKana = JapaneseKeyboardTest.createJapaneseKeyboard(
        KeyboardSpecification.GODAN_KANA, getInstrumentation());
    JapaneseKeyboard qwertyAlphabet = JapaneseKeyboardTest.createJapaneseKeyboard(
        KeyboardSpecification.QWERTY_ALPHABET, getInstrumentation());

    // Register a mapper for GODAN_KANA.
    guesser.formattedKeyboardNameToKeycodeMapper.put(
        getFormattedKeyboardName(godanKana, configuration), new SparseArray<Integer>());
    assertEquals(1, guesser.formattedKeyboardNameToKeycodeMapper.size());

    // Get a mapper for GODAN_KANA. No new mapper should be registered.
    guesser.setJapaneseKeyboard(godanKana);
    assertNotNull(guesser.getKeycodeMapper());
    assertEquals(1, guesser.formattedKeyboardNameToKeycodeMapper.size());

    // Get a mapper for QWERTY_ALPHABET. A new mapper should be registered.
    guesser.setJapaneseKeyboard(qwertyAlphabet);
    assertNotNull(guesser.getKeycodeMapper());
    assertEquals(2, guesser.formattedKeyboardNameToKeycodeMapper.size());
  }

  @SmallTest
  public void testGetLikelihoodArray() {
    class TestData extends Parameter {
      final Integer[] keycodeMapping;
      final double[] likelihood;
      final boolean[] expectation;
      TestData(Integer[] keycodeMaping, double[] likelihood, boolean[] expectation) {
        this.keycodeMapping = keycodeMaping;
        this.likelihood = likelihood;
        this.expectation = expectation;
      }
    }
    float firstX = 1f;
    float firstY = 2f;
    float deltaX = 3f;
    float deltaY = 4f;
    TestData[] testDataList = {
        new TestData(new Integer[] {}, new double[] {}, new boolean[] {}),
        new TestData(new Integer[] {0, null, -1},
                     new double[] {10d, 10d, 10d},
                     new boolean[] {false, false, false}),
        new TestData(new Integer[] {1, 2},
                     new double[] {Double.NaN,
                                   ProbableKeyEventGuesser.LIKELIHOOD_THRESHOLD / 2, Double.NaN},
                     new boolean[] {false, false, false}),
        new TestData(new Integer[] {1, 2},
                     new double[] {Double.NaN, 1d, 2d},
                     new boolean[] {false, true, true}),
    };
    ProbableKeyEventGuesser guesser = createMockBuilder(ProbableKeyEventGuesser.class)
        .addMockedMethods("getLikelihood", "getKeycodeMapper")
        .withConstructor(AssetManager.class)
        .withArgs(getInstrumentation().getTargetContext().getAssets())
        .createMock();
    SparseArray<float[]> eventStatistics = new SparseArray<float[]>();
    SparseArray<Integer> keycodeMapper = new SparseArray<Integer>();
    for (TestData testData : testDataList) {
      resetAll();
      eventStatistics.clear();
      keycodeMapper.clear();
      expect(guesser.getKeycodeMapper()).andReturn(keycodeMapper);
      for (int sourceId = 0; sourceId < testData.keycodeMapping.length; ++sourceId) {
        float[] statistics = new float[0];
        eventStatistics.append(sourceId, statistics);
        Integer keyCode = testData.keycodeMapping[sourceId];
        keycodeMapper.append(sourceId, keyCode);
        if (keyCode != null && keyCode > 0) {
          expect(guesser.getLikelihood(firstX, firstY, deltaX, deltaY, statistics))
              .andReturn(testData.likelihood[keyCode]);
        }
      }

      replayAll();

      SparseArray<Double> result =
          guesser.getLikelihoodArray(eventStatistics, firstX, firstY, deltaX, deltaY);

      verifyAll();

      for (int keyCode = 0; keyCode < testData.expectation.length; ++keyCode) {
        if (testData.expectation[keyCode]) {
          assertEquals(testData.likelihood[keyCode], result.get(keyCode));
        } else {
          assertNull(result.get(keyCode));
        }
      }
    }
  }
}
