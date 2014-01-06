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

package org.mozc.android.inputmethod.japanese.userdictionary;

import org.mozc.android.inputmethod.japanese.MozcUtil;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoUserDictionaryStorage.UserDictionary.Entry;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoUserDictionaryStorage.UserDictionary.PosType;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoUserDictionaryStorage.UserDictionaryCommandStatus.Status;
import org.mozc.android.inputmethod.japanese.resources.R;

import android.app.AlertDialog;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnDismissListener;
import android.content.Intent;
import android.content.res.Resources;
import android.net.Uri;
import android.os.Bundle;
import android.os.Message;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.view.WindowManager.LayoutParams;
import android.view.inputmethod.InputMethodManager;
import android.widget.ArrayAdapter;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.ScrollView;
import android.widget.Spinner;
import android.widget.TextView;

import java.io.IOException;
import java.io.RandomAccessFile;
import java.io.UnsupportedEncodingException;
import java.nio.ByteBuffer;
import java.nio.CharBuffer;
import java.nio.channels.FileChannel;
import java.nio.channels.FileChannel.MapMode;
import java.nio.charset.Charset;
import java.nio.charset.CodingErrorAction;
import java.util.ArrayList;
import java.util.Collections;
import java.util.EnumMap;
import java.util.List;
import java.util.Map;

/**
 * Utilities (of, especially, UI related stuff) for the user dictionary tool.
 *
 */
class UserDictionaryUtil {

  /**
   * Callback which is called when the "positive button" on a dialog is clicked.
   */
  private interface UserDictionaryBaseDialogListener {
    public Status onPositiveButtonClicked(View view);
  }

  /**
   * Base implementation of popup dialog on user dictionary tool.
   *
   * This class has:
   * <ul>
   * <li>Title messaging
   * <li>Content view based on the given resource id
   * <li>Cancel and OK buttons
   * </ul>
   *
   * When the OK button is clicked, UserDictionaryBaseDialogListener callback is invoked.
   * The expected usage is invoke something action by interacting with the mozc server.
   * If the interaction fails (in more precise, the returned status is not
   * USER_DICTIONARY_COMMAND_SUCCESS), this class will show a toast message, and the popup dialog
   * won't be dismissed.
   */
  private static class UserDictionaryBaseDialog extends AlertDialog {
    private final UserDictionaryBaseDialogListener listener;
    private final ToastManager toastManager;

    UserDictionaryBaseDialog(Context context, int titleResourceId, int viewResourceId,
                             UserDictionaryBaseDialogListener listener,
                             OnDismissListener dismissListener,
                             ToastManager toastManager) {
      super(context);
      this.listener = listener;
      this.toastManager = toastManager;

      // Initialize the view. Set the title, the content view and ok, cancel buttons.
      setTitle(titleResourceId);
      setView(LayoutInflater.from(context).inflate(viewResourceId, null));

      // Set a dummy Message instance to fix crashing issue on Android 2.1.
      // On Android 2.1, com.android.internal.app.AlertController wrongly checks the message
      // for the availability of button view. So without the dummy message,
      // getButton(BUTTON_POSITIVE) used in onCreate would return null.
      setButton(DialogInterface.BUTTON_POSITIVE, context.getText(android.R.string.yes),
                Message.obtain());
      setButton(DialogInterface.BUTTON_NEGATIVE, context.getText(android.R.string.cancel),
                DialogInterface.OnClickListener.class.cast(null));
      setCancelable(true);
      setOnDismissListener(dismissListener);
    }

    @Override
    protected void onCreate(Bundle savedInstance) {
      super.onCreate(savedInstance);

      ViewGroup contentGroup = ViewGroup.class.cast(findViewById(android.R.id.content));
      if (contentGroup != null && contentGroup.getChildCount() > 0) {
        // Wrap the content view by ScrollView so that we can scroll the dialog window
        // in order to avoid shrinking the main edit text fields.
        ScrollView view = ScrollView.class.cast(
            LayoutInflater.from(getContext()).inflate(
                R.layout.user_dictionary_tool_empty_scrollview, null));
        View contentView = contentGroup.getChildAt(0);
        contentGroup.removeViewAt(0);
        FrameLayout.class.cast(
            view.findViewById(R.id.user_dictionary_tool_empty_scroll_view_content))
            .addView(contentView);
        contentGroup.addView(view, 0);
      }

      // To override the default behavior that the dialog is dismissed after user's clicking
      // a button regardless of any action inside listener, we set the callback directly
      // to the button and manage dismissing behavior.
      // Note that it is necessary to do this here, instead of in the constructor,
      // because the UI is initialized in super class's onCreate method, and we cannot obtain
      // the button until the initialization.
      getButton(DialogInterface.BUTTON_POSITIVE).setOnClickListener(new View.OnClickListener() {
        @Override
        public void onClick(View view) {
          Status status = listener.onPositiveButtonClicked(view);
          toastManager.maybeShowMessageShortly(status);
          if (status == Status.USER_DICTIONARY_COMMAND_SUCCESS) {
            // Dismiss the dialog, iff the operation is successfully done.
            dismiss();
          }
        }
      });
    }
  }

  /**
   * Simple class to show the internationalized POS names on a spinner.
   */
  private static class PosItem {
    final PosType posType;
    final String name;

    PosItem(PosType posType, String name) {
      this.posType = posType;
      this.name = name;
    }

    // This is a hook to show the appropriate (internationalized) items on spinner.
    @Override
    public String toString() {
      return name;
    }
  }

  /**
   * Spinner implementation which has a list of POS.
   *
   * To keep users out from confusing UI, this class hides the IME when the list dialog is
   * shown by user's tap.
   */
  public static class PosSpinner extends Spinner {
    public PosSpinner(Context context) {
      super(context);
    }
    public PosSpinner(Context context, AttributeSet attrs) {
      super(context, attrs);
    }
    public PosSpinner(Context context, AttributeSet attrs, int defStyle) {
      super(context, attrs, defStyle);
    }

    @Override
    protected void onFinishInflate() {
      // Set adapter containing a list of POS types.
      ArrayAdapter<PosItem> adapter = new ArrayAdapter<PosItem>(
          getContext(), android.R.layout.simple_spinner_item, createPosItemList(getResources()));
      adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
      setAdapter(adapter);
    }

    private static List<PosItem> createPosItemList(Resources resources) {
      PosType[] posTypeValues = PosType.values();
      List<PosItem> result = new ArrayList<PosItem>(posTypeValues.length);
      for (PosType posType : posTypeValues) {
        result.add(new PosItem(posType,
                               resources.getText(getPosStringResourceId(posType)).toString()));
      }
      return result;
    }

    @Override
    public boolean performClick() {
      // When the spinner is tapped (i.e. when the popup dialog is shown),
      // we hide the soft input method.
      // This is because the list of the POS is long so if the soft input is shown
      // continuously, a part of list would be out of the display.
      InputMethodManager imm = InputMethodManager.class.cast(
          getContext().getSystemService(Context.INPUT_METHOD_SERVICE));
      imm.hideSoftInputFromWindow(getWindowToken(), 0);

      return super.performClick();
    }
  }

  interface WordRegisterDialogListener {

    /**
     * Callback to be called when the positive button is clicked.
     * @return result status of the executed command.
     */
    public Status onPositiveButtonClicked(String word, String reading, PosType pos);
  }

  /**
   * Word Register Dialog implementation for adding/editing entries.
   *
   * The dialog should have:
   * <ul>
   * <li>A EditText for "word" editing,
   * <li>A EditText for "reading" editing, and
   * <li>A Spinner for "pos" selecting.
   */
  static class WordRegisterDialog extends UserDictionaryBaseDialog {
    WordRegisterDialog(Context context, int titleResourceId,
                       final WordRegisterDialogListener listener,
                       OnDismissListener dismissListener,
                       ToastManager toastManager) {
      super(context, titleResourceId, R.layout.user_dictionary_tool_word_register_dialog_view,
            new UserDictionaryBaseDialogListener() {
              @Override
              public Status onPositiveButtonClicked(View view) {
                return listener.onPositiveButtonClicked(
                    getText(view, R.id.user_dictionary_tool_word_register_dialog_word),
                    getText(view, R.id.user_dictionary_tool_word_register_dialog_reading),
                    getPos(view, R.id.user_dictionary_tool_word_register_dialog_pos));
              }
            },
            dismissListener, toastManager);
      // TODO(hidehiko): Attach a callback for un-focused event on "word" EditText.
      //   and invoke reverse conversion (if necessary) to fill reading automatically.
    }

    /**
     * Sets the entry so that users can see it when the dialog is shown.
     * To make editing convenient, select all region.
     */
    void setEntry(Entry entry) {
      EditText wordEditText = EditText.class.cast(
          findViewById(R.id.user_dictionary_tool_word_register_dialog_word));
      wordEditText.setText(entry.getValue());
      EditText.class.cast(findViewById(R.id.user_dictionary_tool_word_register_dialog_reading))
          .setText(entry.getKey());
      Spinner posSpinner = Spinner.class.cast(
          findViewById(R.id.user_dictionary_tool_word_register_dialog_pos));
      int numItems = posSpinner.getCount();
      for (int i = 0; i < numItems; ++i) {
        if (PosItem.class.cast(posSpinner.getItemAtPosition(i)).posType == entry.getPos()) {
          posSpinner.setSelection(i);
          break;
        }
      }

      // Focus "word" field by default.
      wordEditText.requestFocus();
    }
  }

  interface DictionaryNameDialogListener {

    /**
     * Callback to be called when the positive button is clicked.
     * @param dictionaryName the text which is filled in EditText on the dialog.
     * @return result status of the executed command.
     */
    public Status onPositiveButtonClicked(String dictionaryName);
  }

  /**
   * Dialog implementation which has one edit box for dictionary name editing.
   */
  static class DictionaryNameDialog extends UserDictionaryBaseDialog {
    DictionaryNameDialog(Context context, int titleResourceId,
                         final DictionaryNameDialogListener listener,
                         OnDismissListener dismissListener,
                         ToastManager toastManager) {
      super(context, titleResourceId, R.layout.user_dictionary_tool_dictionary_name_dialog_view,
            new UserDictionaryBaseDialogListener() {
              @Override
              public Status onPositiveButtonClicked(View view) {
                return listener.onPositiveButtonClicked(
                    getText(view, R.id.user_dictionary_tool_dictionary_name_dialog_name));
              }
            },
            dismissListener, toastManager);
    }

    /**
     * Sets the dictionaryName so that users can see it when the dialog is shown.
     * To make editing convenient, select all region.
     */
    void setDictionaryName(String dictionaryName) {
      EditText editText = EditText.class.cast(
          findViewById(R.id.user_dictionary_tool_dictionary_name_dialog_name));
      editText.setText(dictionaryName);
      editText.selectAll();
    }
  }

  private static int DIALOG_WIDTH_THRESHOLD = 480;  // in dip.

  /** A map from PosType to the string resource id for i18n. */
  private static Map<PosType, Integer> POS_RESOURCE_MAP;
  static {
    EnumMap<PosType, Integer> map = new EnumMap<PosType, Integer>(PosType.class);
    map.put(PosType.NOUN, R.string.japanese_pos_noun);
    map.put(PosType.ABBREVIATION, R.string.japanese_pos_abbreviation);
    map.put(PosType.SUGGESTION_ONLY, R.string.japanese_pos_suggestion_only);
    map.put(PosType.PROPER_NOUN, R.string.japanese_pos_proper_noun);
    map.put(PosType.PERSONAL_NAME, R.string.japanese_pos_personal_name);
    map.put(PosType.FAMILY_NAME, R.string.japanese_pos_family_name);
    map.put(PosType.FIRST_NAME, R.string.japanese_pos_first_name);
    map.put(PosType.ORGANIZATION_NAME, R.string.japanese_pos_organization_name);
    map.put(PosType.PLACE_NAME, R.string.japanese_pos_place_name);
    map.put(PosType.SA_IRREGULAR_CONJUGATION_NOUN,
            R.string.japanese_pos_sa_irregular_conjugation_noun);
    map.put(PosType.ADJECTIVE_VERBAL_NOUN, R.string.japanese_pos_adjective_verbal_noun);
    map.put(PosType.NUMBER, R.string.japanese_pos_number);
    map.put(PosType.ALPHABET, R.string.japanese_pos_alphabet);
    map.put(PosType.SYMBOL, R.string.japanese_pos_symbol);
    map.put(PosType.EMOTICON, R.string.japanese_pos_emoticon);
    map.put(PosType.ADVERB, R.string.japanese_pos_adverb);
    map.put(PosType.PRENOUN_ADJECTIVAL, R.string.japanese_pos_prenoun_adjectival);
    map.put(PosType.CONJUNCTION, R.string.japanese_pos_conjunction);
    map.put(PosType.INTERJECTION, R.string.japanese_pos_interjection);
    map.put(PosType.PREFIX, R.string.japanese_pos_prefix);
    map.put(PosType.COUNTER_SUFFIX, R.string.japanese_pos_counter_suffix);
    map.put(PosType.GENERIC_SUFFIX, R.string.japanese_pos_generic_suffix);
    map.put(PosType.PERSON_NAME_SUFFIX, R.string.japanese_pos_person_name_suffix);
    map.put(PosType.PLACE_NAME_SUFFIX, R.string.japanese_pos_place_name_suffix);
    map.put(PosType.WA_GROUP1_VERB, R.string.japanese_pos_wa_group1_verb);
    map.put(PosType.KA_GROUP1_VERB, R.string.japanese_pos_ka_group1_verb);
    map.put(PosType.SA_GROUP1_VERB, R.string.japanese_pos_sa_group1_verb);
    map.put(PosType.TA_GROUP1_VERB, R.string.japanese_pos_ta_group1_verb);
    map.put(PosType.NA_GROUP1_VERB, R.string.japanese_pos_na_group1_verb);
    map.put(PosType.MA_GROUP1_VERB, R.string.japanese_pos_ma_group1_verb);
    map.put(PosType.RA_GROUP1_VERB, R.string.japanese_pos_ra_group1_verb);
    map.put(PosType.GA_GROUP1_VERB, R.string.japanese_pos_ga_group1_verb);
    map.put(PosType.BA_GROUP1_VERB, R.string.japanese_pos_ba_group1_verb);
    map.put(PosType.HA_GROUP1_VERB, R.string.japanese_pos_ha_group1_verb);
    map.put(PosType.GROUP2_VERB, R.string.japanese_pos_group2_verb);
    map.put(PosType.KURU_GROUP3_VERB, R.string.japanese_pos_kuru_group3_verb);
    map.put(PosType.SURU_GROUP3_VERB, R.string.japanese_pos_suru_group3_verb);
    map.put(PosType.ZURU_GROUP3_VERB, R.string.japanese_pos_zuru_group3_verb);
    map.put(PosType.RU_GROUP3_VERB, R.string.japanese_pos_ru_group3_verb);
    map.put(PosType.ADJECTIVE, R.string.japanese_pos_adjective);
    map.put(PosType.SENTENCE_ENDING_PARTICLE, R.string.japanese_pos_sentence_ending_particle);
    map.put(PosType.PUNCTUATION, R.string.japanese_pos_punctuation);
    map.put(PosType.FREE_STANDING_WORD, R.string.japanese_pos_free_standing_word);
    map.put(PosType.SUPPRESSION_WORD, R.string.japanese_pos_suppression_word);

    if (map.size() != PosType.values().length) {
      // There seems something unknown POS.
      throw new AssertionError();
    }
    POS_RESOURCE_MAP = Collections.unmodifiableMap(map);
  }

  /**
   * List of Japanese encodings to convert text for data importing.
   * These encodings are tries in the order, in other words, UTF-8 is the most high-prioritized
   * encoding.
   */
  private static final String[] JAPANESE_ENCODING_LIST = {
    "UTF-8", "EUC-JP", "ISO-2022-JP", "Shift_JIS", "UTF-16",
  };

  private UserDictionaryUtil() {
  }

  /**
   * Returns the text content of the view with the given resourceId.
   */
  private static String getText(View view, int resourceId) {
    view = view.getRootView();
    TextView textView = TextView.class.cast(view.findViewById(resourceId));
    return textView.getText().toString();
  }

  /**
   * Returns the PosType of the view with the given resourceId.
   */
  private static PosType getPos(View view, int resourceId) {
    view = view.getRootView();
    Spinner spinner = Spinner.class.cast(view.findViewById(resourceId));
    return PosItem.class.cast(spinner.getSelectedItem()).posType;
  }

  /**
   * Returns string resource id for the given {@code pos}.
   */
  static int getPosStringResourceId(PosType pos) {
    return POS_RESOURCE_MAP.get(pos);
  }

  /**
   * Returns the instance for Word Register Dialog.
   */
  static WordRegisterDialog createWordRegisterDialog(
      Context context, int titleResourceId, WordRegisterDialogListener listener,
      OnDismissListener dismissListener, ToastManager toastManager) {
    return new WordRegisterDialog(
        context, titleResourceId, listener, dismissListener, toastManager);
  }

  /**
   * Returns a new instance for Dictionary Name Dialog.
   */
  static DictionaryNameDialog createDictionaryNameDialog(
      Context context, int titleResourceId, DictionaryNameDialogListener listener,
      OnDismissListener dismissListener, ToastManager toastManager) {
    return new DictionaryNameDialog(
        context, titleResourceId, listener, dismissListener, toastManager);
  }

  /**
   * Returns a new instance for a dialog to select a file in a zipfile.
   */
  static Dialog createZipFileSelectionDialog(
      Context context, int titleResourceId,
      DialogInterface.OnClickListener positiveButtonListener,
      DialogInterface.OnClickListener negativeButtonListener,
      DialogInterface.OnCancelListener cancelListener,
      OnDismissListener dismissListener) {
    return createSimpleSpinnerDialog(
        context, titleResourceId, positiveButtonListener, negativeButtonListener,
        cancelListener, dismissListener);
  }

  /**
   * Returns a new instance for a dialog to select import destination.
   */
  static Dialog createImportDictionarySelectionDialog(
      Context context, int titleResourceId,
      DialogInterface.OnClickListener positiveButtonListener,
      DialogInterface.OnClickListener negativeButtonListener,
      DialogInterface.OnCancelListener cancelListener,
      OnDismissListener dismissListener) {
    return createSimpleSpinnerDialog(
        context, titleResourceId, positiveButtonListener, negativeButtonListener,
        cancelListener, dismissListener);
  }

  private static Dialog createSimpleSpinnerDialog(
      Context context, int titleResourceId,
      DialogInterface.OnClickListener positiveButtonListener,
      DialogInterface.OnClickListener negativeButtonListener,
      DialogInterface.OnCancelListener cancelListener,
      OnDismissListener dismissListener) {
    View view = LayoutInflater.from(context).inflate(
        R.layout.user_dictionary_tool_simple_spinner_dialog_view, null);
    AlertDialog dialog = new AlertDialog.Builder(context)
        .setTitle(titleResourceId)
        .setView(view)
        .setPositiveButton(android.R.string.yes, positiveButtonListener)
        .setNegativeButton(android.R.string.cancel, negativeButtonListener)
        .setOnCancelListener(cancelListener)
        .setCancelable(true)
        .create();
    dialog.setOnDismissListener(dismissListener);
    return dialog;
  }

  /**
   * Sets the parameter to show IME when the given dialog is shown.
   */
  static void showInputMethod(Dialog dialog) {
    dialog.getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_VISIBLE);
  }

  /**
   * The default size of dialog looks not good on some devices, such as tablets.
   * So, modify the dialog size to make the look better.
   *
   * The current storategy is:
   * - if the display is small, we tries to fill the display width by the dialog.
   * - otherwise, set w480dip to the window.
   */
  static void setDialogWindowSize(Dialog dialog) {
    DisplayMetrics metrics = dialog.getContext().getResources().getDisplayMetrics();
    LayoutParams params = dialog.getWindow().getAttributes();
    float dialogWidthThreshold = DIALOG_WIDTH_THRESHOLD * metrics.scaledDensity;
    if (metrics.widthPixels > dialogWidthThreshold) {
      params.width = (int) dialogWidthThreshold;
    } else {
      params.width = LayoutParams.MATCH_PARENT;
    }
    if (dialog instanceof UserDictionaryBaseDialog) {
      // If the ScrollView hack is used for the dialog to make it scrollable,
      // it is necessary to set its height parameter MATCH_PARENT as a part of the hack.
      params.height = LayoutParams.MATCH_PARENT;
    }
    dialog.getWindow().setAttributes(params);
  }

  /**
   * Returns the {@code String} instance with detecting the Japanese encoding.
   * @throws UnsupportedEncodingException if it fails to detect the encoding.
   */
  static String toStringWithEncodingDetection(ByteBuffer buffer)
      throws UnsupportedEncodingException {
    for (String encoding : JAPANESE_ENCODING_LIST) {
      buffer.position(0);
      try {
        Charset charset = Charset.forName(encoding);
        CharBuffer result = charset.newDecoder()
           .onMalformedInput(CodingErrorAction.REPORT)
           .onUnmappableCharacter(CodingErrorAction.REPORT)
           .decode(buffer);
        String str = result.toString();
        if (str.length() > 0 && str.charAt(0) == 0xFEFF) {
          // Remove leading BOM if necessary.
          str = str.substring(1);
        }
        return str;
      } catch (Exception e) {
        // Ignore exceptions, and retry next encoding.
      }
    }

    throw new UnsupportedEncodingException("Failed to detect encoding");
  }

  /**
   * Reads the text file with detecting the file encoding.
   */
  static String readFromFile(String path) throws IOException {
    RandomAccessFile file = new RandomAccessFile(path, "r");
    boolean succeeded = false;
    try {
      String result = readFromFileInternal(file);
      succeeded = true;
      return result;
    } finally {
      MozcUtil.close(file, !succeeded);
    }
  }

  private static String readFromFileInternal(RandomAccessFile file) throws IOException {
    FileChannel channel = file.getChannel();
    boolean succeeded = false;
    try {
      String result = toStringWithEncodingDetection(
          channel.map(MapMode.READ_ONLY, 0, channel.size()));
      succeeded = true;
      return result;
    } finally {
      MozcUtil.close(channel, !succeeded);
    }
  }

  /**
   * Returns import source uri based on the Intent.
   */
  static Uri getImportUri(Intent intent) {
    String action = intent.getAction();
    if (Intent.ACTION_SEND.equals(action)) {
      return intent.getParcelableExtra(Intent.EXTRA_STREAM);
    }
    if (Intent.ACTION_VIEW.equals(action)) {
      return intent.getData();
    }
    return null;
  }

  /**
   * Returns a name for a new dictionary based on import URI.
   */
  static String generateDictionaryNameByUri(Uri importUri, List<String> dictionaryNameList) {
    String name = importUri.getLastPathSegment();

    // Strip extension
    {
      int index = name.lastIndexOf('.');
      if (index > 0) {
        // Keep file path beginning with '.'.
        name = name.substring(0, index);
      }
    }

    if (!dictionaryNameList.contains(name)) {
      // No-dupped dictionary name.
      return name;
    }

    // The names extracted from uri is dupped. So look for alternative names by adding
    // number suffix, such as "pathname (1)".
    int suffix = 1;
    while (true) {
      String candidate = name + " (" + suffix + ")";
      if (!dictionaryNameList.contains(candidate)) {
        return candidate;
      }
      // The limit of the number of dictionaries is much smaller than Integer.MAX_VALUE,
      // so this while loop should stop eventually.
      ++suffix;
    }
  }
}
