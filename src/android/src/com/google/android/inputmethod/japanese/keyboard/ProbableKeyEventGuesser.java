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

import org.mozc.android.inputmethod.japanese.JapaneseKeyboard;
import org.mozc.android.inputmethod.japanese.KeyboardSpecificationName;
import org.mozc.android.inputmethod.japanese.MozcLog;
import org.mozc.android.inputmethod.japanese.MozcUtil;
import org.mozc.android.inputmethod.japanese.keyboard.Flick.Direction;
import org.mozc.android.inputmethod.japanese.keyboard.KeyState.MetaState;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchAction;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchEvent;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchPosition;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.KeyEvent.ProbableKeyEvent;
import org.mozc.android.inputmethod.japanese.util.LeastRecentlyUsedCacheMap;

import android.content.res.AssetManager;
import android.content.res.Configuration;
import android.os.Handler;
import android.os.Looper;
import android.util.FloatMath;
import android.util.SparseArray;

import java.io.DataInputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;

/**
 * Guesses probable key events for typing correction feature.
 *
 * We can use following values as touch event statistics, which are store in the file system.
 * <ul>
 * <li>Average of start position (X and Y).
 * <li>Variance of start position (X and Y).
 * <li>Average of delta (X and Y).
 * <li>Variance of delta (X and Y).
 * </ul>
 * We use probability distribution of 4-dimentional Gaussian distribution
 * as likelihood function.
 * We cannot use covariance so we assume that each elements are orthogonal (covariance is 0).
 *
 */
public class ProbableKeyEventGuesser {

  /**
   * A Runnable to load a stats file and update a Map which maps formattedKeyboardName to stats
   * values, asynchronously.
   *
   * File access itself is done on the caller's thread (so worker thread can be applicable)
   * but updating the map is done on the main thread via {@link Handler}.
   * So the map doesn't have to be guarded by 'synchronized' like lock mechanism.
   */
  static class StaticticsLoader implements Runnable {
    private static final Handler MAINLOOP_HANDLER = new Handler(Looper.getMainLooper());

    private final JapaneseKeyboard japaneseKeyboard;
    private final Configuration configuration;
    private final AssetManager assetManager;
    final Map<String, SparseArray<float[]>> formattedKeyboardNameToStats;

    // File names in the root asset directory.
    // AssetManager#list is very slow on some specific devices so this cache is needed.
    // Fortunately asset files are frozen (never modified) so this cache will never be updated
    // after instantiation.
    private static volatile String[] assetListCache;

    /**
     * @param assetManager an asset manager to load a file in assets/ directory. Must be non-null.
     * @param japaneseKeyboard a {@link JapaneseKeyboard} to specify the file to be loaded.
     *                         Can be null (nothing will be done).
     * @param configuration a {@link Configuration} to specify the file to be loaded
     *                      Can be null (nothing will be done).
     * @param formattedKeyboardNameToStats a {@link Map} to be updated. Must be non-null.
     */
    StaticticsLoader(AssetManager assetManager,
                     JapaneseKeyboard japaneseKeyboard,
                     Configuration configuration,
                     Map<String, SparseArray<float[]>> formattedKeyboardNameToStats) {
      if (assetManager == null) {
        throw new NullPointerException("assetManager must be non-null.");
      }
      if (formattedKeyboardNameToStats == null) {
        throw new NullPointerException("formattedKeyboardNameToStats must be non-null.");
      }
      // japaneseKeyboard and configuration can be null. Nothing will be loaded.
      this.assetManager = assetManager;
      this.japaneseKeyboard = japaneseKeyboard;
      this.configuration = configuration;
      this.formattedKeyboardNameToStats = formattedKeyboardNameToStats;
    }

    String[] getAssetList() throws IOException {
      if (assetListCache == null) {
        synchronized (StaticticsLoader.class) {
          if (assetListCache == null) {
            assetListCache = assetManager.list("");
          }
        }
      }
      return assetListCache;
    }

    boolean isAssetFileAvailable(String fileName) {
      if (fileName.indexOf(File.separator) != -1) {
        throw new IllegalArgumentException("fileName shouldn't include separator.");
      }
      try {
        for (String file : getAssetList()) {
          if (file.equals(fileName)) {
            return true;
          }
        }
      } catch (IOException e) {
        // Do nothing.
      }
      return false;
    }

    InputStream openAssetStream(String fileName) throws IOException {
      return assetManager.open(fileName);
    }

    /**
     * @return an {@link InputStream} of the stats file we will be loading.
     */
    InputStream getStream() {
      String baseName = japaneseKeyboard.getSpecification().getKeyboardSpecificationName().baseName;
      String orientation = KeyboardSpecificationName.getDeviceOrientationString(configuration);
      String fileName = String.format("%s_%s.touch_stats", baseName, orientation);
      if (!isAssetFileAvailable(fileName)) {
        // No corresponding file is found.
        return null;
      }
      try {
        return openAssetStream(fileName);
      } catch (IOException e) {
        MozcLog.e(new StringBuilder(fileName).append(" is not accessible.").toString(), e);
        return null;
      }
    }

    /**
     * Reads an {@link InputStream} and updates given {@link SparseArray}.
     *
     * @param stream an {@link InputStream} to be read
     * @param statsToUpdate a {@link SparseArray} which maps source_id to the values.
     *                      Expected to be empty.
     * @throws IOException when stream access fails
     */
    void readStream(DataInputStream stream, SparseArray<float[]> statsToUpdate) throws IOException {
      int length = stream.readInt();
      for (int i = 0; i < length; ++i) {
        int sourceId = stream.readInt();
        float[] stats = new float[9];
        for (int j = 0; j < 8; ++j) {
          stats[j] = stream.readFloat();
        }
        stats[PRECALCULATED_DENOMINATOR] = getPrecalculatedDenominator(stats);
        statsToUpdate.put(sourceId, stats);
      }
    }

    /**
     * Gets pre-caluculated denominator.
     */
    static float getPrecalculatedDenominator(float[] stats) {
      return FloatMath.sqrt(stats[START_X_VAR] * stats[START_Y_VAR]
                            * stats[DELTA_X_VAR] * stats[DELTA_Y_VAR]);
    }

    @Override
    public void run() {
      // If new japaneseKeyboard or new configuration is null, guesser cannot do anything.
      if (japaneseKeyboard == null || configuration == null) {
        return;
      }
      // 64 is heuristics.
      // There will not be such keyboards which have more keys than 64.
      SparseArray<float[]> result = new SparseArray<float[]>(64);
      InputStream inputStream = getStream();
      if (inputStream == null) {
        return;
      }
      try {
        readStream(new DataInputStream(inputStream), result);
      } catch (IOException e) {
        MozcLog.w("Stream access fails.", e);
        return;
      } finally {
        try {
          MozcUtil.close(inputStream, true);
        } catch (IOException e) {
          // Never reach here.
        }
      }
      // Successfully loaded the file so update given Map.
      updateStats(result);
    }

    /**
     * Updates the map on the main thread.
     */
    void updateStats(final SparseArray<float[]> result) {
      getMainLooperHandler().post(createUpdateRunnable(result));
    }

    Handler getMainLooperHandler() {
      return MAINLOOP_HANDLER;
    }

    /**
     * Gets a Runnable to update mapToBeUpdated.
     *
     * The returned Runnable is intended to be run on the main looper.
     * @param result a SparseArray to be added to the map.
     */
    Runnable createUpdateRunnable(final SparseArray<float[]> result) {
      return new Runnable() {
        @Override
        public void run() {
          String formattedKeyboardName = japaneseKeyboard.getSpecification()
              .getKeyboardSpecificationName().formattedKeyboardName(configuration);
          formattedKeyboardNameToStats.put(formattedKeyboardName, result);
        }
      };
    }
  }

  // Tags for stats data.
  // The file format is
  // size : int
  // repeat for #size {
  //   source_id : int
  //   values : float[8]
  // }
  // The values should be accessed via below tags.
  static final int START_X_AVG = 0;
  static final int START_Y_AVG = 1;
  static final int START_X_VAR = 2;
  static final int START_Y_VAR = 3;
  static final int DELTA_X_AVG = 4;
  static final int DELTA_Y_AVG = 5;
  static final int DELTA_X_VAR = 6;
  static final int DELTA_Y_VAR = 7;
  // Pre-calculated value for the probability calculation.
  static final int PRECALCULATED_DENOMINATOR = 8;

  // Threshold of likelihood. Such probable events of which likelihood is under the threshold
  // are not sent to the server.
  static final double LIKELIHOOD_THRESHOLD = 1E-30;

  // Threshold of start position. Far probable events than the threshold are not sent.
  static final double START_POSITION_THRESHOLD = 0.25;

  // Loaded stats are cached. This value is the size of the LRU cache.
  private static final int MAX_LRU_CACHE_CAPACITY = 6;

  // LRU cache of the stats.
  // formattedKeyboardName -> souce_id -> statistic values.
  final Map<String, SparseArray<float[]>> formattedKeyboardNameToStats =
      new LeastRecentlyUsedCacheMap<String, SparseArray<float[]>>(MAX_LRU_CACHE_CAPACITY);

  // LRU cache of mapping table.
  // formattedKeyboardName -> souce_id -> keyCode.
  final Map<String, SparseArray<Integer>> formattedKeyboardNameToKeycodeMapper =
      new LeastRecentlyUsedCacheMap<String, SparseArray<Integer>>(MAX_LRU_CACHE_CAPACITY);

  // AssetManager to access the files under assets/ directory.
  private final AssetManager assetManager;

  // ExecutorService to load the statistic files asynchronously.
  private final ThreadPoolExecutor executorService;

  // Current JapaneseKeyboard.
  JapaneseKeyboard japaneseKeyboard;

  // Current Configuration.
  Configuration configuration;

  // Formatted keyboard name.
  // This can be calculated from japaneseKeyboard and configuration so is derivative variable.
  // Just for cache.
  String formattedKeyboardName;

  /**
   * @param assetManager an AssertManager to access the stats files. Must be non-null.
   */
  public ProbableKeyEventGuesser(AssetManager assetManager) {
    // Typically once commonly used keyboard's stats are loaded,
    // file access will not be done any more typically.
    // Thus corePoolSize = 0.
    this(assetManager,
         new ThreadPoolExecutor(0, 1, 60L, TimeUnit.SECONDS, new ArrayBlockingQueue<Runnable>(1)));
  }

  // For testing
  ProbableKeyEventGuesser(AssetManager assetManager, ThreadPoolExecutor executorService) {
    if (assetManager == null) {
      throw new NullPointerException("AssertManager must be non-null.");
    }
    this.assetManager = assetManager;
    this.executorService = executorService;
  }

  /**
   * Sets a {@link JapaneseKeyboard}.
   *
   * This invocation might load a stats data file asynchronously.
   * Before the loading has finished
   * {@link #getProbableKeyEvents(List)} cannot return any probable key events.
   * @see #getProbableKeyEvents(List)
   * @param japaneseKeyboard a {@link JapaneseKeyboard} to be set.
   * If null {@link #getProbableKeyEvents(List)} will return null.
   */
  public void setJapaneseKeyboard(JapaneseKeyboard japaneseKeyboard) {
    this.japaneseKeyboard = japaneseKeyboard;
    updateFormattedKeyboardName();
    maybeUpdateEventStatistics();
  }

  /**
   * Sets a {@link Configuration}.
   *
   * Behaves almost the same as {@link #setJapaneseKeyboard(JapaneseKeyboard)}.
   * @param configuration a {@link Configuration} to be set.
   */
  public void setConfiguration(Configuration configuration) {
    this.configuration = configuration;
    updateFormattedKeyboardName();
    maybeUpdateEventStatistics();
  }

  /**
   * Updates (cached) {@link #formattedKeyboardName}.
   */
  void updateFormattedKeyboardName() {
    if (japaneseKeyboard == null || configuration == null) {
      formattedKeyboardName = null;
      return;
    }
    formattedKeyboardName = japaneseKeyboard.getSpecification()
                                            .getKeyboardSpecificationName()
                                            .formattedKeyboardName(configuration);
  }

  /**
   * If stats for current JapaneseKeyboard and Configuration has not been loaded,
   * load and update it asynchronously.
   *
   * Loading queue contains only the latest task.
   * Older ones are discarded.
   */
  void maybeUpdateEventStatistics() {
    if (!formattedKeyboardNameToStats.containsKey(formattedKeyboardName)) {
      for (Runnable runnable : executorService.getQueue()) {
        executorService.remove(runnable);
      }
      executorService.execute(
          new StaticticsLoader(
              assetManager, japaneseKeyboard, configuration, formattedKeyboardNameToStats));
    }
  }

  /**
   * Calculates probable key events for given {@code touchEventList}.
   *
   * Null or non-empty List is returned (empty List will not be returned).
   * If corresponding stats data has not been loaded yet (file access is done asynchronously),
   * null is returned.
   *
   * @param touchEventList a List of TouchEvents. If null, null is returned.
   */
  public List<ProbableKeyEvent> getProbableKeyEvents(List<? extends TouchEvent> touchEventList) {
    // This method's responsibility is to pre-check the condition.
    // Calculation itself is done in getProbableKeyEventsInternal method.
    if (japaneseKeyboard == null || configuration == null) {
      // Keyboard has not been set up.
      return null;
    }
    if (touchEventList == null || touchEventList.size() != 1) {
      // If null || size == 0 , we can do nothing.
      // If size >= 2, this is special situation (saturating touch event) so do nothing.
      return null;
    }
    SparseArray<float[]> eventStatistics = formattedKeyboardNameToStats.get(formattedKeyboardName);
    if (eventStatistics == null) {
      // No correspoinding stats is available. Returning null.
      // The stats we need might be pushed out from the LRU cache because of bulk-updates of
      // JapaneseKeyboard or Configuration.
      // For such case we invoke maybeUpdateEventStatistics method to load the stats.
      // This is very rare (and usually not happen) but just in case.
      maybeUpdateEventStatistics();
      return null;
    }
    TouchEvent event = touchEventList.get(0);
    TouchPosition firstPosition = event.getStroke(0);
    TouchPosition lastPosition = event.getStroke(event.getStrokeCount() - 1);
    // The sequence of TouchPositions must start with TOUCH_DOWN action and
    // end with TOUCH_UP action.
    // Otherwise the sequence is special case so we do nothing.
    if (firstPosition.getAction() != TouchAction.TOUCH_DOWN
        || lastPosition.getAction() != TouchAction.TOUCH_UP) {
      return null;
    }
    float firstX = firstPosition.getX();
    float firstY = firstPosition.getY();
    float deltaX = lastPosition.getX() - firstX;
    float deltaY = lastPosition.getY() - firstY;

    return getProbableKeyEventsInternal(eventStatistics, firstX, firstY, deltaX, deltaY);
  }

  /**
   * Returns calculated {@link ProbableKeyEvent} list.
   */
  List<ProbableKeyEvent> getProbableKeyEventsInternal(SparseArray<float[]> eventStatistics,
      float firstX, float firstY, float deltaX, float deltaY) {
    // Calculates a map, keyCode -> likelihood.
    SparseArray<Double> likelihoodArray =
        getLikelihoodArray(eventStatistics, firstX, firstY, deltaX, deltaY);
    double sumLikelihood = 0;
    for (int i = 0; i < likelihoodArray.size(); ++i) {
      sumLikelihood += likelihoodArray.valueAt(i);
    }
    // If no probable events are available or something wrong happened on the calculation,
    // return null.
    if (sumLikelihood == 0 || Double.isNaN(sumLikelihood)) {
      return null;
    }
    // Construct the result list, converting from likelihood to probability.
    List<ProbableKeyEvent> result =
        new ArrayList<ProtoCommands.KeyEvent.ProbableKeyEvent>(likelihoodArray.size());
    for (int i = 0; i < likelihoodArray.size(); ++i) {
      int keyCode = likelihoodArray.keyAt(i);
      double likelihood = likelihoodArray.valueAt(i);
      ProbableKeyEvent probableKeyEvent = ProbableKeyEvent.newBuilder()
          .setProbability(likelihood / sumLikelihood).setKeyCode(keyCode).build();
      result.add(probableKeyEvent);
    }
    return result;
  }

  /**
   * Returns a {@link SparseArray} mapping from souce_id to keyCode.
   *
   * Cached one is used if what we want is already cached.
   */
  SparseArray<Integer> getKeycodeMapper() {
    SparseArray<Integer> result =
        formattedKeyboardNameToKeycodeMapper.get(formattedKeyboardName);
    if (result != null) {
      return result;
    }
    result = new SparseArray<Integer>(64);  // 64 is heuristics.
    for (Row row : japaneseKeyboard.getRowList()) {
      for (Key key : row.getKeyList()) {
        for (MetaState metaState : MetaState.values()) {
          KeyState keyState = key.getKeyState(metaState);
          if (keyState == null) {
            continue;
          }
          for (Direction direction : Direction.values()) {
            Flick flick = keyState.getFlick(direction);
            if (flick != null) {
              KeyEntity keyEntity = flick.getKeyEntity();
              result.put(keyEntity.getSourceId(), keyEntity.getKeyCode());
            }
          }
        }
      }
    }
    formattedKeyboardNameToKeycodeMapper.put(formattedKeyboardName, result);
    return result;
  }

  /**
   * Returns a SparseArray which maps from keyCode to likelihood.
   *
   * @return a SparseArray (non-null). The size might be 0. Its values are not NaN.
   */
  SparseArray<Double> getLikelihoodArray(SparseArray<float[]> eventStatistics,
                                         float firstX, float firstY, float deltaX, float deltaY) {
    SparseArray<Double> result = new SparseArray<Double>(eventStatistics.size());
    SparseArray<Integer> keycodeMapper = getKeycodeMapper();
    for (int i = 0; i < eventStatistics.size(); ++i) {
      int sourceId = eventStatistics.keyAt(i);
      Integer keyCode = keycodeMapper.get(sourceId);
      // Special key or non-existent-key.
      // Don't produce probable key events.
      if (keyCode == null || keyCode <= 0) {
        continue;
      }
      double likelihood =
          getLikelihood(firstX, firstY, deltaX, deltaY, eventStatistics.get(sourceId));
      // Filter out too small likelihood and invalid value.
      if (likelihood < LIKELIHOOD_THRESHOLD || Double.isNaN(likelihood)) {
        continue;
      }
      result.put(keyCode, likelihood);
    }
    return result;
  }

  /**
   * Calculates a likehood value.
   */
  double getLikelihood(float firstX, float firstY, float deltaX, float deltaY,
                       float[] probableEvent) {
    float sdx = firstX - probableEvent[START_X_AVG];
    float sdy = firstY - probableEvent[START_Y_AVG];
    // Too far keys are ignored.
    if (Math.abs(sdx) > START_POSITION_THRESHOLD || Math.abs(sdy) > START_POSITION_THRESHOLD) {
      return 0;
    }
    double sdx2 = sdx * sdx;
    double sdy2 = sdy * sdy;

    float ddx = deltaX - probableEvent[DELTA_X_AVG];
    float ddy = deltaY - probableEvent[DELTA_Y_AVG];
    double ddx2 = ddx * ddx;
    double ddy2 = ddy * ddy;
    return Math.exp(-(
        sdx2 / (probableEvent[START_X_VAR])
        + sdy2 / (probableEvent[START_Y_VAR])
        + ddx2 / (probableEvent[DELTA_X_VAR])
        + ddy2 / (probableEvent[DELTA_X_VAR])) / 2d) / probableEvent[PRECALCULATED_DENOMINATOR];
  }
}
