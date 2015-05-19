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
import org.mozc.android.inputmethod.japanese.KeyboardSpecificationName;
import org.mozc.android.inputmethod.japanese.MozcLog;
import org.mozc.android.inputmethod.japanese.MozcUtil;
import org.mozc.android.inputmethod.japanese.keyboard.Flick.Direction;
import org.mozc.android.inputmethod.japanese.keyboard.KeyState.MetaState;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchAction;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchEvent;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchPosition;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.KeyEvent.ProbableKeyEvent;
import org.mozc.android.inputmethod.japanese.util.LeastRecentlyUsedCacheMap;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

import android.content.res.AssetManager;
import android.content.res.Configuration;
import android.os.Handler;
import android.os.Looper;
import android.util.FloatMath;
import android.util.SparseArray;
import android.util.SparseIntArray;

import java.io.DataInputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.Executor;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;

import javax.annotation.Nullable;

/**
 * An object which guesses probable key events for typing correction feature.
 *
 * <p>We can use following values as touch event statistics, which are store in the file system.
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
   * Accessor for stats files.
   *
   * Extracted as an interface for testing.
   */
  @VisibleForTesting
  interface StatsFileAccessor {
    InputStream openStream(JapaneseKeyboard japaneseKeyboard, Configuration configuration)
        throws IOException;
  }

  /**
   * Concrete implementation of StatsFileAccessor, using AssetManager.
   */
  @VisibleForTesting
  static final class StatsFileAccessorImpl implements StatsFileAccessor {

    private final AssetManager assetManager;
    private final List<String> assetFileNames;

    StatsFileAccessorImpl(AssetManager assetManager) {
      this.assetManager = assetManager;
      this.assetFileNames = getAssetFileNameList(assetManager);
    }

    private static List<String> getAssetFileNameList(AssetManager assetManager) {
      try {
        return Collections.unmodifiableList(Arrays.asList(assetManager.list("")));
      } catch (IOException e) {
        MozcLog.e("Accessing asset is failed.");
      }
      return Collections.emptyList();
    }

    @Override
    public InputStream openStream(JapaneseKeyboard japaneseKeyboard, Configuration configuration)
        throws IOException {
      String baseName =
          japaneseKeyboard.getSpecification().getKeyboardSpecificationName().baseName;
      String orientation = KeyboardSpecificationName.getDeviceOrientationString(configuration);
      String fileName = String.format("%s_%s.touch_stats", baseName, orientation);
      if (fileName.indexOf(File.separator) != -1) {
        throw new IllegalArgumentException("fileName shouldn't include separator.");
      }
      for (String file : assetFileNames) {
        if (file.equals(fileName)) {
          return assetManager.open(fileName);
        }
      }
      throw new IOException(String.format("%s is not found.", fileName));
    }
  }

  /**
   * Calculator of likelihood.
   *
   * <p>Extracted as an interface for testing.
   */
  @VisibleForTesting
  static interface LikelihoodCalculator {

    public double getLikelihood(float firstX, float firstY, float deltaX, float deltaY,
        float[] probableEvent);
  }

  /**
   * Concrete implementation of LikelihoodCalculator.
   */
  @VisibleForTesting
  static final class LikelihoodCalculatorImpl implements LikelihoodCalculator {

    @Override
    public double getLikelihood(float firstX, float firstY, float deltaX, float deltaY,
        float[] probableEvent) {
      float sdx = firstX - probableEvent[START_X_AVG];
      float sdy = firstY - probableEvent[START_Y_AVG];
      // Keys that are too far away from user's touch-down position
      // are not considered as possibilities
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

  /**
   * A Runnable to load a stats file and calls back a {@link UpdateStatsListener} with loaded stats.
   *
   * <p>File access itself and updating the stats (via UpdateStatsListener) are
   * done on the caller's thread (so worker thread can be applicable).
   * So if you want to run the process asyncnously,
   * <ul>
   * <li>Run this on a worker thread.
   * <li>Make UpdateStatsListener thread safe or make it run on the UI thread.
   * </ul>
   */
  @VisibleForTesting
  static final class StatisticsLoader implements Runnable {

    @VisibleForTesting
    interface UpdateStatsListener {
      void updateStats(String formattedKeyboardName, SparseArray<float[]> stats);
    }

    private final JapaneseKeyboard japaneseKeyboard;
    private final Configuration configuration;
    private final StatsFileAccessor statsFileAccessor;
    private final UpdateStatsListener updateStatsListener;

    /**
     * @param statsFileAccessor an accessor for stats files. Must be non-null.
     * @param japaneseKeyboard a {@link JapaneseKeyboard} to specify the file to be loaded.
     * @param configuration a {@link Configuration} to specify the file to be loaded
     * @param formattedKeyboardNameToStats a {@link Map} to be updated. Must be non-null.
     * @param updateStatsExecutor an Executor on which the result is propagated
     */
    private StatisticsLoader(
        StatsFileAccessor statsFileAccessor,
        JapaneseKeyboard japaneseKeyboard,
        Configuration configuration,
        final Map<String, SparseArray<float[]>> formattedKeyboardNameToStats,
        final Executor updateStatsExecutor) {
      this(
          statsFileAccessor,
          japaneseKeyboard,
          configuration,
          formattedKeyboardNameToStats,
          new UpdateStatsListener() {
            @Override
            public void updateStats(final String formattedKeyboardName,
                                    final SparseArray<float[]> stats) {
              updateStatsExecutor.execute(new Runnable() {
                @Override
                public void run() {
                  formattedKeyboardNameToStats.put(formattedKeyboardName, stats);
                }
              });
            }
          });
    }

    // For testing purpose.
    @VisibleForTesting
    StatisticsLoader(StatsFileAccessor statsFileAccessor,
        JapaneseKeyboard japaneseKeyboard,
        Configuration configuration,
        Map<String, SparseArray<float[]>> formattedKeyboardNameToStats,
        UpdateStatsListener updateStatsListener) {
      this.statsFileAccessor = Preconditions.checkNotNull(statsFileAccessor);
      this.japaneseKeyboard = Preconditions.checkNotNull(japaneseKeyboard);
      this.configuration = Preconditions.checkNotNull(configuration);
      this.updateStatsListener = Preconditions.checkNotNull(updateStatsListener);
    }

    /**
     * Reads an {@link InputStream} and updates given {@link SparseArray}.
     *
     * @param stream an {@link InputStream} to be read
     * @param statsToUpdate a {@link SparseArray} which maps source_id to the values.
     *                      Expected to be empty.
     * @throws IOException when stream access fails
     */
    private void readStream(DataInputStream stream, SparseArray<float[]> statsToUpdate)
        throws IOException {
      int length = stream.readInt();
      for (int i = 0; i < length; ++i) {
        int sourceId = stream.readInt();
        float[] stats = new float[9];
        for (int j = 0; j < 8; ++j) {
          stats[j] = stream.readFloat();
        }
        stats[PRECALCULATED_DENOMINATOR] =
            FloatMath.sqrt(stats[START_X_VAR] * stats[START_Y_VAR]
                           * stats[DELTA_X_VAR] * stats[DELTA_Y_VAR]);
        statsToUpdate.put(sourceId, stats);
      }
    }

    @Override
    public void run() {
      SparseArray<float[]> result = new SparseArray<float[]>(MAX_KEY_NUMBER_IN_KEYBOARD);
      InputStream inputStream = null;
      try {
        inputStream = statsFileAccessor.openStream(japaneseKeyboard, configuration);
        readStream(new DataInputStream(inputStream), result);
      } catch (IOException e) {
        MozcLog.d("Stream access fails.", e);
        return;
      } finally {
        try {
          if (inputStream != null) {
            MozcUtil.close(inputStream, true);
          }
        } catch (IOException e) {
          // Never reach here.
        }
      }
      // Successfully loaded the file so call back updateStatsListener.
      String formattedKeyboardName = japaneseKeyboard.getSpecification()
          .getKeyboardSpecificationName().formattedKeyboardName(configuration);
      updateStatsListener.updateStats(formattedKeyboardName, result);
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
  private static final int START_X_AVG = 0;
  private static final int START_Y_AVG = 1;
  private static final int START_X_VAR = 2;
  private static final int START_Y_VAR = 3;
  private static final int DELTA_X_AVG = 4;
  private static final int DELTA_Y_AVG = 5;
  private static final int DELTA_X_VAR = 6;
  private static final int DELTA_Y_VAR = 7;
  // Pre-calculated value for the probability calculation.
  private static final int PRECALCULATED_DENOMINATOR = 8;

  // Threshold of likelihood. Such probable events of which likelihood is under the threshold
  // are not sent to the server.
  private static final double LIKELIHOOD_THRESHOLD = 1E-30;

  // Threshold of start position. Far probable events than the threshold are not sent.
  private static final double START_POSITION_THRESHOLD = 0.25;

  // Loaded stats are cached. This value is the size of the LRU cache.
  private static final int MAX_LRU_CACHE_CAPACITY = 6;

  // Estimated maximum number of keys in a keyboard.
  // Currently 64 is enough because even in mobile-qwerty keyboard has less than 40 keys.
  // Even if there are more keys than this, nothing will happen except for (very) small
  // performance change.
  private static final int MAX_KEY_NUMBER_IN_KEYBOARD = 64;

  // LRU cache of the stats.
  // formattedKeyboardName -> souce_id -> statistic values.
  private final Map<String, SparseArray<float[]>> formattedKeyboardNameToStats =
      new LeastRecentlyUsedCacheMap<String, SparseArray<float[]>>(MAX_LRU_CACHE_CAPACITY);

  // LRU cache of mapping table.
  // formattedKeyboardName -> souce_id -> keyCode.
  @VisibleForTesting final Map<String, SparseIntArray> formattedKeyboardNameToKeycodeMapper =
      new LeastRecentlyUsedCacheMap<String, SparseIntArray>(MAX_LRU_CACHE_CAPACITY);

  // StatsFileAccessor to access the files under assets/ directory.
  private final StatsFileAccessor statsFileAccessor;

  // An Executor to load the statistic files asynchronously.
  private final ThreadPoolExecutor dataLoadExecutor;

  // An Executor to propagate loaded statistics data.
  // Invoked from the thread which dataLoadExecutor uses.
  private final Executor dataPropagationExecutor;

  // Current JapaneseKeyboard.
  private Optional<JapaneseKeyboard> japaneseKeyboard = Optional.absent();

  // Current Configuration.
  private Optional<Configuration> configuration = Optional.absent();

  // Formatted keyboard name.
  // This can be calculated from japaneseKeyboard and configuration so is derivative variable.
  // Just for cache.
  private Optional<String> formattedKeyboardName = Optional.absent();

  // Threshold of very small likelihood.
  private final double likelihoodThreshold;

  // A Calculator of likelihood.
  private final LikelihoodCalculator likelihoodCalculator;

  /**
   * @param assetManager an AssertManager to access the stats files. Must be non-null.
   */
  public ProbableKeyEventGuesser(final AssetManager assetManager) {
    Preconditions.checkNotNull(assetManager);
    this.statsFileAccessor = new StatsFileAccessorImpl(assetManager);
    this.likelihoodThreshold = LIKELIHOOD_THRESHOLD;

    // Execute file loading on another thread.
    // Typically once commonly used keyboard's stats are loaded,
    // file access will not be done any more typically.
    // Thus corePoolSize = 0.
    this.dataLoadExecutor =
        new ThreadPoolExecutor(0, 1, 60L, TimeUnit.SECONDS, new ArrayBlockingQueue<Runnable>(1));

    // Execute propagation of read stats data on current thread.
    // By this we can avoid from synchronized keywords.
    this.dataPropagationExecutor = new Executor() {
      private final Handler mainloopHandler = new Handler(Looper.getMainLooper());
      @Override
      public void execute(Runnable command) {
        mainloopHandler.post(command);
      }
    };

    this.likelihoodCalculator = new LikelihoodCalculatorImpl();
  }

  @VisibleForTesting
  ProbableKeyEventGuesser(StatsFileAccessor statsFileAccessor,
                          double likelyhoodThreshold,
                          ThreadPoolExecutor dataLoadExecutor,
                          Executor dataPropagationExecutor,
                          LikelihoodCalculator likelihoodCalculator) {
    this.statsFileAccessor = Preconditions.checkNotNull(statsFileAccessor);
    this.likelihoodThreshold = likelyhoodThreshold;
    this.dataLoadExecutor = Preconditions.checkNotNull(dataLoadExecutor);
    this.dataPropagationExecutor = Preconditions.checkNotNull(dataPropagationExecutor);
    this.likelihoodCalculator = Preconditions.checkNotNull(likelihoodCalculator);
  }

  /**
   * Sets a {@link JapaneseKeyboard}.
   *
   * This invocation might load a stats data file (typically) asynchronously.
   * Before the loading has finished
   * {@link #getProbableKeyEvents(List)} cannot return any probable key events.
   * @see #getProbableKeyEvents(List)
   * @param japaneseKeyboard a {@link JapaneseKeyboard} to be set.
   * If null {@link #getProbableKeyEvents(List)} will return null.
   */
  public void setJapaneseKeyboard(@Nullable JapaneseKeyboard japaneseKeyboard) {
    this.japaneseKeyboard = Optional.fromNullable(japaneseKeyboard);
    updateFormattedKeyboardName();
    maybeUpdateEventStatistics();
  }

  /**
   * Sets a {@link Configuration}.
   *
   * Behaves almost the same as {@link #setJapaneseKeyboard(JapaneseKeyboard)}.
   * @param configuration a {@link Configuration} to be set.
   */
  public void setConfiguration(@Nullable Configuration configuration) {
    this.configuration = Optional.fromNullable(configuration);
    updateFormattedKeyboardName();
    maybeUpdateEventStatistics();
  }

  /**
   * Updates (cached) {@link #formattedKeyboardName}.
   */
  private void updateFormattedKeyboardName() {
    if (!japaneseKeyboard.isPresent() || !configuration.isPresent()) {
      formattedKeyboardName = Optional.absent();
      return;
    }
    formattedKeyboardName = Optional.of(japaneseKeyboard.get()
                                                        .getSpecification()
                                                        .getKeyboardSpecificationName()
                                                        .formattedKeyboardName(
                                                            configuration.get()));
  }

  /**
   * If stats for current JapaneseKeyboard and Configuration has not been loaded,
   * load and update it asynchronously.
   *
   * Loading queue contains only the latest task.
   * Older ones are discarded.
   */
  private void maybeUpdateEventStatistics() {
    if (!formattedKeyboardName.isPresent()
        || !japaneseKeyboard.isPresent()
        || !configuration.isPresent()) {
      return;
    }
    if (!formattedKeyboardNameToStats.containsKey(formattedKeyboardName.get())) {
      for (Runnable runnable : dataLoadExecutor.getQueue()) {
        dataLoadExecutor.remove(runnable);
      }
      dataLoadExecutor.execute(
          new StatisticsLoader(
              statsFileAccessor, japaneseKeyboard.get(), configuration.get(),
              formattedKeyboardNameToStats,
              dataPropagationExecutor));
    }
  }

  /**
   * Calculates probable key events for given {@code touchEventList}.
   *
   * A List is returned (null will not be returned).
   * If corresponding stats data has not been loaded yet (file access is done asynchronously),
   * empty list is returned.
   *
   * TODO(matsuzakit): Change the caller side which expects null-return-value,
   *                   before submitting this CL.
   *
   * @param touchEventList a List of TouchEvents. If null, empty List is returned.
   */
  public List<ProbableKeyEvent> getProbableKeyEvents(
      @Nullable List<? extends TouchEvent> touchEventList) {
    // This method's responsibility is to pre-check the condition.
    // Calculation itself is done in getProbableKeyEventsInternal method.
    if (!japaneseKeyboard.isPresent() || !configuration.isPresent()) {
      // Keyboard has not been set up.
      return Collections.emptyList();
    }
    if (touchEventList == null || touchEventList.size() != 1) {
      // If null || size == 0 , we can do nothing.
      // If size >= 2, this is special situation (saturating touch event) so do nothing.
      return Collections.emptyList();
    }
    SparseArray<float[]> eventStatistics = formattedKeyboardNameToStats.get(
        formattedKeyboardName.get());
    if (eventStatistics == null) {
      // No corresponding stats is available. Returning null.
      // The stats we need might be pushed out from the LRU cache because of bulk-updates of
      // JapaneseKeyboard or Configuration.
      // For such case we invoke maybeUpdateEventStatistics method to load the stats.
      // This is very rare (and usually not happen) but just in case.
      maybeUpdateEventStatistics();
      return Collections.emptyList();
    }
    TouchEvent event = touchEventList.get(0);
    if (event.getStrokeCount() < 2) {
      // TOUCH_DOWN and TOUCH_UP should be included so at least two strokes should be in the event.
      return Collections.emptyList();
    }
    TouchPosition firstPosition = event.getStroke(0);
    TouchPosition lastPosition = event.getStroke(event.getStrokeCount() - 1);
    // The sequence of TouchPositions must start with TOUCH_DOWN action and
    // end with TOUCH_UP action.
    // Otherwise the sequence is special case so we do nothing.
    if (firstPosition.getAction() != TouchAction.TOUCH_DOWN
        || lastPosition.getAction() != TouchAction.TOUCH_UP) {
      return Collections.emptyList();
    }
    float firstX = firstPosition.getX();
    float firstY = firstPosition.getY();
    float deltaX = lastPosition.getX() - firstX;
    float deltaY = lastPosition.getY() - firstY;

    // Calculates a map, keyCode -> likelihood.
    SparseArray<Double> likelihoodArray =
        getLikelihoodArray(eventStatistics, firstX, firstY, deltaX, deltaY);
    double sumLikelihood = 0;
    for (int i = 0; i < likelihoodArray.size(); ++i) {
      sumLikelihood += likelihoodArray.valueAt(i);
    }
    // If no probable events are available or something wrong happened on the calculation,
    // return empty list.
    if (sumLikelihood <= likelihoodThreshold || Double.isNaN(sumLikelihood)) {
      return Collections.emptyList();
    }
    // Construct the result list, converting from likelihood to probability.
    List<ProbableKeyEvent> result = new ArrayList<ProbableKeyEvent>(likelihoodArray.size());
    for (int i = 0; i < likelihoodArray.size(); ++i) {
      int keyCode = likelihoodArray.keyAt(i);
      double likelihood = likelihoodArray.valueAt(i);
      ProbableKeyEvent probableKeyEvent = ProbableKeyEvent.newBuilder()
          .setProbability(likelihood / sumLikelihood)
          .setKeyCode(keyCode)
          .build();
      result.add(probableKeyEvent);
    }
    return result;
  }

  /**
   * Returns a {@link SparseArray} mapping from souce_id to keyCode.
   *
   * Cached one is used if what we want is already cached.
   *
   * TODO(matsuzakit): Move this method to JapaneseKeyboard.
   */
  SparseIntArray getKeycodeMapper() {
    SparseIntArray result =
        formattedKeyboardNameToKeycodeMapper.get(formattedKeyboardName.get());
    if (result != null) {
      return result;
    }
    result = new SparseIntArray(MAX_KEY_NUMBER_IN_KEYBOARD);
    for (Row row : japaneseKeyboard.get().getRowList()) {
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
    formattedKeyboardNameToKeycodeMapper.put(formattedKeyboardName.get(), result);
    return result;
  }

  /**
   * Returns a SparseArray which maps from keyCode to likelihood.
   *
   * @return a SparseArray (non-null). The size might be 0. Its values are not NaN.
   */
  private SparseArray<Double> getLikelihoodArray(SparseArray<float[]> eventStatistics,
                                                 float firstX, float firstY,
                                                 float deltaX, float deltaY) {
    SparseArray<Double> result = new SparseArray<Double>(eventStatistics.size());
    SparseIntArray keycodeMapper = getKeycodeMapper();
    for (int i = 0; i < eventStatistics.size(); ++i) {
      int sourceId = eventStatistics.keyAt(i);
      Integer keyCode = keycodeMapper.get(sourceId);
      // Special key or non-existent-key.
      // Don't produce probable key events.
      if (keyCode == null || keyCode <= 0) {
        continue;
      }
      double likelihood =
          likelihoodCalculator.getLikelihood(
              firstX, firstY, deltaX, deltaY, eventStatistics.get(sourceId));
      // Filter out too small likelihood and invalid value.
      if (likelihood <= likelihoodThreshold || Double.isNaN(likelihood)) {
        continue;
      }
      result.put(keyCode, likelihood);
    }
    return result;
  }
}
