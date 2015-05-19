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

package org.mozc.android.inputmethod.japanese;

import org.mozc.android.inputmethod.japanese.emoji.EmojiProviderType;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Request;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Request.CrossingEdgeBehavior;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Request.EmojiCarrierType;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Request.RewriterCapability;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Request.SpaceOnAlphanumeric;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Request.SpecialRomanjiTable;
import org.mozc.android.inputmethod.japanese.util.ResourcesWrapper;
import com.google.protobuf.ByteString;

import android.annotation.TargetApi;
import android.app.Dialog;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.Message;
import android.os.StrictMode;
import android.provider.Settings;
import android.telephony.TelephonyManager;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodInfo;
import android.view.inputmethod.InputMethodManager;

import java.io.Closeable;
import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.lang.reflect.InvocationTargetException;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.Collections;
import java.util.EnumMap;
import java.util.Map;
import java.util.zip.ZipFile;

/**
 * Utility class
 *
 */
public final class MozcUtil {
  private static class OutOfMemoryRetryingResources extends ResourcesWrapper {
    private final int retryCount;

    public OutOfMemoryRetryingResources(Resources base, int retryCount) {
      super(base);
      this.retryCount = retryCount;
    }

    @Override
    public Drawable getDrawable(int id) throws NotFoundException {
      Resources base = getBase();
      for (int i = 0; i < retryCount; ++i) {
        try {
          return base.getDrawable(id);
        } catch (OutOfMemoryError e) {
          // Retry with running gc.
          System.gc();
        }
      }

      // Final trial.
      return base.getDrawable(id);
    }

    @TargetApi(15)
    @Override
    public Drawable getDrawableForDensity(int id, int density) throws NotFoundException {
      Resources base = getBase();
      for (int i = 0; i < retryCount; ++i) {
        try {
          return base.getDrawableForDensity(id, density);
        } catch (OutOfMemoryError e) {
          // Retry with running gc.
          System.gc();
        }
      }

      // Final trial.
      return base.getDrawableForDensity(id, density);
    }
  }

  /**
   * Simple interface to use mock of TelephonyManager for testing purpose.
   */
  public interface TelephonyManagerInterface {
    public String getNetworkOperator();
  }

  /**
   * Real implementation of TelephonyManagerInterface.
   */
  private static class TelephonyManagerImpl implements TelephonyManagerInterface {
    private final TelephonyManager telephonyManager;
    TelephonyManagerImpl(TelephonyManager telephonyManager) {
      this.telephonyManager = telephonyManager;
    }

    @Override
    public String getNetworkOperator() {
      return telephonyManager.getNetworkOperator();
    }
  }

  static class InputMethodPickerShowingCallback implements Handler.Callback {
    @Override
    public boolean handleMessage(Message msg) {
      Context context = Context.class.cast(msg.obj);
      InputMethodManager.class.cast(
          context.getSystemService(Context.INPUT_METHOD_SERVICE)).showInputMethodPicker();
      return false;
    }
  }

  // Cursor position values.
  // According to the restriction of IMF,
  // the cursor position must be at the head or tail of precomposition.
  public static final int CURSOR_POSITION_HEAD = 0;
  public static final int CURSOR_POSITION_TAIL = 1;

  // Tag for logging.
  // This constant value affects only the logs printed by Java layer.
  // If you want to change the tag name, see also kProductPrefix in base/const.h.
  public static final String LOGTAG = "Mozc";

  private static final int OUT_OF_MEMORY_RETRY_COUNT = 5;

  private static Boolean isDebug = null;
  private static Boolean isDevChannel = null;
  private static Boolean isMozcEnabled = null;
  private static Boolean isMozcDefaultIme = null;

  private static final int SHOW_INPUT_METHOD_PICKER_WHAT = 0;
  private static final Handler showInputMethodPickerHandler =
      new Handler(new InputMethodPickerShowingCallback());

  private static final Map<EmojiProviderType, EmojiCarrierType> EMOJI_PROVIDER_TYPE_MAP;
  static {
    EnumMap<EmojiProviderType, EmojiCarrierType> map =
        new EnumMap<EmojiProviderType, EmojiCarrierType>(EmojiProviderType.class);
    map.put(EmojiProviderType.DOCOMO, EmojiCarrierType.DOCOMO_EMOJI);
    map.put(EmojiProviderType.SOFTBANK, EmojiCarrierType.SOFTBANK_EMOJI);
    map.put(EmojiProviderType.KDDI, EmojiCarrierType.KDDI_EMOJI);
    EMOJI_PROVIDER_TYPE_MAP = Collections.unmodifiableMap(map);
  }

  // Disallow instantiation.
  private MozcUtil() {
  }

  public static final boolean isDebug(Context context) {
    if (isDebug != null) {
      return isDebug;
    }

    PackageManager manager = context.getPackageManager();
    try {
      ApplicationInfo appInfo =
          manager.getApplicationInfo(context.getPackageName(), 0);
      return (appInfo.flags & ApplicationInfo.FLAG_DEBUGGABLE) != 0;
    } catch (NameNotFoundException e) {
      MozcLog.w("PackageManager#getApplicationInfo cannot find this application.");
      return false;
    }
  }

  /**
   * For testing purpose.
   * @param isDebug null if default behavior is preferable
   */
  public static final void setDebug(Boolean isDebug) {
    MozcUtil.isDebug = isDebug;
  }

  public static final boolean isDevChannel(Context context) {
    if (isDevChannel != null) {
      return isDevChannel;
    }

    try {
      return isDevChannelVersionName(getVersionName(context));
    } catch (NameNotFoundException e) {
      MozcLog.w("PackageManager#getApplicationInfo cannot find this application.");
      return false;
    }
  }

  public static String getVersionName(Context context) throws NameNotFoundException {
    PackageManager manager = context.getPackageManager();
    PackageInfo packageInfo = manager.getPackageInfo(context.getPackageName(), 0);
    return packageInfo.versionName;
  }

  /**
   * For testing purpose.
   * @param isDevChannel null if default behavior is preferable
   */
  public static final void setDevChannel(Boolean isDevChannel) {
    MozcUtil.isDevChannel = isDevChannel;
  }

  /**
   * Returns true if {@code versionName} indicates dev channel.
   *
   * This method sees only the characters after the last period.
   * If the characters is greater than or equal to (int)100,
   * this method returns true.
   */
  private static boolean isDevChannelVersionName(String versionName) {
    if (versionName == null) {
      return false;
    }
    int lastDot = versionName.lastIndexOf('.');
    if (lastDot < 0 || versionName.length() == lastDot + 1) {
      return false;
    }
    try {
      return Integer.parseInt(versionName.substring(lastDot + 1)) >= 100;
    } catch (NumberFormatException e) {
      return false;
    }
  }

  /**
   * @param context an application context. Shouldn't be {@code null}.
   * @return {@code true} if Mozc is enabled. Otherwise {@code false}.
   */
  public static boolean isMozcEnabled(Context context) {
    if (isMozcEnabled != null) {
      return isMozcEnabled;
    }

    InputMethodManager inputMethodManager =
        InputMethodManager.class.cast(context.getSystemService(Context.INPUT_METHOD_SERVICE));
    if (inputMethodManager == null) {
      MozcLog.w("InputMethodManager is not found.");
      return false;
    }
    String packageName = context.getPackageName();
    for (InputMethodInfo inputMethodInfo : inputMethodManager.getEnabledInputMethodList()) {
      if (inputMethodInfo.getServiceName().startsWith(packageName)) {
        return true;
      }
    }
    return false;
  }

  /**
   * Just injects the result of isMozcEnabled for testing purpose,
   * doesn't actually enable Mozc.
   */
  public static void setMozcEnabled(Boolean isMozcEnabled) {
    MozcUtil.isMozcEnabled = isMozcEnabled;
  }

  /**
   * @param context an application context. Shouldn't be {@code null}.
   * @return {@code true} if the default IME is Mozc. Otherwise {@code false}.
   */
  public static boolean isMozcDefaultIme(Context context) {
    if (isMozcDefaultIme != null) {
      return isMozcDefaultIme;
    }

    InputMethodInfo mozcInputMethodInfo = getMozcInputMethodInfo(context);
    if (mozcInputMethodInfo == null) {
      MozcLog.w("Mozc's InputMethodInfo is not found.");
      return false;
    }

    String currentIme = Settings.Secure.getString(
        context.getContentResolver(), Settings.Secure.DEFAULT_INPUT_METHOD);
    return mozcInputMethodInfo.getId().equals(currentIme);
  }

  /**
   * Just injects the result of isMozcDefaultIme for testing purpose,
   * doesn't actually set mozc as the default ime.
   */
  public static void setMozcDefaultIme(Boolean isMozcDefaultIme) {
    MozcUtil.isMozcDefaultIme = isMozcDefaultIme;
  }

  private static InputMethodInfo getMozcInputMethodInfo(Context context) {
    InputMethodManager inputMethodManager =
        InputMethodManager.class.cast(context.getSystemService(Context.INPUT_METHOD_SERVICE));
    if (inputMethodManager == null) {
      MozcLog.w("InputMethodManager is not found.");
      return null;
    }
    String packageName = context.getPackageName();
    for (InputMethodInfo inputMethodInfo : inputMethodManager.getInputMethodList()) {
      if (inputMethodInfo.getPackageName().equals(packageName)) {
        return inputMethodInfo;
      }
    }

    // Not found.
    return null;
  }

  public static boolean requestShowInputMethodPicker(Context context) {
    return showInputMethodPickerHandler.sendMessage(
        showInputMethodPickerHandler.obtainMessage(SHOW_INPUT_METHOD_PICKER_WHAT, context));
  }

  public static void cancelShowInputMethodPicker(Context context) {
    showInputMethodPickerHandler.removeMessages(SHOW_INPUT_METHOD_PICKER_WHAT, context);
  }

  public static boolean hasShowInputMethodPickerRequest(Context context) {
    return showInputMethodPickerHandler.hasMessages(SHOW_INPUT_METHOD_PICKER_WHAT, context);
  }

  /**
   * Returns the {@code TelephonyManagerInterface} corresponding to the given {@code context}.
   */
  public static TelephonyManagerInterface getTelephonyManager(Context context) {
    return new TelephonyManagerImpl(
        TelephonyManager.class.cast(context.getSystemService(Context.TELEPHONY_SERVICE)));
  }

  public static Context getContextWithOutOfMemoryRetrial(Context context) {
    return new ContextWrapper(context) {
      final Resources resources =
          new OutOfMemoryRetryingResources(super.getResources(), OUT_OF_MEMORY_RETRY_COUNT);

      @Override
      public Resources getResources() {
        return resources;
      }
    };
  }

  public static <T extends View> T inflateWithOutOfMemoryRetrial(
      Class<T> clazz, LayoutInflater inflater,
      int resourceId, ViewGroup root, boolean attachToRoot) {
    for (int i = 0; i < OUT_OF_MEMORY_RETRY_COUNT; ++i) {
      try {
        return clazz.cast(inflater.inflate(resourceId, root, attachToRoot));
      } catch (OutOfMemoryError e) {
        // Retry with GC.
        System.gc();
      }
    }

    return clazz.cast(inflater.inflate(resourceId, root, attachToRoot));
  }

  public static Bitmap createBitmap(int width, int height, Config config) {
    for (int i = 0; i < OUT_OF_MEMORY_RETRY_COUNT; ++i) {
      try {
        return Bitmap.createBitmap(width, height, config);
      } catch (OutOfMemoryError e) {
        // Retry with GC.
        System.gc();
      }
    }

    return Bitmap.createBitmap(width, height, config);
  }

  public static Bitmap createBitmap(Bitmap src) {
    for (int i = 0; i < OUT_OF_MEMORY_RETRY_COUNT; ++i) {
      try {
        return Bitmap.createBitmap(src);
      } catch (OutOfMemoryError e) {
        // Retry with GC.
        System.gc();
      }
    }

    return Bitmap.createBitmap(src);
  }

  /**
   * Sets the given {@code token} and some layout parameters required to show the dialog from
   * the IME service correctly to the {@code dialog}.
   */
  public static void setWindowToken(IBinder token, Dialog dialog) {
    if (token == null) {
      MozcLog.w("Unknown window token.");
      return;
    }

    Window window = dialog.getWindow();
    WindowManager.LayoutParams layoutParams = window.getAttributes();
    layoutParams.token = token;
    layoutParams.type = WindowManager.LayoutParams.TYPE_APPLICATION_ATTACHED_DIALOG;
    layoutParams.flags |= WindowManager.LayoutParams.FLAG_ALT_FOCUSABLE_IM;
    window.setAttributes(layoutParams);
  }

  public static Request getRequestForKeyboard(
      KeyboardSpecificationName specName,
      SpecialRomanjiTable romanjiTable,
      SpaceOnAlphanumeric spaceOnAlphanumeric,
      Boolean isKanaModifierInsensitiveConversion,
      CrossingEdgeBehavior crossingEdgeBehavior,
      Configuration configuration) {
    Request.Builder builder = Request.newBuilder();
    builder.setKeyboardName(specName.formattedKeyboardName(configuration));
    if (romanjiTable != null) {
      builder.setSpecialRomanjiTable(romanjiTable);
    }
    if (spaceOnAlphanumeric != null) {
      builder.setSpaceOnAlphanumeric(spaceOnAlphanumeric);
    }
    if (isKanaModifierInsensitiveConversion != null) {
      builder.setKanaModifierInsensitiveConversion(
          isKanaModifierInsensitiveConversion.booleanValue());
    }
    if (crossingEdgeBehavior != null) {
      builder.setCrossingEdgeBehavior(crossingEdgeBehavior);
    }
    return builder.build();
  }

  public static boolean isEmojiAllowed(EditorInfo editorInfo) {
    Bundle bundle = editorInfo.extras;
    return (bundle != null) && bundle.getBoolean("allowEmoji");
  }

  public static Request createEmojiRequest(
      int sdkInt, EmojiProviderType emojiProviderType) {
    int availableEmojiCarrier = 0;
    if (sdkInt >= 16) {
      // Unicode emoji is available when SDK version is equal to or higher than Jelly bean.
      availableEmojiCarrier |= EmojiCarrierType.UNICODE_EMOJI.getNumber();
    }

    EmojiCarrierType emojiCarrierType =
        EMOJI_PROVIDER_TYPE_MAP.get(emojiProviderType);
    if (emojiCarrierType != null) {
      availableEmojiCarrier |= emojiCarrierType.getNumber();
    }

    return Request.newBuilder()
        .setAvailableEmojiCarrier(availableEmojiCarrier)
        .setEmojiRewriterCapability(RewriterCapability.ALL.getNumber())
        .build();
  }

  public static String utf8CStyleByteStringToString(ByteString value) {
    // Find '\0' terminator. (if value doesn't contain '\0', the size should be as same as
    // value's.)
    int index = 0;
    while (index < value.size() && value.byteAt(index) != 0) {
      ++index;
    }

    byte[] bytes = new byte[index];
    value.copyTo(bytes, 0, 0, bytes.length);
    try {
      return new String(bytes, "UTF-8");
    } catch (UnsupportedEncodingException e) {
      // Should never happen because of Java's spec.
      MozcLog.w("UTF-8 charset is not available");
    }

    return null;
  }

  /**
   * Simple utility to close {@link Closeable} instance.
   *
   * A typical usage is as follows:
   * <pre>{@code
   *   Closeable stream = ...;
   *   boolean succeeded = false;
   *   try {
   *     // Read data from stream here.
   *     ...
   *
   *     succeeded = true;
   *   } finally {
   *     close(stream, !succeeded);
   *   }
   * }</pre>
   *
   * @param closeable
   * @param ignoreException
   * @throws IOException
   */
  public static void close(Closeable closeable, boolean ignoreException) throws IOException {
    try {
      closeable.close();
    } catch (IOException e) {
      if (!ignoreException) {
        throw e;
      }
    }
  }

  /**
   * Simple utility to close {@link Socket} instance.
   * See {@link MozcUtil#close(Closeable,boolean)} for details.
   */
  public static void close(Socket socket, boolean ignoreIOException) throws IOException {
    try {
      socket.close();
    } catch (IOException e) {
      if (!ignoreIOException) {
        throw e;
      }
    }
  }

  /**
   * Simple utility to close {@link ServerSocket} instance.
   * See {@link MozcUtil#close(Closeable,boolean)} for details.
   */
  public static void close(ServerSocket socket, boolean ignoreIOException) throws IOException {
    try {
      socket.close();
    } catch (IOException e) {
      if (!ignoreIOException) {
        throw e;
      }
    }
  }

  /**
   * Closes the given {@code closeable}, and ignore any {@code IOException}s.
   */
  public static void closeIgnoringIOException(Closeable closeable) {
    try {
      if (closeable != null) {
        closeable.close();
      }
    } catch (IOException e) {
      MozcLog.e("Failed to close.", e);
    }
  }

  /**
   * Closes the given {@code closeable}, and ignore any {@code IOException}s.
   */
  public static void closeIgnoringIOException(ZipFile zipFile) {
    try {
      if (zipFile != null) {
        zipFile.close();
      }
    } catch (IOException e) {
      MozcLog.e("Failed to close.", e);
    }
  }

  /**
   * Relaxes ThreadPolicy to enable network access.
   *
   * public accessibility for easier invocation via reflection.
   */
  @TargetApi(9)
  public static class StrictModeRelaxer {
    private StrictModeRelaxer() {}
    public static void relaxStrictMode() {
      StrictMode.setThreadPolicy(new StrictMode.ThreadPolicy.Builder().permitNetwork().build());
    }
  }

  // The restriction is applied since API Level 9.
  private static boolean isStrictModeRelaxed = Build.VERSION.SDK_INT < 9;

  /**
   * Relaxes the main thread's restriction.
   *
   * On newer Android OS, network access from the main thread is forbidden by default.
   * This method may relax the restriction.
   *
   * Note that this method works only in Debug build because this depends on reflection and
   * ProGuard will obfuscate the symbol name.
   * This behavior will not be fixed because this method is used only from Debug utilities
   * (i.e. SocketSessionHandler).
   */
  public static void relaxMainthreadStrictMode() {
    if (isStrictModeRelaxed) {
      // Already relaxed.
      return;
    }
    if (!Thread.currentThread().equals(Looper.getMainLooper().getThread())) {
      // Not on the main thread.
      return;
    }
    try {
      Class<?> clazz =
          Class.forName(new StringBuilder(MozcUtil.class.getCanonicalName())
                                          .append('$')
                                          .append("StrictModeRelaxer").toString());
      clazz.getMethod("relaxStrictMode").invoke(null);
      isStrictModeRelaxed = true;
    } catch (ClassNotFoundException e) {
      MozcLog.e(e.getMessage(), e);
    } catch (NoSuchMethodException e) {
      MozcLog.e(e.getMessage(), e);
    } catch (IllegalArgumentException e) {
      MozcLog.e(e.getMessage(), e);
    } catch (IllegalAccessException e) {
      MozcLog.e(e.getMessage(), e);
    } catch (InvocationTargetException e) {
      MozcLog.e(e.getMessage(), e);
    }
  }
}
