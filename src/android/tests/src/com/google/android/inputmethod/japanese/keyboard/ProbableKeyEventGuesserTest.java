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

package org.mozc.android.inputmethod.japanese.keyboard;

import org.mozc.android.inputmethod.japanese.JapaneseKeyboard;
import org.mozc.android.inputmethod.japanese.JapaneseKeyboard.KeyboardSpecification;
import org.mozc.android.inputmethod.japanese.JapaneseKeyboardTest;
import org.mozc.android.inputmethod.japanese.keyboard.ProbableKeyEventGuesser.LikelihoodCalculator;
import org.mozc.android.inputmethod.japanese.keyboard.ProbableKeyEventGuesser.LikelihoodCalculatorImpl;
import org.mozc.android.inputmethod.japanese.keyboard.ProbableKeyEventGuesser.StatisticsLoader;
import org.mozc.android.inputmethod.japanese.keyboard.ProbableKeyEventGuesser.StatisticsLoader.UpdateStatsListener;
import org.mozc.android.inputmethod.japanese.keyboard.ProbableKeyEventGuesser.StatsFileAccessor;
import org.mozc.android.inputmethod.japanese.keyboard.ProbableKeyEventGuesser.StatsFileAccessorImpl;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchAction;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchEvent;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchPosition;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.KeyEvent.ProbableKeyEvent;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import org.mozc.android.inputmethod.japanese.testing.Parameter;
import com.google.common.collect.Lists;
import com.google.common.io.ByteArrayDataOutput;
import com.google.common.io.ByteStreams;

import android.content.res.Configuration;
import android.test.MoreAsserts;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.SparseArray;
import android.util.SparseIntArray;

import org.easymock.Capture;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.Executor;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;

/**
 */
public class ProbableKeyEventGuesserTest extends InstrumentationTestCaseWithMock {

  private static class BlockingThreadPoolExecutor extends ThreadPoolExecutor {
    public BlockingThreadPoolExecutor() {
      super(0, 1, 60L, TimeUnit.SECONDS, new ArrayBlockingQueue<Runnable>(1));
    }
    @Override
    public boolean remove(Runnable task) {
      return true;
    }
    @Override
    public void execute(Runnable command) {
      command.run();
    }
  }

  private static class BlockingExecutor implements Executor {
    @Override
    public void execute(Runnable command) {
      command.run();
    }
  }

  private static final List<TouchEvent> TOUCH_DOWN_UP_EVENT_LIST =
      Collections.singletonList(
          TouchEvent.newBuilder()
                    .addStroke(TouchPosition.newBuilder().setAction(TouchAction.TOUCH_DOWN))
                    .addStroke(TouchPosition.newBuilder().setAction(TouchAction.TOUCH_UP))
                    .build());

  private static String getFormattedKeyboardName(JapaneseKeyboard japaneseKeyboard,
                                                 Configuration configuration) {
    return japaneseKeyboard.getSpecification().getKeyboardSpecificationName()
               .formattedKeyboardName(configuration);
  }

  private static InputStream createStream(int[] sourceIds, float[][] stats) {
    ByteArrayDataOutput b = ByteStreams.newDataOutput();
    b.writeInt(stats.length);
    if (sourceIds.length != stats.length) {
      throw new IllegalArgumentException("sourceIds and stats must be the same size.");
    }
    for (int i = 0; i < sourceIds.length; ++i) {
      b.writeInt(sourceIds[i]);
      float[] dataList = stats[i];
      for (float data : dataList) {
        b.writeFloat(data);
      }
    }
    return new ByteArrayInputStream(b.toByteArray());
  }

  private ProbableKeyEventGuesser createFakeGuesser(double threshold) {
    return new ProbableKeyEventGuesser(
        new StatsFileAccessorImpl(getInstrumentation().getTargetContext().getAssets()),
        threshold,
        new BlockingThreadPoolExecutor(),
        new BlockingExecutor(),
        new LikelihoodCalculatorImpl());
  }

  @SmallTest
  public void testStaticticsLoader_run() {
     int[] sourceIdList = {10, 20};
     float[][] statsList = {
         {1f, 2f, 3f, 4f, 5f, 6f, 7f, 8f},
         {11f, 12f, 13f, 14f, 15f, 16f, 17f, 18f}};

    final InputStream testStream = createStream(sourceIdList, statsList);
    final Configuration configuration = new Configuration();
    configuration.orientation = Configuration.ORIENTATION_LANDSCAPE;
    final JapaneseKeyboard japaneseKeyboard = JapaneseKeyboardTest.createJapaneseKeyboard(
        KeyboardSpecification.GODAN_KANA, getInstrumentation());
    final Capture<SparseArray<float[]>> capture = new Capture<SparseArray<float[]>>();
    StatisticsLoader loader = new StatisticsLoader(
        new StatsFileAccessor() {
          @Override
          public InputStream openStream(
              JapaneseKeyboard japaneseKeyboardToLoad, Configuration configurationToLoad) {
            assertSame(japaneseKeyboard, japaneseKeyboardToLoad);
            assertSame(configuration, configurationToLoad);
            return testStream;
          }},
          japaneseKeyboard,
        configuration,
        Collections.<String, SparseArray<float[]>>emptyMap(),
        new UpdateStatsListener() {
          @Override
          public void updateStats(String formattedKeyboardName, SparseArray<float[]> stats) {
            assertEquals(
                getFormattedKeyboardName(japaneseKeyboard, configuration),
                formattedKeyboardName);
            capture.setValue(stats);
          }
        });
    loader.run();

    assertEquals(sourceIdList.length, capture.getValue().size());
    for (int i = 0; i < sourceIdList.length; ++i) {
      float[] expectation = statsList[i];
      float[] actual = capture.getValue().get(sourceIdList[i]);
      assertNotNull(actual);
      for (int j = 0; j < expectation.length; ++j) {
        assertEquals(expectation[j], actual[j]);
      }
    }
  }

  @SmallTest
  public void testStaticticsLoader_run_loadNonExistentFile() {
    Configuration configuration = new Configuration();
    configuration.orientation = Configuration.ORIENTATION_LANDSCAPE;
    StatisticsLoader loader = new StatisticsLoader(
        new StatsFileAccessor() {
          @Override
          public InputStream openStream(
              JapaneseKeyboard japaneseKeyboard, Configuration configuration) throws IOException {
            throw new IOException("No file found");
          }
        },
        JapaneseKeyboardTest.createJapaneseKeyboard(KeyboardSpecification.GODAN_KANA,
                                                    getInstrumentation()),
        configuration,
        Collections.<String, SparseArray<float[]>>emptyMap(),
        new UpdateStatsListener() {
          @Override
          public void updateStats(String formattedKeyboardName, SparseArray<float[]> stats) {
            fail("Should not be called.");
          }
        });
    loader.run();
  }

  @SuppressWarnings("unchecked")
  @SmallTest
  public void testStaticticsLoader_run_IOException() {
    Configuration configuration = new Configuration();
    configuration.orientation = Configuration.ORIENTATION_LANDSCAPE;
    // If IOException is thrown on stream access, no update should happen.
    StatisticsLoader loader = new StatisticsLoader(
        new StatsFileAccessor() {
          @Override
          public InputStream openStream(
              JapaneseKeyboard japaneseKeyboard, Configuration configuration) {
            return new InputStream() {
              @Override
              public int read() throws IOException {
                throw new IOException();
              }
            };
          }},
        JapaneseKeyboardTest.createJapaneseKeyboard(
            KeyboardSpecification.GODAN_KANA, getInstrumentation()),
        configuration,
        Collections.<String, SparseArray<float[]>>emptyMap(),
        new UpdateStatsListener() {
            @Override
            public void updateStats(String formattedKeyboardName, SparseArray<float[]> stats) {
              fail("Should not be called.");
            }
          });
    loader.run();
  }

  @SmallTest
  public void testGetLikelihood() {
    LikelihoodCalculator calculator = new LikelihoodCalculatorImpl();
    // The result itself is not tested.
    // Here we test "more likely touch point returns higher likelihood".
    float[] probableEvent = {0f, 0f, 1f, 1f, 0f, 0f, 1f, 1f, 1f};
    // <- high (near)        low (far) ->
    float[] testData = {0f, +0.1f, -0.2f, +0.4f, -0.8f, +1f, -1.2f};
    for (int i = 0; i < testData.length; ++i) {
      for (int j = i; j < testData.length; ++j) {
        String msg = i + " vs " + j;
        float valueI = testData[i];
        float valueJ = testData[j];
        assertTrue(msg + ", sx",
            calculator.getLikelihood(valueI, 0f, 0f, 0f, probableEvent)
            >= calculator.getLikelihood(valueJ, 0f, 0f, 0f, probableEvent));
        assertTrue(msg + ", sy",
            calculator.getLikelihood(0f, valueI, 0f, 0f, probableEvent)
            >= calculator.getLikelihood(0f, valueJ, 0f, 0f, probableEvent));
        assertTrue(msg + ", dx",
            calculator.getLikelihood(0f, 0f, valueI, 0f, probableEvent)
            >= calculator.getLikelihood(0f, 0f, valueJ, 0f, probableEvent));
        assertTrue(msg + ", dy",
            calculator.getLikelihood(0f, 0f, 0f, valueI, probableEvent)
            >= calculator.getLikelihood(0f, 0f, 0f, valueJ, probableEvent));
      }
    }
  }

  @SmallTest
  public void testConstructor() {
    try {
      // TODO(matsuzakit): Introduce NullPointerTester. Unfortunately it's not runnable on Android.
      ProbableKeyEventGuesser guesser = new ProbableKeyEventGuesser(null);
      fail("Non-null assetManager shouldn't be accepted.");
    } catch (NullPointerException e) {
      // Expected.
    }
    new ProbableKeyEventGuesser(getInstrumentation().getTargetContext().getAssets());
  }

  @SmallTest
  public void testSetJapaneseKeyboardWithNullConfig() {
    JapaneseKeyboard godanKana = JapaneseKeyboardTest.createJapaneseKeyboard(
        KeyboardSpecification.GODAN_KANA, getInstrumentation());
    ProbableKeyEventGuesser guesser = createFakeGuesser(0);

    // JapaneseKeyboard == godanKana
    // Configuration == null
    guesser.setJapaneseKeyboard(godanKana);
    guesser.setConfiguration(null);

    MoreAsserts.assertEmpty(
        guesser.getProbableKeyEvents(Arrays.asList(TouchEvent.getDefaultInstance())));
  }

  @SmallTest
  public void testSetConfigWithNullJanapaneseKeyboard() {
    Configuration configuration = new Configuration();
    configuration.orientation = Configuration.ORIENTATION_LANDSCAPE;
    ProbableKeyEventGuesser guesser = createFakeGuesser(0);

    // JapaneseKeyboard == null
    // Configuration == non-null
    guesser.setJapaneseKeyboard(null);
    guesser.setConfiguration(configuration);

    MoreAsserts.assertEmpty(
        guesser.getProbableKeyEvents(Arrays.asList(TouchEvent.getDefaultInstance())));
  }

  @SmallTest
  public void testNonExistentJanapaneseKeyboard() {
    // This test expects that HARDWARE_QWERTY_ALPHABET doesn't have corresponding
    // typing correction stats.
    JapaneseKeyboard keyboard =
        new JapaneseKeyboard(
            Collections.<Row>emptyList(), 0f, KeyboardSpecification.HARDWARE_QWERTY_ALPHABET);
    Configuration configuration = new Configuration();
    configuration.orientation = Configuration.ORIENTATION_LANDSCAPE;
    ProbableKeyEventGuesser guesser = createFakeGuesser(0);

    guesser.setConfiguration(configuration);
    guesser.setJapaneseKeyboard(keyboard);

    MoreAsserts.assertEmpty(
        guesser.getProbableKeyEvents(Arrays.asList(TouchEvent.getDefaultInstance())));
  }

  @SmallTest
  public void testCorrectKeyboard() {
    JapaneseKeyboard godanKana = JapaneseKeyboardTest.createJapaneseKeyboard(
        KeyboardSpecification.GODAN_KANA, getInstrumentation());
    Configuration configuration = new Configuration();
    configuration.orientation = Configuration.ORIENTATION_LANDSCAPE;
    ProbableKeyEventGuesser guesser = createFakeGuesser(Double.NEGATIVE_INFINITY);

    guesser.setConfiguration(configuration);
    guesser.setJapaneseKeyboard(godanKana);

    MoreAsserts.assertNotEmpty(guesser.getProbableKeyEvents(TOUCH_DOWN_UP_EVENT_LIST));
  }

  @SmallTest
  public void testFilterLessProbableEvents() {
    JapaneseKeyboard godanKana = JapaneseKeyboardTest.createJapaneseKeyboard(
        KeyboardSpecification.GODAN_KANA, getInstrumentation());
    Configuration configuration = new Configuration();
    configuration.orientation = Configuration.ORIENTATION_LANDSCAPE;
    ProbableKeyEventGuesser guesser = createFakeGuesser(Double.POSITIVE_INFINITY);

    guesser.setConfiguration(configuration);
    guesser.setJapaneseKeyboard(godanKana);

    MoreAsserts.assertEmpty(guesser.getProbableKeyEvents(TOUCH_DOWN_UP_EVENT_LIST));
  }

  @SmallTest
  public void testEmptyTouchEvent() {
    JapaneseKeyboard godanKana = JapaneseKeyboardTest.createJapaneseKeyboard(
        KeyboardSpecification.GODAN_KANA, getInstrumentation());
    Configuration configuration = new Configuration();
    configuration.orientation = Configuration.ORIENTATION_LANDSCAPE;
    ProbableKeyEventGuesser guesser = createFakeGuesser(0);

    guesser.setConfiguration(configuration);
    guesser.setJapaneseKeyboard(godanKana);

    MoreAsserts.assertEmpty(guesser.getProbableKeyEvents(null));
    MoreAsserts.assertEmpty(guesser.getProbableKeyEvents(Collections.<TouchEvent>emptyList()));
    MoreAsserts.assertEmpty(guesser.getProbableKeyEvents(
        Collections.<TouchEvent>singletonList(TouchEvent.getDefaultInstance())));
  }

  @SmallTest
  public void testVariousStatistics() {
    final JapaneseKeyboard godanKana = JapaneseKeyboardTest.createJapaneseKeyboard(
        KeyboardSpecification.GODAN_KANA, getInstrumentation());
    final Configuration configuration = new Configuration();
    configuration.orientation = Configuration.ORIENTATION_LANDSCAPE;

    class TestData extends Parameter {
      int[] sourceIds;
      float[][] stats;
      List<ProbableKeyEvent> expectation;
      TestData(int[] sourceIds, float[][] stats, List<ProbableKeyEvent> expectation) {
        this.sourceIds = sourceIds;
        this.stats = stats;
        this.expectation = expectation;
      }
    }

    final float startX = 1;
    final float startY = 2;
    final float deltaX = 3;
    final float deltaY = 4;

    LikelihoodCalculator fakeCalculator = new LikelihoodCalculator() {
      @Override
      public double getLikelihood(
          float sx, float sy, float dx, float dy, float[] probableEvent) {
        assertEquals(startX, sx);
        assertEquals(startY, sy);
        assertEquals(deltaX, dx);
        assertEquals(deltaY, dy);
        return probableEvent[0];
      }
    };

    // Key code for souceId=1 and 2.
    // The value can be obtained by ProbableKeyEventGuesser#getKeycodeMapper() but
    // here we don't have available one.
    // We should get the value from JapaneseKeyboard after refactoring.
    int keyCodeForSourceId1 = 97;
    int keyCodeForSourceId2 = 36;
    TestData[] testDataList = {
        new TestData(
            new int[] {},
            new float[][] {},
            Collections.<ProbableKeyEvent>emptyList()),
        new TestData(
            new int[] {1},
            new float[][] {
                {0, 0, 0, 0, 0, 0, 0, 0}},
            Collections.<ProbableKeyEvent>emptyList()),
        new TestData(
            new int[] {1, 2},
            new float[][] {
                {1f, 0, 0, 0, 0, 0, 0, 0},
                {1f, 0, 0, 0, 0, 0, 0, 0}},
            Lists.newArrayList(
                ProbableKeyEvent.newBuilder()
                    .setKeyCode(keyCodeForSourceId1).setProbability(1f / 2f).build(),
                ProbableKeyEvent.newBuilder()
                    .setKeyCode(keyCodeForSourceId2).setProbability(1f / 2f).build())),
        new TestData(
            new int[] {1, 2},
            new float[][] {
                {1f, 0, 0, 0, 0, 0, 0, 0},
                {3f, 0, 0, 0, 0, 0, 0, 0}},
            Lists.newArrayList(
                ProbableKeyEvent.newBuilder()
                    .setKeyCode(keyCodeForSourceId1).setProbability(1f / 4f).build(),
                ProbableKeyEvent.newBuilder()
                    .setKeyCode(keyCodeForSourceId2).setProbability(3f / 4f).build())),
        // The result of souceId=3 is too small so no corresponding result for it.
        new TestData(
            new int[] {1, 2, 3},
            new float[][] {
                {1f, 0, 0, 0, 0, 0, 0, 0},
                {1f, 0, 0, 0, 0, 0, 0, 0},
                {0, 0, 0, 0, 0, 0, 0, 0}},
            Lists.newArrayList(
                ProbableKeyEvent.newBuilder()
                    .setKeyCode(keyCodeForSourceId1).setProbability(1f / 2f).build(),
                ProbableKeyEvent.newBuilder()
                    .setKeyCode(keyCodeForSourceId2).setProbability(1f / 2f).build())),
    };

    for (final TestData testData : testDataList) {
      StatsFileAccessor assetManager = new StatsFileAccessor() {
        @Override
        public InputStream openStream(
            JapaneseKeyboard japaneseKeyboardToLoad, Configuration configurationToLoad) {
          assertSame(godanKana, japaneseKeyboardToLoad);
          assertSame(configuration, configurationToLoad);
          return createStream(testData.sourceIds, testData.stats);
        }
      };

      ProbableKeyEventGuesser guesser =
          new ProbableKeyEventGuesser(
              assetManager,
              0,
              new BlockingThreadPoolExecutor(),
              new BlockingExecutor(),
              fakeCalculator);
      guesser.setConfiguration(configuration);
      guesser.setJapaneseKeyboard(godanKana);

      List<TouchEvent> touchEventList =
          Collections.singletonList(
              TouchEvent.newBuilder()
                        .addStroke(
                            TouchPosition.newBuilder()
                                .setX(startX)
                                .setY(startY)
                                .setAction(TouchAction.TOUCH_DOWN))
                        .addStroke(
                            TouchPosition.newBuilder()
                                .setX(startX + deltaX)
                                .setY(startY + deltaY)
                                .setAction(TouchAction.TOUCH_UP))
                        .build());

      MoreAsserts.assertContentsInAnyOrder(
          testData.toString(),
          guesser.getProbableKeyEvents(touchEventList),
          testData.expectation.toArray());
    }
  }

  // TODO(matsuzakit): Move to JapaneseKeyboardTest.
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
        getFormattedKeyboardName(godanKana, configuration), new SparseIntArray());
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
}
