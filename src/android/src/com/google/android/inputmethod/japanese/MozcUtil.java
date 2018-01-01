// Copyright 2010-2018, Google Inc.
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

import org.mozc.android.inputmethod.japanese.keyboard.Keyboard.KeyboardSpecification;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Request;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;
import com.google.common.base.Strings;
import com.google.protobuf.ByteString;

import android.annotation.SuppressLint;
import android.app.Dialog;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.os.Build;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.Message;
import android.os.ParcelFileDescriptor;
import android.os.StrictMode;
import android.os.SystemClock;
import android.provider.Settings;
import android.telephony.TelephonyManager;
import android.text.InputType;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodInfo;
import android.view.inputmethod.InputMethodManager;

import java.io.Closeable;
import java.io.File;
import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.lang.reflect.InvocationTargetException;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.zip.ZipFile;

/**
 * Utility class
 *
 */
public final class MozcUtil {

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
      this.telephonyManager = Preconditions.checkNotNull(telephonyManager);
    }

    @Override
    public String getNetworkOperator() {
      return telephonyManager.getNetworkOperator();
    }
  }

  static class InputMethodPickerShowingCallback implements Handler.Callback {

    @Override
    public boolean handleMessage(Message msg) {
      Context context = Context.class.cast(Preconditions.checkNotNull(msg).obj);
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

  private static Optional<Boolean> isDebug = Optional.<Boolean>absent();
  private static Optional<Boolean> isDevChannel = Optional.<Boolean>absent();
  private static Optional<Boolean> isMozcEnabled = Optional.<Boolean>absent();
  private static Optional<Boolean> isMozcDefaultIme = Optional.<Boolean>absent();
  private static Optional<Boolean> isSystemApplication = Optional.<Boolean>absent();
  private static Optional<Boolean> isTouchUI = Optional.<Boolean>absent();
  private static Optional<Boolean> isUpdatedSystemApplication = Optional.<Boolean>absent();
  private static Optional<Integer> versionCode = Optional.<Integer>absent();
  private static Optional<Long> uptimeMillis = Optional.<Long>absent();

  private static final int SHOW_INPUT_METHOD_PICKER_WHAT = 0;
  private static Optional<Handler> showInputMethodPickerHandler = Optional.absent();

  public static final String IME_OPTION_NO_MICROPHONE_COMPAT = "nm";
  public static final String IME_OPTION_NO_MICROPHONE =
      "com.google.android.inputmethod.latin.noMicrophoneKey";

  private static final String USER_DICTIONARY_EXPORT_DIR = "user_dictionary_export";

  /**
   * Lazy creation of a handler.
   *
   * Creating a handler must be done in threads which has a Looper.
   * This lazy creation enables worker threads (which don't have a Looper) to access this class.
   */
  private static Handler getShowInputMethodPickerHandler() {
    if (!showInputMethodPickerHandler.isPresent()) {
      showInputMethodPickerHandler =
        Optional.of(new Handler(new InputMethodPickerShowingCallback()));
    }
    return showInputMethodPickerHandler.get();
  }

  // Disallow instantiation.
  private MozcUtil() {
  }

  private static final boolean checkApplicationFlag(Context context, int flag) {
    Preconditions.checkNotNull(context);
    PackageManager manager = context.getPackageManager();
    try {
      ApplicationInfo appInfo =
          manager.getApplicationInfo(context.getPackageName(), 0);
      return (appInfo.flags & flag) != 0;
    } catch (NameNotFoundException e) {
      MozcLog.w("PackageManager#getApplicationInfo cannot find this application.");
      return false;
    }
  }

  public static final boolean isDebug(Context context) {
    Preconditions.checkNotNull(context);
    if (isDebug.isPresent()) {
      return isDebug.get();
    }
    return checkApplicationFlag(context, ApplicationInfo.FLAG_DEBUGGABLE);
  }

  /**
   * For testing purpose.
   *
   * @param isDebug Optional.absent() if default behavior is preferable
   */
  public static final void setDebug(Optional<Boolean> isDebug) {
    MozcUtil.isDebug = Preconditions.checkNotNull(isDebug);
  }

  public static final boolean isDevChannel(Context context) {
    Preconditions.checkNotNull(context);
    if (isDevChannel.isPresent()) {
      return isDevChannel.get();
    }
    return isDevChannelVersionName(getVersionName(context));
  }

  public static final boolean isSystemApplication(Context context) {
    Preconditions.checkNotNull(context);
    if (isSystemApplication.isPresent()) {
      return isSystemApplication.get();
    }
    return checkApplicationFlag(context, ApplicationInfo.FLAG_SYSTEM);
  }

  /**
   * For testing purpose.
   * @param isSystemApplication Optional.absent() if default behavior is preferable
   */
  public static final void setSystemApplication(Optional<Boolean> isSystemApplication) {
    MozcUtil.isSystemApplication = Preconditions.checkNotNull(isSystemApplication);
  }

  public static final boolean isUpdatedSystemApplication(Context context) {
    Preconditions.checkNotNull(context);
    if (isUpdatedSystemApplication.isPresent()) {
      return isUpdatedSystemApplication.get();
    }
    return checkApplicationFlag(context, ApplicationInfo.FLAG_UPDATED_SYSTEM_APP);
  }

  /**
   * For testing purpose.
   * @param isUpdatedSystemApplication Optional.absent() if default behavior is preferable
   */
  public static final void setUpdatedSystemApplication(
      Optional<Boolean> isUpdatedSystemApplication) {
    MozcUtil.isUpdatedSystemApplication = Preconditions.checkNotNull(isUpdatedSystemApplication);
  }

  /**
   * Gets version name.
   *
   * @return version name string or empty. Non-null is guaranteed.
   */
  public static String getVersionName(Context context) {
    Preconditions.checkNotNull(context);

    PackageManager manager = context.getPackageManager();
    try {
      PackageInfo packageInfo = manager.getPackageInfo(context.getPackageName(), 0);
      return Strings.nullToEmpty(packageInfo.versionName);
    } catch (NameNotFoundException e) {
      MozcLog.e("Package info error", e);
    }
    return "";
  }

  /**
   * For testing purpose.
   */
  public static void setVersionCode(Optional<Integer> versionCode) {
    MozcUtil.versionCode = Preconditions.checkNotNull(versionCode);
  }

  /**
   * Gets raw (ABI dependent) version code.
   *
   * @return version code. Note that this version code contains ABI specific mask.
   */
  public static int getVersionCode(Context context) {
    Preconditions.checkNotNull(context);

    if (MozcUtil.versionCode.isPresent()) {
      return MozcUtil.versionCode.get();
    }

    PackageManager manager = context.getPackageManager();
    try {
      PackageInfo packageInfo = manager.getPackageInfo(context.getPackageName(), 0);
      return packageInfo.versionCode;
    } catch (NameNotFoundException e) {
      MozcLog.e("Package info error", e);
    }
    return 0;
  }

  /**
   * Gets ABI independent version code.
   *
   * ABI independent version code is equivalent to "Build number" of Mozc project.
   *
   * <p>Must be consistent with mozc_version.py
   */
  public static int getAbiIndependentVersionCode(Context context) {
    // Version code format:
    // 00000BBBBB or
    // 0006BBBBBA
    // A: ABI (0: Fat, 6: x86_64, 5:arm64, 4:mips64, 3: x86, 2: armeabi-v7a, 1:mips)
    // B: ANDROID_VERSION_CODE
    Preconditions.checkNotNull(context);
    int rawVersionCode = getVersionCode(context);
    String versionCode = Integer.toString(getVersionCode(context));
    if (versionCode.length() == 7 && versionCode.charAt(0) == '6') {
      return Integer.valueOf(versionCode.substring(1, versionCode.length() - 1));
    }
    return rawVersionCode;
  }

  /**
   * For testing purpose.
   *
   * @param isDevChannel Optional.absent() if default behavior is preferable
   */
  public static final void setDevChannel(Optional<Boolean> isDevChannel) {
    MozcUtil.isDevChannel = Preconditions.checkNotNull(isDevChannel);
  }

  /**
   * Returns true if {@code versionName} indicates dev channel.
   *
   * This method sees only the characters between the last period and hyphen.
   * If the characters is greater than or equal to (int)100,
   * this method returns true.
   */
  @VisibleForTesting static boolean isDevChannelVersionName(String versionName) {
    Preconditions.checkNotNull(versionName);

    int lastDot = versionName.lastIndexOf('.');
    if (lastDot < 0) {
      return false;
    }
    int lastHyphen = versionName.lastIndexOf('-');
    if (lastHyphen < 0) {
      lastHyphen = versionName.length();
    }
    try {
      return Integer.parseInt(versionName.substring(lastDot + 1, lastHyphen)) >= 100;
    } catch (NumberFormatException e) {
      return false;
    }
  }

  /**
   * @param context an application context. Shouldn't be {@code null}.
   * @return {@code true} if Mozc is enabled. Otherwise {@code false}.
   */
  public static boolean isMozcEnabled(Context context) {
    Preconditions.checkNotNull(context);
    if (isMozcEnabled.isPresent()) {
      return isMozcEnabled.get();
    }

    InputMethodManager inputMethodManager = getInputMethodManager(context);
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
   *
   * @param isMozcEnabled Optional.absent() for default behavior
   */
  public static void setMozcEnabled(Optional<Boolean> isMozcEnabled) {
    MozcUtil.isMozcEnabled = Preconditions.checkNotNull(isMozcEnabled);
  }

  /**
   * @param context an application context. Shouldn't be {@code null}.
   * @return {@code true} if the default IME is Mozc. Otherwise {@code false}.
   */
  public static boolean isMozcDefaultIme(Context context) {
    Preconditions.checkNotNull(context);
    if (isMozcDefaultIme.isPresent()) {
      return isMozcDefaultIme.get();
    }

    Optional<InputMethodInfo> mozcInputMethodInfo = getMozcInputMethodInfo(context);
    if (!mozcInputMethodInfo.isPresent()) {
      MozcLog.w("Mozc's InputMethodInfo is not found.");
      return false;
    }

    String currentIme = Settings.Secure.getString(
        context.getContentResolver(), Settings.Secure.DEFAULT_INPUT_METHOD);
    return mozcInputMethodInfo.get().getId().equals(currentIme);
  }

  /**
   * Just injects the result of isMozcDefaultIme for testing purpose,
   * doesn't actually set mozc as the default ime.
   *
   * @param isMozcDefaultIme Optional.absent() for default behavior
   */
  public static void setMozcDefaultIme(Optional<Boolean> isMozcDefaultIme) {
    MozcUtil.isMozcDefaultIme = Preconditions.checkNotNull(isMozcDefaultIme);
  }

  private static Optional<InputMethodInfo> getMozcInputMethodInfo(Context context) {
    Preconditions.checkNotNull(context);
    InputMethodManager inputMethodManager = getInputMethodManager(context);
    if (inputMethodManager == null) {
      MozcLog.w("InputMethodManager is not found.");
      return Optional.absent();
    }
    String packageName = context.getPackageName();
    for (InputMethodInfo inputMethodInfo : inputMethodManager.getInputMethodList()) {
      if (inputMethodInfo.getPackageName().equals(packageName)) {
        return Optional.of(inputMethodInfo);
      }
    }

    // Not found.
    return Optional.absent();
  }

  /**
   * Returns true is touch UI should be shown.
   */
  public static boolean isTouchUI(Context context) {
    Preconditions.checkNotNull(context);
    if (isTouchUI.isPresent()) {
      return isTouchUI.get();
    }
    return context.getResources().getConfiguration().touchscreen
               != Configuration.TOUCHSCREEN_NOTOUCH;
  }

  /**
   * For testing purpose.
   *
   * @param isTouchUI Optional.absent() if default behavior is preferable
   */
  public static final void setTouchUI(Optional<Boolean> isTouchUI) {
    MozcUtil.isTouchUI = Preconditions.checkNotNull(isTouchUI);
  }

  public static long getUptimeMillis() {
    if (uptimeMillis.isPresent()) {
      return uptimeMillis.get();
    }
    return SystemClock.uptimeMillis();
  }

  /**
   * For testing purpose.
   *
   * @param uptimeMillis Optional.absent() if default behavior is preferable
   */
  public static void setUptimeMillis(Optional<Long> uptimeMillis) {
    MozcUtil.uptimeMillis = Preconditions.checkNotNull(uptimeMillis);
  }

  public static boolean requestShowInputMethodPicker(Context context) {
    Preconditions.checkNotNull(context);
    Handler showInputMethodPickerHandler = getShowInputMethodPickerHandler();
    return showInputMethodPickerHandler.sendMessage(
        showInputMethodPickerHandler.obtainMessage(SHOW_INPUT_METHOD_PICKER_WHAT, context));
  }

  public static void cancelShowInputMethodPicker(Context context) {
    Preconditions.checkNotNull(context);
    Handler showInputMethodPickerHandler = getShowInputMethodPickerHandler();
    showInputMethodPickerHandler.removeMessages(SHOW_INPUT_METHOD_PICKER_WHAT, context);
  }

  public static boolean hasShowInputMethodPickerRequest(Context context) {
    Preconditions.checkNotNull(context);
    Handler showInputMethodPickerHandler = getShowInputMethodPickerHandler();
    return showInputMethodPickerHandler.hasMessages(SHOW_INPUT_METHOD_PICKER_WHAT, context);
  }

  /**
   * Returns the {@code TelephonyManagerInterface} corresponding to the given {@code context}.
   */
  public static TelephonyManagerInterface getTelephonyManager(Context context) {
    Preconditions.checkNotNull(context);
    return new TelephonyManagerImpl(
        TelephonyManager.class.cast(context.getSystemService(Context.TELEPHONY_SERVICE)));
  }

  /**
   * Sets the given {@code token} and some layout parameters required to show the dialog from
   * the IME service correctly to the {@code dialog}.
   */
  public static void setWindowToken(IBinder token, Dialog dialog) {
    Preconditions.checkNotNull(token);
    Preconditions.checkNotNull(dialog);

    Window window = dialog.getWindow();
    WindowManager.LayoutParams layoutParams = window.getAttributes();
    layoutParams.token = token;
    layoutParams.type = WindowManager.LayoutParams.TYPE_APPLICATION_ATTACHED_DIALOG;
    layoutParams.flags |= WindowManager.LayoutParams.FLAG_ALT_FOCUSABLE_IM;
    window.setAttributes(layoutParams);
  }

  private static Request.Builder getRequestBuilderInternal(
      KeyboardSpecification specification, Configuration configuration) {
    return Request.newBuilder()
        .setKeyboardName(
            specification.getKeyboardSpecificationName().formattedKeyboardName(configuration))
        .setSpecialRomanjiTable(specification.getSpecialRomanjiTable())
        .setSpaceOnAlphanumeric(specification.getSpaceOnAlphanumeric())
        .setKanaModifierInsensitiveConversion(
            specification.isKanaModifierInsensitiveConversion())
        .setCrossingEdgeBehavior(specification.getCrossingEdgeBehavior());
  }

  private static void setHardwareKeyboardRequest(Request.Builder builder, Resources resources) {
    builder.setMixedConversion(false)
        .setZeroQuerySuggestion(false)
        .setUpdateInputModeFromSurroundingText(true)
        .setAutoPartialSuggestion(false)
        .setCandidatePageSize(resources.getInteger(R.integer.floating_candidate_candidate_num));
  }

  public static void setSoftwareKeyboardRequest(Request.Builder builder) {
    builder.setMixedConversion(true)
        .setZeroQuerySuggestion(true)
        .setUpdateInputModeFromSurroundingText(false)
        .setAutoPartialSuggestion(true);
  }

  public static Request.Builder getRequestBuilder(
      Resources resources, KeyboardSpecification specification, Configuration configuration) {
    Preconditions.checkNotNull(resources);
    Preconditions.checkNotNull(specification);
    Preconditions.checkNotNull(configuration);
    Request.Builder builder = getRequestBuilderInternal(specification, configuration);
    if (specification.isHardwareKeyboard()) {
      setHardwareKeyboardRequest(builder, resources);
    } else {
      setSoftwareKeyboardRequest(builder);
    }
    return builder;
  }

  @SuppressLint("InlinedApi")
  public static boolean isPasswordField(int inputType) {
    int inputClass = inputType & InputType.TYPE_MASK_CLASS;
    int inputVariation = inputType & InputType.TYPE_MASK_VARIATION;
    return inputClass == InputType.TYPE_CLASS_TEXT
        && (inputVariation == InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD
            || inputVariation == InputType.TYPE_TEXT_VARIATION_PASSWORD
            || inputVariation == InputType.TYPE_TEXT_VARIATION_WEB_PASSWORD);
  }

  /**
   * Returns true if voice input is preferred by EditorInfo.inputType.
   *
   * <p>Caller should check that voice input is allowed or not by isVoiceInputAllowed().
   */
  public static boolean isVoiceInputPreferred(EditorInfo editorInfo) {
    Preconditions.checkNotNull(editorInfo);

    // Check privateImeOptions to ensure the text field supports voice input.
    if (editorInfo.privateImeOptions != null) {
      for (String option : editorInfo.privateImeOptions.split(",")) {
        if (option.equals(IME_OPTION_NO_MICROPHONE)
            || option.equals(IME_OPTION_NO_MICROPHONE_COMPAT)) {
          return false;
        }
      }
    }

    int inputType = editorInfo.inputType;
    if (isNumberKeyboardPreferred(inputType) || isPasswordField(inputType)) {
      return false;
    }
    if ((inputType & EditorInfo.TYPE_MASK_CLASS) == InputType.TYPE_CLASS_TEXT) {
      switch (inputType & EditorInfo.TYPE_MASK_VARIATION) {
        case InputType.TYPE_TEXT_VARIATION_EMAIL_ADDRESS:
        case InputType.TYPE_TEXT_VARIATION_WEB_EMAIL_ADDRESS:
        case InputType.TYPE_TEXT_VARIATION_URI:
          return false;
        default:
          break;
      }
    }
    return true;
  }

  /** Returns true if number keyboard is preferred by EditorInfo.inputType. */
  public static boolean isNumberKeyboardPreferred(int inputType) {
    int typeClass = inputType & InputType.TYPE_MASK_CLASS;
    // As of API Level 21, following condition equals to "typeClass != InputType.TYPE_CLASS_TEXT".
    // However type-class might be added in future so safer expression is employed here.
    return typeClass == InputType.TYPE_CLASS_DATETIME
        || typeClass == InputType.TYPE_CLASS_NUMBER
        || typeClass == InputType.TYPE_CLASS_PHONE;
  }

  public static String utf8CStyleByteStringToString(ByteString value) {
    Preconditions.checkNotNull(value);
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
      throw new IllegalStateException("UTF-8 charset is not available.");
    }
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
    Preconditions.checkNotNull(closeable);
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
    Preconditions.checkNotNull(socket);
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
    Preconditions.checkNotNull(socket);
    try {
      socket.close();
    } catch (IOException e) {
      if (!ignoreIOException) {
        throw e;
      }
    }
  }

  /**
   * Simple utility to close {@link ParcelFileDescriptor} instance.
   * See {@link MozcUtil#close(Closeable,boolean)} for details.
   *
   * On later OS ParcelFileDescriptor implements {@link Closeable} but on earlier OS it doesn't.
   */
  public static void close(ParcelFileDescriptor descriptor, boolean ignoreIOException)
      throws IOException {
    Preconditions.checkNotNull(descriptor);
    try {
      descriptor.close();
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
      Preconditions.checkNotNull(closeable).close();
    } catch (IOException e) {
      MozcLog.e("Failed to close.", e);
    }
  }

  /**
   * Closes the given {@code closeable}, and ignore any {@code IOException}s.
   */
  public static void closeIgnoringIOException(ZipFile zipFile) {
    try {
      Preconditions.checkNotNull(zipFile).close();
    } catch (IOException e) {
      MozcLog.e("Failed to close.", e);
    }
  }

  public static InputMethodManager getInputMethodManager(Context context) {
    Preconditions.checkNotNull(context);
    return InputMethodManager.class.cast(context.getSystemService(Context.INPUT_METHOD_SERVICE));
  }

  /**
   * Get temporary directory for user dictionary export feature.
   *
   * This method creates a new directory if it doesn't exist.
   */
  public static File getUserDictionaryExportTempDirectory(Context context) {
    File directory = new File(context.getCacheDir().getAbsolutePath(), USER_DICTIONARY_EXPORT_DIR);
    if (directory.exists()) {
      Preconditions.checkState(directory.isDirectory());
    } else {
      directory.mkdir();
    }
    return directory;
  }

  /**
   * Delete contents of the directory.
   *
   * The root directory itself is NOT deleted.
   *
   * @return true if all entries are successfully deleted.
   */
  public static boolean deleteDirectoryContents(File directory) {
    Preconditions.checkArgument(Preconditions.checkNotNull(directory).isDirectory());
    boolean result = true;
    for (File entry : directory.listFiles()) {
      if (entry.isDirectory()) {
        result &= deleteDirectoryContents(entry);
      }
      result &= entry.delete();
    }
    return result;
  }

  /**
   * Relaxes ThreadPolicy to enable network access.
   *
   * public accessibility for easier invocation via reflection.
   */
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

  /**
   * If value &gt;= max returns max. If value &lt;= min returns min. Otherwise returns value.
   */
  public static int clamp(int value, int min, int max) {
    return Math.max(Math.min(value, max), min);
  }

  /**
   * If value &gt;= max returns max. If value &lt;= min returns min. Otherwise returns value.
   */
  public static float clamp(float value, float min, float max) {
    return Math.max(Math.min(value, max), min);
  }

  /**
   * Get a dimension for the specified orientation.
   * This method may be heavy since it updates the {@code resources} twice.
   */
  public static float getDimensionForOrientation(Resources resources, int id, int orientation) {
    Configuration configuration = resources.getConfiguration();
    if (configuration.orientation == orientation) {
      return resources.getDimension(id);
    }

    Configuration originalConfiguration = new Configuration(resources.getConfiguration());
    try {
      configuration.orientation = orientation;
      resources.updateConfiguration(configuration, null);
      return resources.getDimension(id);
    } finally {
      resources.updateConfiguration(originalConfiguration, null);
    }
  }
}
