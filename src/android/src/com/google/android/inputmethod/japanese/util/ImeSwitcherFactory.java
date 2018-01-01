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

package org.mozc.android.inputmethod.japanese.util;

import org.mozc.android.inputmethod.japanese.MozcLog;
import org.mozc.android.inputmethod.japanese.MozcUtil;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Preconditions;

import android.annotation.TargetApi;
import android.inputmethodservice.InputMethodService;
import android.os.Build;
import android.os.IBinder;
import android.view.inputmethod.InputMethodInfo;
import android.view.inputmethod.InputMethodManager;
import android.view.inputmethod.InputMethodSubtype;

import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.util.List;
import java.util.Map;

/**
 * Factory class for IME switching.
 *
 * Typical usage ;
 * {@code
 * ImeSwitcher switcher = ImeSwitcherFactory.getImeSwitcher(inputMethodService);
 * boolean isVoiceAvailable = switcher.isVoiceImeAvailable();
 * if (isVoiceAvailable) {
 *   switcher.switchToVoiceIme("ja");
 * }
 * }
 *
 */
public class ImeSwitcherFactory {

  private static final String GOOGLE_PACKAGE_ID_PREFIX = "com.google.android";
  private static final String VOICE_IME_MODE = "voice";

  /**
   * A container of an InputMethodInfo and an InputMethodSubtype.
   */
  private static class InputMethodSubtypeInfo {

    private final InputMethodInfo inputMethodInfo;
    private final InputMethodSubtype subtype;

    public InputMethodSubtypeInfo(
        InputMethodInfo inputMethodInfo, InputMethodSubtype subtype) {
      if (inputMethodInfo == null || subtype == null) {
        throw new NullPointerException("All the prameters must be non-null");
      }
      this.inputMethodInfo = inputMethodInfo;
      this.subtype = subtype;
    }

    public InputMethodInfo getInputMethodInfo() {
      return inputMethodInfo;
    }

    public InputMethodSubtype getSubtype() {
      return subtype;
    }
  }

  /**
   * A class to switch default (==current) input method subtype.
   */
  public interface ImeSwitcher {

    /**
     * Returns true if at least one voice IME is available.
     *
     * Eligible voice IME subtype must match following conditions.
     * <ul>
     * <li>The voice IME's id has prefix "com.google.android" (for security and avoiding from
     *     activating unexpected IME).
     * <li>The subtype's mode is "voice".
     * <li>The subtype is auxiliary (usually a user wants to get back to this IME when (s)he
     *     types back key).
     * </ul>
     *
     * You don't have to call this method so frequently. Just call after IME's activation.
     * The only way to make available an IME is to use system preference screen and when a user
     * returns from it to an application the IME receives onStartInput.
     */
    public boolean isVoiceImeAvailable();

    /**
     * Switches to voice ime if possible
     *
     * @param locale preferred locale. This method uses the subtype of which locale is given one
     *        but this is best effort.
     * @return true if target subtype is found. Note that even if switching itself fails this method
     *         might return true.
     */
    boolean switchToVoiceIme(String locale);

    /**
     * Switches to *next* IME (including subtype).
     *
     * @param onlyCurrentIme if true this method find next candidates which are in this IME.
     * @return true if the switching succeeds
     */
    boolean switchToNextInputMethod(boolean onlyCurrentIme);

    /**
     * @see InputMethodManager#shouldOfferSwitchingToNextInputMethod(IBinder)
     *
     * If not supported the API, returns false.
     */
    boolean shouldOfferSwitchingToNextInputMethod();
  }

  /**
   * A switcher for later OS where IME subtype is available.
   */
  static class SubtypeImeSwitcher implements ImeSwitcher {

    interface InputMethodManagerProxy {
      Map<InputMethodInfo, List<InputMethodSubtype>> getShortcutInputMethodsAndSubtypes();
      void setInputMethodAndSubtype(IBinder token, String id, InputMethodSubtype subtype);
    }

    protected final InputMethodService inputMethodService;
    private final InputMethodManagerProxy inputMethodManagerProxy;

    public SubtypeImeSwitcher(final InputMethodService inputMethodService) {
      this(inputMethodService, new InputMethodManagerProxy() {
        InputMethodManager inputMethodManager = MozcUtil.getInputMethodManager(inputMethodService);
        @Override
        public Map<InputMethodInfo, List<InputMethodSubtype>> getShortcutInputMethodsAndSubtypes() {
          return inputMethodManager.getShortcutInputMethodsAndSubtypes();
        }
        @Override
        public void setInputMethodAndSubtype(IBinder token,
            String id, InputMethodSubtype subtype) {
          inputMethodManager.setInputMethodAndSubtype(token, id, subtype);
        }
      });
    }

    @VisibleForTesting
    SubtypeImeSwitcher(InputMethodService inputMethodService,
                       InputMethodManagerProxy inputMethodManagerProxy) {
      this.inputMethodService = inputMethodService;
      this.inputMethodManagerProxy = inputMethodManagerProxy;
    }

    protected IBinder getToken() {
      return inputMethodService.getWindow().getWindow().getAttributes().token;
    }

    /**
     * @param locale if null locale test is omitted (for performance)
     * @return null if no candidate is found
     */
    private InputMethodSubtypeInfo getVoiceInputMethod(String locale) {
      InputMethodSubtypeInfo fallBack = null;
      for (Map.Entry<InputMethodInfo, List<InputMethodSubtype>> inputmethod :
           inputMethodManagerProxy.getShortcutInputMethodsAndSubtypes().entrySet()) {
        InputMethodInfo inputMethodInfo = inputmethod.getKey();
        if (!inputMethodInfo.getComponent().getPackageName().startsWith(GOOGLE_PACKAGE_ID_PREFIX)) {
          continue;
        }
        for (InputMethodSubtype subtype : inputmethod.getValue()) {
          if (!subtype.isAuxiliary() || !subtype.getMode().equals(VOICE_IME_MODE)) {
            continue;
          }
          if (locale == null || subtype.getLocale().equals(locale)) {
            return new InputMethodSubtypeInfo(inputMethodInfo, subtype);
          }
          fallBack = new InputMethodSubtypeInfo(inputMethodInfo, subtype);
        }
      }
      return fallBack;
    }

    @Override
    public boolean isVoiceImeAvailable() {
      // Here we don't take account of locale. Any voice IMEs are acceptable.
      return getVoiceInputMethod(null) != null;
    }

    @Override
    public boolean switchToVoiceIme(String locale) {
      InputMethodSubtypeInfo inputMethod = getVoiceInputMethod(locale);
      if (inputMethod == null) {
        return false;
      }
      inputMethodManagerProxy.setInputMethodAndSubtype(
          getToken(),
          inputMethod.getInputMethodInfo().getId(),
          inputMethod.getSubtype());
      return true;
    }

    @Override
    public boolean switchToNextInputMethod(boolean onlyCurrentIme) {
      return false;
    }

    @Override
    public boolean shouldOfferSwitchingToNextInputMethod() {
      return false;
    }
  }

  /**
   * A switcher for much later OS where switchToNextInputMethod is available.
   */
  @TargetApi(16)
  static class ImeSwitcher16 extends SubtypeImeSwitcher {

    public ImeSwitcher16(InputMethodService inputMethodService) {
      super(inputMethodService);
    }

    @Override
    public boolean switchToNextInputMethod(boolean onlyCurrentIme) {
      return MozcUtil.getInputMethodManager(inputMethodService)
                 .switchToNextInputMethod(getToken(), onlyCurrentIme);
    }
  }

  /**
   * A switcher for much later OS where switchToNextInputMethod is available.
   */
  @TargetApi(19)
  static class ImeSwitcher21 extends ImeSwitcher16 {

    public ImeSwitcher21(InputMethodService inputMethodService) {
      super(inputMethodService);
    }

    @Override
    public boolean shouldOfferSwitchingToNextInputMethod() {
      return MozcUtil.getInputMethodManager(inputMethodService)
          .shouldOfferSwitchingToNextInputMethod(getToken());
    }
  }

  // A constructor of concrete switcher class.
  // Null if reflection fails.
  static final Constructor<? extends ImeSwitcher> switcherConstructor;

  static {
    Class<? extends ImeSwitcher> clazz;
    if (Build.VERSION.SDK_INT >= 21) {
      clazz = ImeSwitcher21.class;
    } else if (Build.VERSION.SDK_INT >= 16) {
      clazz = ImeSwitcher16.class;
    } else {
      clazz = SubtypeImeSwitcher.class;
    }
    Constructor<? extends ImeSwitcher> constructor = null;
    try {
      constructor = clazz.getConstructor(InputMethodService.class);
    } catch (NoSuchMethodException e) {
      MozcLog.e("No ImeSwitcher's constructor found.", e);
    }
    switcherConstructor = constructor;
  }

  /**
   * Gets an {@link ImeSwitcher}.
   */
  public static ImeSwitcher getImeSwitcher(InputMethodService inputMethodService) {
    Preconditions.checkNotNull(inputMethodService);
    if (switcherConstructor != null) {
      try {
        return switcherConstructor.newInstance(inputMethodService);
      } catch (IllegalArgumentException e) {
        MozcLog.e(e.getMessage(), e);
      } catch (InstantiationException e) {
        MozcLog.e(e.getMessage(), e);
      } catch (IllegalAccessException e) {
        MozcLog.e(e.getMessage(), e);
      } catch (InvocationTargetException e) {
        MozcLog.e(e.getMessage(), e);
      }
    }
    return new SubtypeImeSwitcher(inputMethodService);
  }
}
