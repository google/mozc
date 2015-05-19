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

/**
 * @fileoverview This file contains NaclMozc option page implementation.
 *
 * TODO(horo): Write tests of option_page.js.
 *
 */

'use strict';

/**
 * Default value of configuration.
 * These values must be same as defined in config.proto.
 * @const
 * @type {!Object.<string, string|number|boolean>}
 * @private
 */
mozc.DEFAULT_CONFIG_ = {
  'verbose_level': 0,
  'incognito_mode': false,
  'check_default': true,
  'presentation_mode': false,
  'preedit_method': 'ROMAN',
  'session_keymap': 'KOTOERI',
  'custom_keymap_table': '',
  'punctuation_method': 'KUTEN_TOUTEN',
  'symbol_method': 'CORNER_BRACKET_MIDDLE_DOT',
  'space_character_form': 'FUNDAMENTAL_INPUT_MODE',
  'use_keyboard_to_change_preedit_method': false,
  'history_learning_level': 'DEFAULT_HISTORY',
  'selection_shortcut': 'SHORTCUT_123456789',
  'use_auto_ime_turn_off': true,
  'shift_key_mode_switch': 'ASCII_INPUT_MODE',
  'numpad_character_form': 'NUMPAD_HALF_WIDTH',
  'use_auto_conversion': false,
  'auto_conversion_key': 13,
  'yen_sign_character': 'YEN_SIGN',
  'use_japanese_layout': false,
  'use_date_conversion': true,
  'use_single_kanji_conversion': true,
  'use_symbol_conversion': true,
  'use_number_conversion': true,
  'use_emoticon_conversion': true,
  'use_calculator': true,
  'use_t13n_conversion': true,
  'use_zip_code_conversion': true,
  'use_spelling_correction': true,
  'use_history_suggest': true,
  'use_dictionary_suggest': true,
  'use_realtime_conversion': true,
  'suggestions_size': 3,
  'allow_cloud_handwriting': false,
  'upload_usage_stats': false
};

/**
 * Option title information data.
 * @const
 * @type {!Array.<!Object>}
 * @private
 */
mozc.OPTION_TITLES_ = [
  {
    id: 'settings_title',
    name: chrome.i18n.getMessage('configSettingsTitle')
  },
  {
    id: 'basics_title',
    name: chrome.i18n.getMessage('configBasicsTitle')
  },
  {
    id: 'input_assistance_title',
    name: chrome.i18n.getMessage('configInputAssistanceTitle')
  },
  {
    id: 'suggest_title',
    name: chrome.i18n.getMessage('configSuggestTitle')
  },
  {
    id: 'privacy_title',
    name: chrome.i18n.getMessage('configPrivacyTitle')
  },
  {
    id: 'credits_title',
    name: chrome.i18n.getMessage('configCreditsDescription')
  },
  {
    id: 'oss_credits_title',
    name: chrome.i18n.getMessage('configOssCreditsDescription')
  },
  {
    id: 'sync_advanced_settings_title',
    name: chrome.i18n.getMessage('configSyncAdvancedSettings')
  },
  {
    id: 'sync_description',
    name: chrome.i18n.getMessage('configSyncDescription')
  },
  {
    id: 'sync_title',
    name: chrome.i18n.getMessage('configSyncTitle')
  },
  {
    id: 'clear_history_title',
    name: chrome.i18n.getMessage('configClearHistoryTitle')
  },
  {
    id: 'clear_history_conversion_history_label',
    name: chrome.i18n.getMessage('configClearHistoryConversionHistory')
  },
  {
    id: 'clear_history_suggestion_history_label',
    name: chrome.i18n.getMessage('configClearHistorySuggestionHistory')
  },
  {
    id: 'dictionary_tool_title',
    name: chrome.i18n.getMessage('configDictionaryToolTitle')
  },
  {
    id: 'dictionary_tool_description',
    name: chrome.i18n.getMessage('configDictionaryToolDescription')
  },
  {
    id: 'dictionary_tool_page_title',
    name: chrome.i18n.getMessage('dictionaryToolPageTitle')
  },
  {
    id: 'dictionary_tool_reading_title',
    name: chrome.i18n.getMessage('dictionaryToolReadingTitle')
  },
  {
    id: 'dictionary_tool_word_title',
    name: chrome.i18n.getMessage('dictionaryToolWordTitle')
  },
  {
    id: 'dictionary_tool_category_title',
    name: chrome.i18n.getMessage('dictionaryToolCategoryTitle')
  },
  {
    id: 'dictionary_tool_comment_title',
    name: chrome.i18n.getMessage('dictionaryToolCommentTitle')
  }
];

/**
 * Option checkbox information data.
 * @const
 * @type {!Array.<!Object>}
 * @private
 */
mozc.OPTION_CHECKBOXES_ = [
  {
    id: 'upload_usage_stats',
    configId: 'upload_usage_stats',
    configType: 'general_config',
    name: chrome.i18n.getMessage('configUploadUsageStats'),
    // Disable 'upload_usage_stats' checkbox in OSS Mozc.
    disabled: (chrome.i18n.getMessage('branding') == 'Mozc')
  },
  {
    id: 'incognito_mode',
    configId: 'incognito_mode',
    name: chrome.i18n.getMessage('configIncognitoMode')
  },
  {
    id: 'use_auto_ime_turn_off',
    configId: 'use_auto_ime_turn_off',
    name: chrome.i18n.getMessage('configUseAutoImeTurnOff')
  },
  {
    id: 'use_history_suggest',
    configId: 'use_history_suggest',
    name: chrome.i18n.getMessage('configUseHistorySuggest')
  },
  {
    id: 'use_dictionary_suggest',
    configId: 'use_dictionary_suggest',
    name: chrome.i18n.getMessage('configUseDictionarySuggest')
  }
];

/**
 * Option selection information data.
 * @const
 * @type {!Array.<!Object>}
 * @private
 */
mozc.OPTION_SELECTIONS_ = [
  {
    id: 'punctuation_method',
    configId: 'punctuation_method',
    name: chrome.i18n.getMessage('configPunctuationMethod'),
    items: [
      {name: '\u3001\u3002', value: 'KUTEN_TOUTEN'},  // '、。'
      {name: '\uFF0C\uFF0E', value: 'COMMA_PERIOD'},  // '，．'
      {name: '\u3001\uFF0E', value: 'KUTEN_PERIOD'},  // '、．'
      {name: '\uFF0C\u3002', value: 'COMMA_TOUTEN'}  // '，。'
    ]
  },
  {
    id: 'preedit_method',
    configId: 'preedit_method',
    name: chrome.i18n.getMessage('configPreeditMethod'),
    items: [
      {
        name: chrome.i18n.getMessage('configPreeditMethodRomaji'),
        value: 'ROMAN'
      },
      {name: chrome.i18n.getMessage('configPreeditMethodKana'), value: 'KANA'}
    ]
  },
  {
    id: 'symbol_method',
    configId: 'symbol_method',
    name: chrome.i18n.getMessage('configSymbolMethod'),
    items: [
      // '「」・'
      {name: '\u300C\u300D\u30FB', value: 'CORNER_BRACKET_MIDDLE_DOT'},
      {name: '\uFF3B\uFF3D\uFF0F', value: 'SQUARE_BRACKET_SLASH'},  // '［］／'
      {name: '\u300C\u300D\uFF0F', value: 'CORNER_BRACKET_SLASH'},  // '「」／'
      {name: '\uFF3B\uFF3D\u30FB', value: 'SQUARE_BRACKET_MIDDLE_DOT'}  // '［］・'
    ]
  },
  {
    id: 'space_character_form',
    configId: 'space_character_form',
    name: chrome.i18n.getMessage('configSpaceCharacterForm'),
    items: [
      {
        name: chrome.i18n.getMessage('configSpaceCharacterFormFollow'),
        value: 'FUNDAMENTAL_INPUT_MODE'
      },
      {
        name: chrome.i18n.getMessage('configSpaceCharacterFormFull'),
        value: 'FUNDAMENTAL_FULL_WIDTH'
      },
      {
        name: chrome.i18n.getMessage('configSpaceCharacterFormHalf'),
        value: 'FUNDAMENTAL_HALF_WIDTH'
      }
    ]
  },
  {
    id: 'selection_shortcut',
    configId: 'selection_shortcut',
    name: chrome.i18n.getMessage('configSelectionShortcut'),
    items: [
      {
        name: chrome.i18n.getMessage('configSelectionShortcutNo'),
        value: 'NO_SHORTCUT'
      },
      {name: '1 -- 9', value: 'SHORTCUT_123456789'},
      {name: 'A -- L', value: 'SHORTCUT_ASDFGHJKL'}
    ]
  },
  {
    id: 'shift_key_mode_switch',
    configId: 'shift_key_mode_switch',
    name: chrome.i18n.getMessage('configShiftKeyModeSwitch'),
    items: [
      {
        name: chrome.i18n.getMessage('configShiftKeyModeSwitchOff'),
        value: 'OFF'
      },
      {
        name: chrome.i18n.getMessage('configShiftKeyModeSwitchAlphanumeric'),
        value: 'ASCII_INPUT_MODE'
      },
      {
        name: chrome.i18n.getMessage('configShiftKeyModeSwitchKatakana'),
        value: 'KATAKANA_INPUT_MODE'
      }
    ]
  },
  {
    id: 'session_keymap',
    configId: 'session_keymap',
    name: chrome.i18n.getMessage('configSessionKeymap'),
    items: [
      {name: chrome.i18n.getMessage('configSessionKeymapAtok'), value: 'ATOK'},
      {
        name: chrome.i18n.getMessage('configSessionKeymapMsIme'),
        value: 'MSIME'
      },
      {
        name: chrome.i18n.getMessage('configSessionKeymapKotoeri'),
        value: 'KOTOERI'
      }
    ]
  }
];

/**
 * Option number information data.
 * @const
 * @type {!Array.<!Object>}
 * @private
 */
mozc.OPTION_NUMBERS_ = [
  {
    id: 'suggestions_size',
    configId: 'suggestions_size',
    name: chrome.i18n.getMessage('configSuggestionsSize'),
    min: 1,
    max: 9
  }
];

/**
 * Option button information data.
 * @const
 * @type {!Array.<!Object>}
 * @private
 */
mozc.OPTION_BUTTONS_ = [
  {
    id: 'clear_history_open',
    name: chrome.i18n.getMessage('configClearHistory')
  },
  {
    id: 'clear_history_close'
  },
  {
    id: 'clear_history_ok',
    name: chrome.i18n.getMessage('configClearHistoryOkButton')
  },
  {
    id: 'clear_history_cancel',
    name: chrome.i18n.getMessage('configClearHistoryCancelButton')
  },
  {
    id: 'sync_config_cancel',
    name: chrome.i18n.getMessage('configSyncConfigCancel')
  },
  {
    id: 'sync_config_ok',
    name: chrome.i18n.getMessage('configSyncConfigOk')
  },
  {
    id: 'sync_config_close'
  },
  {
    id: 'sync_toggle_button',
    name: chrome.i18n.getMessage('configSyncStartSync')
  },
  {
    id: 'sync_customization_button',
    name: chrome.i18n.getMessage('configSyncCustomization')
  },
  {
    id: 'dictionary_tool_open_button',
    name: chrome.i18n.getMessage('configDictionaryToolButton')
  },
  {
    id: 'create_dictionary_button',
    name: chrome.i18n.getMessage('dictionaryToolCreateButton')
  },
  {
    id: 'rename_dictionary_button',
    name: chrome.i18n.getMessage('dictionaryToolRenameButton')
  },
  {
    id: 'delete_dictionary_button',
    name: chrome.i18n.getMessage('dictionaryToolDeleteButton')
  },
  {
    id: 'export_dictionary_button',
    name: chrome.i18n.getMessage('dictionaryToolExportButton')
  },
  {
    id: 'dictionary_tool_done_button',
    name: chrome.i18n.getMessage('dictionaryToolDoneButton')
  },
  {
    id: 'dictionary_tool_close'
  }
];

/**
 * Option sync checkbox information data.
 * @const
 * @type {!Array.<!Object>}
 * @private
 */
mozc.OPTION_SYNC_CHECKBOXES_ = [
  {
    id: 'sync_settings',
    syncConfigId: 'use_config_sync',
    name: chrome.i18n.getMessage('configUseConfigSync')
  },
  {
    id: 'sync_user_dictionary',
    configId: 'use_user_dictionary_sync',
    name: chrome.i18n.getMessage('configUseUserDictionarySync')
  }
];

/**
 * Dictionary tool error messages.
 * @const
 * @type {!Object.<string, string>}
 * @private
 */
mozc.DICTIONARY_TOOL_STATUS_ERRORS_ = {
  'FILE_NOT_FOUND':
      chrome.i18n.getMessage('dictionaryToolStatuErrorFileNotFound'),
  'INVALID_FILE_FORMAT':
      chrome.i18n.getMessage('dictionaryToolStatuErrorInvalidFileFormat'),
  'FILE_SIZE_LIMIT_EXCEEDED':
      chrome.i18n.getMessage('dictionaryToolStatuErrorFileSizeLimitExceeded'),
  'DICTIONARY_SIZE_LIMIT_EXCEEDED':
      chrome.i18n.getMessage(
          'dictionaryToolStatuErrorDictionarySizeLimitExceeded'),
  'ENTRY_SIZE_LIMIT_EXCEEDED':
      chrome.i18n.getMessage('dictionaryToolStatuErrorEntrySizeLimitExceeded'),
  'DICTIONARY_NAME_EMPTY':
      chrome.i18n.getMessage('dictionaryToolStatuErrorDictionaryNameEmpty'),
  'DICTIONARY_NAME_TOO_LONG':
      chrome.i18n.getMessage('dictionaryToolStatuErrorDictionaryNameTooLong'),
  'DICTIONARY_NAME_CONTAINS_INVALID_CHARACTER':
      chrome.i18n.getMessage(
          'dictionaryToolStatuErrorDictionaryNameContainsInvalidCharacter'),
  'DICTIONARY_NAME_DUPLICATED':
      chrome.i18n.getMessage(
          'dictionaryToolStatuErrorDictionaryNameDuplicated'),
  'READING_EMPTY':
      chrome.i18n.getMessage('dictionaryToolStatuErrorReadingEmpty'),
  'READING_TOO_LONG':
      chrome.i18n.getMessage('dictionaryToolStatuErrorReadingTooLong'),
  'READING_CONTAINS_INVALID_CHARACTER':
      chrome.i18n.getMessage(
          'dictionaryToolStatuErrorReadingContainsInvalidCharacter'),
  'WORD_EMPTY': chrome.i18n.getMessage('dictionaryToolStatuErrorWordEmpty'),
  'WORD_TOO_LONG':
      chrome.i18n.getMessage('dictionaryToolStatuErrorWordTooLong'),
  'WORD_CONTAINS_INVALID_CHARACTER':
      chrome.i18n.getMessage(
          'dictionaryToolStatuErrorWordContainsInvalidCharacter'),
  'IMPORT_TOO_MANY_WORDS':
      chrome.i18n.getMessage('dictionaryToolStatuErrorImportTooManyWords'),
  'IMPORT_INVALID_ENTRIES':
      chrome.i18n.getMessage('dictionaryToolStatuErrorImportInvalidEntries'),
  'NO_UNDO_HISTORY':
      chrome.i18n.getMessage('dictionaryToolStatuErrorNoUndoHistory')
};

/**
 * The refresh interval of sync status in milliseconds.
 * @type {number}
 * @private
 */
mozc.SYNC_STATUS_REFRESH_INTERVAL_ = 3000;

/**
 * Cloud sync feature is only available in official dev channel.
 * @type {boolean}
 * @private
 */
mozc.ENABLE_CLOUD_SYNC_ = mozc.VERSION_.DEV && mozc.VERSION_.OFFICIAL;


/**
 * In Official Stable NaCl Mozc we use simple UI version dictionary tool.
 * In simple UI version the user can only have one user dictionary named
 * "user dictionary".
 * @type {boolean}
 * @private
 */
mozc.ENABLE_SIMPLE_DICTIONARY_TOOL_ = !mozc.VERSION_.DEV &&
                                      mozc.VERSION_.OFFICIAL;

/**
 * An empty constructor.
 * @param {!mozc.NaclMozc} naclMozc NaCl Mozc.
 * @param {!Window} optionWindow Window object of the option page.
 * @constructor
 */
mozc.OptionPage = function(naclMozc, optionWindow) {
  /**
   * NaclMozc object.
   * This value will be null when the option page is unloaded.
   * @type {mozc.NaclMozc}
   * @private
   */
  this.naclMozc_ = naclMozc;

  /**
   * Window object of the option page.
   * This value will be null when the option page is unloaded.
   * @type {Window}
   * @private
   */
  this.window_ = optionWindow;

  /**
   * Document object of the option page.
   * This value will be null when the option page is unloaded.
   * @type {Document}
   * @private
   */
  this.document_ = optionWindow.document;

  /**
   * Console object of the option page.
   * This value will be null when the option page is unloaded.
   * @type {Object}
   * @private
   */
  this.console_ = optionWindow.console;

  /**
   * Timer ID which is used to refresh sync status.
   * @type {undefined|number}
   * @private
   */
  this.timeoutID_ = undefined;

  /**
   * The last synced time which is acquired from NaCl module using
   * getCloudSyncStatus().
   * @type {number}
   * @private
   */
  this.lastSyncedTimestamp_ = 0;

  /**
   * User dictionary session id created in NaCl module. It is set when
   * dictionary tool is opened.
   * @type {string}
   * @private
   */
  this.userDictionarySessionId_ = '';

  /**
   * Pos type list. (example: {type: 'NOUN', name: '名詞'})
   * @type {!Array.<{type: string, name: string}>}
   * @private
   */
  this.posList_ = [];

  /**
   * Pos type name to display name map. (example: 'NOUN' -> '名詞')
   * @type {!Object.<string, string>}
   * @private
   */
  this.posNameMap_ = {};

  /**
   * Whether cloud sync is enabled or not.
   * @type {boolean}
   * @private
   */
  this.syncEnabled_ = false;

  /**
   * Stack of Esc key handlers. An Esc key handler is pushed to it when an
   * dialog　box is opend and the handler will be popped when the dialog box is
   * closed. It is used to close the dialog when the user presses Esc key.
   * @type {!Array.<!Function>}
   * @private
   */
  this.escapeKeyHandlers_ = [];
};

/**
 * Initializes the option page.
 */
mozc.OptionPage.prototype.initialize = function() {
  if (!this.naclMozc_) {
    console.error('The option page is already unloaded.');
    return;
  }
  this.initPages_();
  this.naclMozc_.callWhenInitialized((function() {
    this.naclMozc_.getPosList((function(message) {
      this.posList_ = message['posList'];
      var new_category_select =
        this.document_.getElementById('dictionary_tool_category_new_select');
      for (var i = 0; i < this.posList_.length; ++i) {
        this.posNameMap_[this.posList_[i]['type']] = this.posList_[i]['name'];
        new_category_select.appendChild(
            this.createOptionElement_(this.posList_[i]['name'],
                                      this.posList_[i]['type']));
      }
      this.naclMozc_.getConfig(this.onConfigLoaded_.bind(this));
    }).bind(this));
    if (mozc.ENABLE_CLOUD_SYNC_) {
      this.updateSyncStatus_();
    }
    // Outputs the version information to JavaScript console.
    this.naclMozc_.getVersionInfo((function(message) {
      this.console_.log(JSON.stringify(message));
    }).bind(this));
  }).bind(this));
};

/**
 * Unloads the option page.
 */
mozc.OptionPage.prototype.unload = function() {
  if (this.timeoutID_ != undefined) {
    clearTimeout(this.timeoutID_);
    this.timeoutID_ = undefined;
  }
  this.window_ = null;
  this.naclMozc_ = null;
  this.document_ = null;
  this.console_ = null;
};

/**
 * Gets whether the option page is unloaded.
 * @return {boolean} Whether the option page is already unloaded.
 */
mozc.OptionPage.prototype.isUnloaded = function() {
  return this.naclMozc_ == null;
};

/**
 * Creates an Option element.
 * @param {string} name Name of the option.
 * @param {string} value Value of the option.
 * @return {!Element} Created Option element.
 * @private
 */
mozc.OptionPage.prototype.createOptionElement_ = function(name, value) {
  var option = this.document_.createElement('option');
  option.appendChild(this.document_.createTextNode(name));
  option.setAttribute('value', value);
  return option;
};

/**
 * Initializes the page.
 * @private
 */
mozc.OptionPage.prototype.initPages_ = function() {
  this.document_.title = chrome.i18n.getMessage('configSettingsTitle');
  for (var i = 0; i < mozc.OPTION_TITLES_.length; ++i) {
    var optionTitle = mozc.OPTION_TITLES_[i];
    this.document_.getElementById(optionTitle.id).innerHTML =
        optionTitle.name;
  }

  // A checkbox (id:'CHECK_BOX_ID') is in a DIV (id:'CHECK_BOX_ID_div') and has
  // a label (id:'CHECK_BOX_ID_label').
  // <div id='CHECK_BOX_ID_div'>
  //   <span>
  //     <input type='checkbox' id='CHECK_BOX_ID'>
  //     <span>
  //       <label for='CHECK_BOX_ID' id='CHECK_BOX_ID_label'>...</label>
  //     </span>
  //   </span>
  // </div>
  for (var i = 0; i < mozc.OPTION_CHECKBOXES_.length; ++i) {
    var optionCheckbox = mozc.OPTION_CHECKBOXES_[i];
    if (optionCheckbox.disabled) {
      this.document_.getElementById(optionCheckbox.id + '_div').style
          .visibility = 'hidden';
    }
    this.document_.getElementById(optionCheckbox.id + '_label').innerHTML =
        optionCheckbox.name;
    this.document_.getElementById(optionCheckbox.id).addEventListener(
        'change', this.saveConfig_.bind(this), true);
  }

  // A selection (id:'SELECTION_ID') is in a DIV (id:'SELECTION_ID_div') and has
  // a label (id:'SELECTION_ID_label')
  // <div id='SELECTION_ID_div'>
  //   <span class='controlled-setting-with-label'>
  //     <span class='selection-label' id='SELECTION_ID_label'>...</span>
  //     <select id='SELECTION_ID'></select>
  //   </span>
  // </div>
  for (var i = 0; i < mozc.OPTION_SELECTIONS_.length; ++i) {
    var optionSelection = mozc.OPTION_SELECTIONS_[i];
    this.document_.getElementById(optionSelection.id + '_label').innerHTML =
        optionSelection.name;
    var selectionElement = this.document_.getElementById(optionSelection.id);
    selectionElement.innerHTML = optionSelection.name;
    while (selectionElement.hasChildNodes()) {
      selectionElement.removeChild(selectionElement.firstChild);
    }
    for (var j = 0; j < optionSelection.items.length; ++j) {
      selectionElement.appendChild(this.createOptionElement_(
          optionSelection.items[j].name,
          optionSelection.items[j].value));
    }
    selectionElement.addEventListener(
        'change', this.saveConfig_.bind(this), true);
  }

  // We use a select element for number selection.
  // A selection (id:'NUMBER_ID') is in a DIV (id:'NUMBER_ID_div') and has a
  // label (id:'NUMBER_ID_label')
  // <div id='NUMBER_ID_div'>
  //   <span class='controlled-setting-with-label'>
  //     <span class='selection-label' id='NUMBER_ID_label'>...</span>
  //     <select id='NUMBER_ID'></select>
  //   </span>
  // </div>
  for (var i = 0; i < mozc.OPTION_NUMBERS_.length; ++i) {
    var optionNumber = mozc.OPTION_NUMBERS_[i];
    this.document_.getElementById(optionNumber.id + '_label').innerHTML =
        optionNumber.name;
    var selectionElement = this.document_.getElementById(optionNumber.id);
    selectionElement.innerHTML = optionNumber.name;
    while (selectionElement.hasChildNodes()) {
      selectionElement.removeChild(selectionElement.firstChild);
    }
    for (var j = optionNumber.min; j <= optionNumber.max; ++j) {
      selectionElement.appendChild(this.createOptionElement_(j, j));
    }
    selectionElement.addEventListener(
        'change', this.saveConfig_.bind(this), true);
  }

  // A button (id:'BUTTON_ID') is in a DIV (id:'BUTTON_ID_div').
  // <div id='BUTTON_ID_div'>
  //   <span class='controlled-setting-with-label'>
  //     <input type='button' id='BUTTON_ID' value='...'>
  //   </span>
  // </div>
  for (var i = 0; i < mozc.OPTION_BUTTONS_.length; ++i) {
    var optionButton = mozc.OPTION_BUTTONS_[i];
    var buttonElement = this.document_.getElementById(optionButton.id);
    if (optionButton.name) {
      buttonElement.value = optionButton.name;
    }
    buttonElement.addEventListener('click',
                                   this.onButtonClick_.bind(this,
                                                            optionButton.id),
                                   true);
  }

  // A sync checkbox (id:'CHECK_BOX_ID') has a label (id:'CHECK_BOX_ID_label').
  // <span>
  //   <input type='checkbox' id='CHECK_BOX_ID'>
  //   <span>
  //     <label for='CHECK_BOX_ID' id='CHECK_BOX_ID_label'>...</label>
  //   </span>
  // </span>
  for (var i = 0; i < mozc.OPTION_SYNC_CHECKBOXES_.length; ++i) {
    var optionCheckbox = mozc.OPTION_SYNC_CHECKBOXES_[i];
    this.document_.getElementById(optionCheckbox.id + '_label').innerHTML =
        optionCheckbox.name;
  }

  this.document_.getElementById('user_dictionary_select').addEventListener(
      'change', this.onDictionarySelectChanged_.bind(this), true);
  this.document_.getElementById('dictionary_tool_reading_new_input')
      .addEventListener(
        'blur', this.onDictionaryNewEntryLostFocus_.bind(this), true);
  this.document_.getElementById('dictionary_tool_word_new_input')
      .addEventListener(
        'blur', this.onDictionaryNewEntryLostFocus_.bind(this), true);
  this.document_.getElementById('dictionary_tool_category_new_select')
      .addEventListener(
        'blur', this.onDictionaryNewEntryLostFocus_.bind(this), true);
  this.document_.getElementById('dictionary_tool_comment_new_input')
      .addEventListener(
        'blur', this.onDictionaryNewEntryLostFocus_.bind(this), true);

  // Removes cloud_sync_div if cloud sync is not enabled.
  if (!mozc.ENABLE_CLOUD_SYNC_) {
    this.document_.getElementById('settings_div').removeChild(
      this.document_.getElementById('cloud_sync_div'));
  }

  // Hides menu of user dictionary if simple dictionary tool is enabled.
  if (mozc.ENABLE_SIMPLE_DICTIONARY_TOOL_) {
    this.document_.getElementById('dictionary_tool_menu_span').style.display =
        'none';
  }

  this.document_.addEventListener(
      'keydown', this.onKeyDown_.bind(this), false);
};

/**
 * Called when the configuration is loaded.
 * @param {Object} response Response data of GET_CONFIG from NaCl module.
 * @private
 */
mozc.OptionPage.prototype.onConfigLoaded_ = function(response) {
  if (this.isUnloaded()) {
    console.error('The option page is already unloaded.');
    return;
  }
  var config = response['output']['config'];

  // Custom keymap option is available only when custom_keymap_table exists or
  // session_keymap is already set to 'CUSTOM'.
  // This is because the user can't edit custom keymap table in NaCl Mozc.
  // It can be downloaded form the sync server if the user has already uploaded
  // from other platform's Mozc.
  var session_keymap_select = this.document_.getElementById('session_keymap');
  var session_keymap_custom =
      this.document_.getElementById('session_keymap_custom');
  if (config['custom_keymap_table'] || config['session_keymap'] == 'CUSTOM') {
    if (!session_keymap_custom) {
      session_keymap_custom = this.createOptionElement_(
          chrome.i18n.getMessage('configSessionKeymapCustom'),
          'CUSTOM');
      session_keymap_custom.id = 'session_keymap_custom';
      session_keymap_select.appendChild(session_keymap_custom);
    }
  } else if (session_keymap_custom) {
    session_keymap_select.removeChild(session_keymap_custom);
  }

  for (var i = 0; i < mozc.OPTION_CHECKBOXES_.length; ++i) {
    var optionCheckbox = mozc.OPTION_CHECKBOXES_[i];
    if (optionCheckbox.disabled) {
      continue;
    }
    var value = false;
    if (optionCheckbox.configType == 'general_config') {
      value =
          ((config['general_config'] != undefined) &&
           (config['general_config'][optionCheckbox.configId] != undefined)) ?
              config['general_config'][optionCheckbox.configId] :
              mozc.DEFAULT_CONFIG_[optionCheckbox.configId];
    } else {
      value = (config[optionCheckbox.configId] != undefined) ?
                  config[optionCheckbox.configId] :
                  mozc.DEFAULT_CONFIG_[optionCheckbox.configId];
    }
    this.document_.getElementById(optionCheckbox.id).checked = value;
  }
  for (var i = 0; i < mozc.OPTION_SELECTIONS_.length; ++i) {
    var optionSelection = mozc.OPTION_SELECTIONS_[i];
    var value = (config[optionSelection.configId] != undefined) ?
                config[optionSelection.configId] :
                mozc.DEFAULT_CONFIG_[optionSelection.configId];
    this.document_.getElementById(optionSelection.id).value = value;
  }
  for (var i = 0; i < mozc.OPTION_NUMBERS_.length; ++i) {
    var optionNumber = mozc.OPTION_NUMBERS_[i];
    var value = (config[optionNumber.configId] != undefined) ?
                config[optionNumber.configId] :
                mozc.DEFAULT_CONFIG_[optionNumber.configId];
    this.document_.getElementById(optionNumber.id).value = value;
  }

  this.syncEnabled_ = !!config['sync_config'];

  this.document_.body.style.visibility = 'visible';
};

/**
 * Saves the configuration.
 * @private
 */
mozc.OptionPage.prototype.saveConfig_ = function() {
  if (this.isUnloaded()) {
    console.error('The option page is already unloaded.');
    return;
  }

  this.naclMozc_.getConfig((function(response) {
    var config = response['output']['config'];
    for (var i = 0; i < mozc.OPTION_CHECKBOXES_.length; ++i) {
      var optionCheckbox = mozc.OPTION_CHECKBOXES_[i];
      if (optionCheckbox.disabled) {
        continue;
      }
      if (optionCheckbox.configType == 'general_config') {
        if (config['general_config'] == undefined) {
          config['general_config'] = {};
        }
        config['general_config'][optionCheckbox.configId] =
            this.document_.getElementById(optionCheckbox.id).checked;
      } else {
        config[optionCheckbox.configId] =
            this.document_.getElementById(optionCheckbox.id).checked;
      }
    }
    for (var i = 0; i < mozc.OPTION_NUMBERS_.length; ++i) {
      var optionNumber = mozc.OPTION_NUMBERS_[i];
      config[optionNumber.configId] =
          Number(this.document_.getElementById(optionNumber.id).value);
    }
    for (var i = 0; i < mozc.OPTION_SELECTIONS_.length; ++i) {
      var optionSelection = mozc.OPTION_SELECTIONS_[i];
      config[optionSelection.configId] =
          this.document_.getElementById(optionSelection.id).value;
    }
    this.console_.log(config);
    this.naclMozc_.setConfig(
        config,
        (function() {this.naclMozc_.sendReload()}).bind(this));
  }).bind(this));
};

/**
 * Called when the user presses a key.
 * @param {Event} event Event object passed from browser.
 * @private
 */
mozc.OptionPage.prototype.onKeyDown_ = function(event) {
  if (event.keyCode == 27) {  // Escape
    if (this.escapeKeyHandlers_.length) {
      this.escapeKeyHandlers_[this.escapeKeyHandlers_.length - 1]();
    }
  }
};

/**
 * Enables the dom element which is specified by id in the option page.
 * @param {string} id The element ID.
 * @private
 */
mozc.OptionPage.prototype.enableElementById_ = function(id) {
  this.document_.getElementById(id).disabled = false;
};

/**
 * Disables the dom element which is specified by id in the option page.
 * @param {string} id The element ID.
 * @private
 */
mozc.OptionPage.prototype.disableElementById_ = function(id) {
  this.document_.getElementById(id).disabled = true;
};

/**
 * Shows the overlay element which is specified by id in the option page.
 * @param {string} id The element ID.
 * @private
 */
mozc.OptionPage.prototype.showOverlayElementById_ = function(id) {
  this.freezeMainDiv_();
  this.document_.getElementById(id).style.visibility = 'visible';
};

/**
 * Hides the overlay element which is specified by id in the option page.
 * @param {string} id The element ID.
 * @private
 */
mozc.OptionPage.prototype.hideOverlayElementById_ = function(id) {
  this.document_.getElementById(id).style.visibility = 'hidden';
  this.unfreezeMainDiv_();
};

/**
 * Shows a confirm dialog box. This method makes callerPageId element
 * unfocusable while showing the dialog box.
 * @param {string} callerPageId The ID of caller page.
 * @param {string} title The title of the dialog box.
 * @param {string} message The message in the dialog box.
 * @param {!function(?boolean)} callback Function to be called with the result.
 * @private
 */
mozc.OptionPage.prototype.showConfirm_ = function(callerPageId,
                                                  title,
                                                  message,
                                                  callback) {
  this.setChildNodesUnfocusableByTabKeyById_(callerPageId);
  var confirmOverlay = this.document_.createElement('div');
  confirmOverlay.classList.add('overlay', 'confirm_overlay');

  var confirmPage = this.document_.createElement('div');
  confirmPage.classList.add('overlay_page', 'confirm_page');

  var closeDiv = this.document_.createElement('div');
  closeDiv.classList.add('close_button');

  var section = this.document_.createElement('section');

  var confirmTitle = this.document_.createElement('div');
  confirmTitle.classList.add('confirm_title');
  confirmTitle.appendChild(this.document_.createTextNode(title));

  var confirmMessage = this.document_.createElement('div');
  confirmMessage.classList.add('confirm_message_div');
  confirmMessage.appendChild(this.document_.createTextNode(message));

  var confirmOkCancelDiv = this.document_.createElement('div');
  confirmOkCancelDiv.classList.add('confirm_ok_cancel_div');

  var confirmOkButton = this.document_.createElement('input');
  confirmOkButton.classList.add('confirm_ok_button');
  confirmOkButton.type = 'button';
  confirmOkButton.value = chrome.i18n.getMessage('configDialogOk');

  var confirmCancelButton = this.document_.createElement('input');
  confirmCancelButton.classList.add('confirm_cancel_button');
  confirmCancelButton.type = 'button';
  confirmCancelButton.value = chrome.i18n.getMessage('configDialogCancel');

  confirmOkCancelDiv.appendChild(confirmCancelButton);
  confirmOkCancelDiv.appendChild(confirmOkButton);
  section.appendChild(confirmTitle);
  section.appendChild(confirmMessage);
  section.appendChild(confirmOkCancelDiv);
  confirmPage.appendChild(closeDiv);
  confirmPage.appendChild(section);
  confirmOverlay.appendChild(confirmPage);

  var okCallback =
      (function(overlay, pageId, callbackFunc) {
        this.escapeKeyHandlers_.pop();
        this.document_.body.removeChild(overlay);
        this.setChildNodesFocusableByTabKeyById_(pageId);
        callbackFunc(true);
      }).bind(this, confirmOverlay, callerPageId, callback);
  var cancelCallback =
      (function(overlay, pageId, callbackFunc) {
        this.escapeKeyHandlers_.pop();
        this.document_.body.removeChild(overlay);
        this.setChildNodesFocusableByTabKeyById_(pageId);
        callbackFunc(false);
      }).bind(this, confirmOverlay, callerPageId, callback);
  closeDiv.addEventListener('click', cancelCallback, true);
  confirmOkButton.addEventListener('click', okCallback, true);
  confirmCancelButton.addEventListener('click', cancelCallback, true);
  this.escapeKeyHandlers_.push(cancelCallback);
  this.document_.body.appendChild(confirmOverlay);
  confirmOkButton.focus();
};

/**
 * Shows a prompt dialog box. This method makes callerPageId element unfocusable
 * while showing the dialog box.
 * @param {string} callerPageId The ID of caller page.
 * @param {string} title The title of the dialog box.
 * @param {string} message The message in the dialog box.
 * @param {string} defaultValue The default value.
 * @param {!function(?string)} callback Function to be called with the result.
 *     If the user clicks the cancel button the result will be null.
 * @private
 */
mozc.OptionPage.prototype.showPrompt_ = function(callerPageId,
                                                 title,
                                                 message,
                                                 defaultValue,
                                                 callback) {
  this.setChildNodesUnfocusableByTabKeyById_(callerPageId);
  var promptOverlay = this.document_.createElement('div');
  promptOverlay.classList.add('overlay', 'prompt_overlay');

  var promptPage = this.document_.createElement('div');
  promptPage.classList.add('overlay_page', 'prompt_page');

  var closeDiv = this.document_.createElement('div');
  closeDiv.classList.add('close_button');

  var section = this.document_.createElement('section');

  var promptTitle = this.document_.createElement('div');
  promptTitle.classList.add('prompt_title');
  promptTitle.appendChild(this.document_.createTextNode(title));

  var promptMessage = this.document_.createElement('div');
  promptMessage.classList.add('prompt_message_div');
  promptMessage.appendChild(this.document_.createTextNode(message));

  var promptOkCancelDiv = this.document_.createElement('div');
  promptOkCancelDiv.classList.add('prompt_ok_cancel_div');

  var promptInputDiv = this.document_.createElement('div');
  promptInputDiv.classList.add('prompt_input_div');

  var promptInput = this.document_.createElement('input');
  promptInput.classList.add('prompt_input');
  promptInput.type = 'text';
  promptInput.value = defaultValue;
  promptInputDiv.appendChild(promptInput);

  var promptOkButton = this.document_.createElement('input');
  promptOkButton.classList.add('prompt_ok_button');
  promptOkButton.type = 'button';
  promptOkButton.value = chrome.i18n.getMessage('configDialogOk');

  var promptCancelButton = this.document_.createElement('input');
  promptCancelButton.classList.add('prompt_cancel_button');
  promptCancelButton.type = 'button';
  promptCancelButton.value = chrome.i18n.getMessage('configDialogCancel');

  promptOkCancelDiv.appendChild(promptCancelButton);
  promptOkCancelDiv.appendChild(promptOkButton);
  section.appendChild(promptTitle);
  section.appendChild(promptMessage);
  section.appendChild(promptInputDiv);
  section.appendChild(promptOkCancelDiv);
  promptPage.appendChild(closeDiv);
  promptPage.appendChild(section);
  promptOverlay.appendChild(promptPage);

  var okCallback =
      (function(overlay, pageId, callbackFunc, input) {
        this.escapeKeyHandlers_.pop();
        this.document_.body.removeChild(overlay);
        this.setChildNodesFocusableByTabKeyById_(pageId);
        callbackFunc(input.value);
      }).bind(this, promptOverlay, callerPageId, callback, promptInput);
  var cancelCallback =
      (function(overlay, pageId, callbackFunc) {
        this.escapeKeyHandlers_.pop();
        this.document_.body.removeChild(overlay);
        this.setChildNodesFocusableByTabKeyById_(pageId);
        callbackFunc(null);
      }).bind(this, promptOverlay, callerPageId, callback);
  closeDiv.addEventListener('click', cancelCallback, true);
  promptOkButton.addEventListener('click', okCallback, true);
  promptCancelButton.addEventListener('click', cancelCallback, true);
  this.escapeKeyHandlers_.push(cancelCallback);
  this.document_.body.appendChild(promptOverlay);
  promptInput.focus();
};

/**
 * Shows an alert dialog box. This method makes callerPageId element unfocusable
 * while showing the dialog box.
 * @param {string} callerPageId The ID of caller page.
 * @param {string} title The title of the dialog box.
 * @param {string} message The message in the dialog box.
 * @param {!function()=} opt_callback Function to be called when closed.
 * @private
 */
mozc.OptionPage.prototype.showAlert_ = function(callerPageId,
                                                title,
                                                message,
                                                opt_callback) {
  this.setChildNodesUnfocusableByTabKeyById_(callerPageId);
  var alertOverlay = this.document_.createElement('div');
  alertOverlay.classList.add('overlay', 'alert_overlay');

  var alertPage = this.document_.createElement('div');
  alertPage.classList.add('overlay_page', 'alert_page');

  var closeDiv = this.document_.createElement('div');
  closeDiv.classList.add('close_button');

  var section = this.document_.createElement('section');

  var alertTitle = this.document_.createElement('div');
  alertTitle.classList.add('alert_title');
  alertTitle.appendChild(this.document_.createTextNode(title));

  var alertMessage = this.document_.createElement('div');
  alertMessage.classList.add('alert_message_div');
  alertMessage.appendChild(this.document_.createTextNode(message));

  var alertOkDiv = this.document_.createElement('div');
  alertOkDiv.classList.add('alert_ok_div');

  var alertOkButton = this.document_.createElement('input');
  alertOkButton.classList.add('alert_ok_button');
  alertOkButton.type = 'button';
  alertOkButton.value = chrome.i18n.getMessage('configDialogOk');

  alertOkDiv.appendChild(alertOkButton);
  section.appendChild(alertTitle);
  section.appendChild(alertMessage);
  section.appendChild(alertOkDiv);
  alertPage.appendChild(closeDiv);
  alertPage.appendChild(section);
  alertOverlay.appendChild(alertPage);

  var okCallback =
      (function(overlay, pageId, opt_callbackFunc) {
        this.escapeKeyHandlers_.pop();
        this.document_.body.removeChild(overlay);
        this.setChildNodesFocusableByTabKeyById_(pageId);
        if (opt_callbackFunc) {
          opt_callbackFunc();
        }
      }).bind(this, alertOverlay, callerPageId, opt_callback);
  closeDiv.addEventListener('click', okCallback, true);
  alertOkButton.addEventListener('click', okCallback, true);

  this.escapeKeyHandlers_.push(okCallback);
  this.document_.body.appendChild(alertOverlay);
  alertOkButton.focus();
};

/**
 * Called when a button is clicked.
 * @param {string} buttonId The clicked button ID.
 * @private
 */
mozc.OptionPage.prototype.onButtonClick_ = function(buttonId) {
  if (buttonId == 'clear_history_open') {
    this.onClearHistoryOpenClicked_();
  } else if (buttonId == 'clear_history_close') {
    this.onClearHistoryCancelClicked_();
  } else if (buttonId == 'clear_history_ok') {
    this.onClearHistoryOkClicked_();
  } else if (buttonId == 'clear_history_cancel') {
    this.onClearHistoryCancelClicked_();
  } else if (buttonId == 'sync_config_cancel') {
    this.onSyncConfigCancelClicked_();
  } else if (buttonId == 'sync_config_ok') {
    this.onSyncConfigOkClicked_();
  } else if (buttonId == 'sync_config_close') {
    this.onSyncConfigCancelClicked_();
  } else if (buttonId == 'sync_toggle_button') {
    this.onSyncToggleButtonClicked_();
  } else if (buttonId == 'sync_customization_button') {
    this.onSyncCustomizationButtonClicked_();
  } else if (buttonId == 'dictionary_tool_open_button') {
    this.onDictionaryToolOpenButtonClicked_();
  } else if (buttonId == 'dictionary_tool_done_button') {
    this.onDictionaryToolDoneButtonClicked_();
  } else if (buttonId == 'dictionary_tool_close') {
    this.onDictionaryToolDoneButtonClicked_();
  } else if (buttonId == 'create_dictionary_button') {
    this.onDictionaryToolCreateButtonClicked_();
  } else if (buttonId == 'rename_dictionary_button') {
    this.onDictionaryToolRenameButtonClicked_();
  } else if (buttonId == 'delete_dictionary_button') {
    this.onDictionaryToolDeleteButtonClicked_();
  } else if (buttonId == 'export_dictionary_button') {
    this.onDictionaryToolExportButtonClicked_();
  }
};

/**
 * Called when clear_history_open button is clicked.
 * This method opens clear history dialog.
 * @private
 */
mozc.OptionPage.prototype.onClearHistoryOpenClicked_ = function() {
  this.escapeKeyHandlers_.push(this.onClearHistoryCancelClicked_.bind(this));
  this.document_.getElementById('clear_history_conversion_history').checked =
      true;
  this.document_.getElementById('clear_history_suggestion_history').checked =
      true;
  this.showOverlayElementById_('clear_history_overlay');
  this.document_.getElementById('clear_history_cancel').focus();
};

/**
 * Called when clear_history_ok button is clicked.
 * This method calls CLEAR_USER_HISTORY and CLEAR_USER_PREDICTION command
 * according to clear_history_conversion_history and
 * clear_history_suggestion_history check boxes.
 * @private
 */
mozc.OptionPage.prototype.onClearHistoryOkClicked_ = function() {
  if (this.document_.getElementById(
          'clear_history_conversion_history').checked) {
    this.naclMozc_.clearUserHistory();
  }
  if (this.document_.getElementById(
          'clear_history_suggestion_history').checked) {
    this.naclMozc_.clearUserPrediction();
  }
  this.escapeKeyHandlers_.pop();
  this.hideOverlayElementById_('clear_history_overlay');
  this.document_.getElementById('clear_history_open').focus();
};

/**
 * Called when clear_history_cancel button or clear_history_close button is
 * clicked or the user presses Esc key while clear history dialog is opened.
 * This method closes the clear history dialog.
 * @private
 */
mozc.OptionPage.prototype.onClearHistoryCancelClicked_ = function() {
  this.escapeKeyHandlers_.pop();
  this.hideOverlayElementById_('clear_history_overlay');
  this.document_.getElementById('clear_history_open').focus();
};

/**
 * Updates the cloud sync status and closes the sync dialog.
 * @param {!function()=} opt_callback Function to be called when finished.
 * @private
 */
mozc.OptionPage.prototype.updateSyncStatusAndCloseSyncDialog_ =
    function(opt_callback) {
  this.naclMozc_.getCloudSyncStatus((function(res) {
    this.displaySyncStatus_(res);
    this.escapeKeyHandlers_.pop();
    this.hideOverlayElementById_('sync_config_overlay');
    this.document_.getElementById('sync_customization_button').focus();
    if (opt_callback) {
      opt_callback();
    }
  }).bind(this));
};

/**
 * Called when sync_config_cancel button or sync_config_close button is clicked
 * or the user presses Esc key while sycn dialog is opened.
 * This method closes the sync dialog.
 * @private
 */
mozc.OptionPage.prototype.onSyncConfigCancelClicked_ = function() {
  this.updateSyncStatusAndCloseSyncDialog_();
};

/**
 * Called when sync_config_ok button is clicked.
 * This method starts sync if sync_settings or sync_user_dictionary is checked.
 * @private
 */
mozc.OptionPage.prototype.onSyncConfigOkClicked_ = function() {
  if (!this.document_.getElementById('sync_settings').checked &&
      !this.document_.getElementById('sync_user_dictionary').checked) {
    // Stop sync.
    this.naclMozc_.getConfig((function(response) {
      var config = response['output']['config'];
      delete config['sync_config'];
      this.syncEnabled_ = false;
      this.naclMozc_.setConfig(
          config,
          this.naclMozc_.addAuthCode.bind(
              this.naclMozc_,
              {'access_token': ''},
              this.updateSyncStatusAndCloseSyncDialog_.bind(this, undefined)));
    }).bind(this));
    return;
  }
  this.naclMozc_.getConfig((function(response) {
    var config = response['output']['config'];
    if (!config['sync_config']) {
      config['sync_config'] = {};
    }
    config['sync_config']['use_config_sync'] =
        this.document_.getElementById('sync_settings').checked;
    config['sync_config']['use_user_dictionary_sync'] =
        this.document_.getElementById('sync_user_dictionary').checked;
    this.syncEnabled_ = true;
    this.naclMozc_.setConfig(
        config,
        (function() {
          this.naclMozc_.sendReload();
          this.getAuthTokenAndStartCloudSync_(
              this.naclMozc_.getCloudSyncStatus.bind(
                  this.naclMozc_,
                  this.updateSyncStatusAndCloseSyncDialog_.bind(this,
                                                                undefined)));
        }).bind(this));
  }).bind(this));
};

/**
 * Gets the auth token and starts cloud sync.
 * @param {!function(Object)=} opt_callback Function to be called with results
 *     of START_CLOUD_SYNC.
 * @private
 */
mozc.OptionPage.prototype.getAuthTokenAndStartCloudSync_ =
    function(opt_callback) {
  chrome.identity.getAuthToken(
      {interactive: true},
      /** @param {string=} opt_token */
      (function(opt_token) {
        this.naclMozc_.addAuthCode(
            {'access_token': opt_token ? opt_token : ''},
            this.naclMozc_.startCloudSync.bind(this.naclMozc_, opt_callback));
      }).bind(this));
};

/**
 * Called when sync_toggle_button button is clicked.
 * This method opens sync dialog if the current sync status is NOSYNC.
 * Otherwise this method stops sync.
 * @private
 */
mozc.OptionPage.prototype.onSyncToggleButtonClicked_ = function() {
  this.disableElementById_('sync_toggle_button');
  this.disableElementById_('sync_customization_button');
  if (!this.syncEnabled_) {
    // Show sync config dialog.
    this.escapeKeyHandlers_.push(this.onSyncConfigCancelClicked_.bind(this));
    this.document_.getElementById('sync_settings').checked = true;
    this.document_.getElementById('sync_user_dictionary').checked = true;
    this.showOverlayElementById_('sync_config_overlay');
    this.document_.getElementById('sync_config_cancel').focus();
  } else {
    // Stop sync.
    this.naclMozc_.getConfig((function(response) {
      var config = response['output']['config'];
      delete config['sync_config'];
      this.syncEnabled_ = false;
      this.naclMozc_.setConfig(
          config,
          this.naclMozc_.addAuthCode.bind(
              this.naclMozc_,
              {'access_token': ''},
              this.naclMozc_.getCloudSyncStatus.bind(
                  this.naclMozc_,
                  this.displaySyncStatus_.bind(this))));
    }).bind(this));
  }
};

/**
 * Called when sync_customization_button button is clicked.
 * This method opens sync dialog.
 * @private
 */
mozc.OptionPage.prototype.onSyncCustomizationButtonClicked_ = function() {
  this.disableElementById_('sync_toggle_button');
  this.disableElementById_('sync_customization_button');
  this.naclMozc_.getConfig((function(response) {
    this.escapeKeyHandlers_.push(this.onSyncConfigCancelClicked_.bind(this));
    var sync_config = response['output']['config']['sync_config'];
    this.document_.getElementById('sync_settings').checked =
        sync_config && sync_config['use_config_sync'];
    this.document_.getElementById('sync_user_dictionary').checked =
        sync_config && sync_config['use_user_dictionary_sync'];
    this.showOverlayElementById_('sync_config_overlay');
    this.document_.getElementById('sync_config_cancel').focus();
  }).bind(this));
};

/**
 * Called when dictionary_tool_open_button button is clicked.
 * This method creates a dictionary session and calls LOAD dictionary command
 * and gets the user dictionary storage from NaCl Mozc module and shows a
 * dictionary tool.
 * @private
 */
mozc.OptionPage.prototype.onDictionaryToolOpenButtonClicked_ = function() {
  this.disableElementById_('dictionary_tool_open_button');
  this.sendUserDictionaryCommand_(
    {'type': 'CREATE_SESSION'},
    (function(response) {
      this.userDictionarySessionId_ = response['session_id'];
      this.sendUserDictionaryCommand_(
        {
          'type': 'LOAD',
          'session_id': this.userDictionarySessionId_,
          'ensure_non_empty_storage': mozc.ENABLE_SIMPLE_DICTIONARY_TOOL_
        },
        this.loadStorage_.bind(
            this,
            (function() {
              this.escapeKeyHandlers_.push(
                  this.onDictionaryToolDoneButtonClicked_.bind(this));
              this.showOverlayElementById_('dictionary_tool_overlay');
              this.document_.getElementById('dictionary_tool_reading_new_input')
                  .focus();
            }).bind(this)));
    }).bind(this));
};

/**
 * Updates the dictionary list in 'user_dictionary_select' element according to
 * dictionaries_.
 * @private
 */
mozc.OptionPage.prototype.updateDictionaryList_ = function() {
  var selectionElement =
      this.document_.getElementById('user_dictionary_select');
  selectionElement.disabled = !this.dictionaries_.length;
  var lastValue = selectionElement.value;
  while (selectionElement.hasChildNodes()) {
    selectionElement.removeChild(selectionElement.firstChild);
  }
  for (var i = 0; i < this.dictionaries_.length; ++i) {
    var dictionaryName = this.dictionaries_[i].name;
    // 'Sync Dictionary' is the default name of sync-able dictionary defined in
    // user_dictionary_storage.cc.
    if (dictionaryName == 'Sync Dictionary') {
      dictionaryName =
          chrome.i18n.getMessage('dictionaryToolSyncableDictionaryName');
    }
    var newOption =
        this.createOptionElement_(dictionaryName,
                                  i.toString());
    if (i == lastValue) {
      newOption.selected = true;
    }
    selectionElement.appendChild(newOption);
  }
};

/**
 * Gets the user dictionary storage from NaCl Mozc module and stores it to
 * dictionaries_ and update the dictionary list and content.
 * @param {!function()=} opt_callback Function to be called when finished.
 * @private
 */
mozc.OptionPage.prototype.loadStorage_ = function(opt_callback) {
  this.getStorage_((function() {
    this.updateDictionaryList_();
    this.updateDictionaryContent_();
    if (opt_callback) {
      opt_callback();
    }
  }).bind(this));
};

/**
 * Gets the user dictionary storage from NaCl Mozc module and stores it to
 * dictionaries_.
 * @param {!function()=} opt_callback Function to be called when finished.
 * @private
 */
mozc.OptionPage.prototype.getStorage_ = function(opt_callback) {
  this.sendUserDictionaryCommand_(
    {
      'type': 'GET_STORAGE',
      'session_id': this.userDictionarySessionId_
    },
    (function(response) {
      this.dictionaries_ = response['storage']['dictionaries'];
      if (!this.dictionaries_) {
        this.dictionaries_ = [];
      }
      if (opt_callback) {
        opt_callback();
      }
    }).bind(this));
};

/**
 * Called when dictionary_tool_done_button button or dictionary_tool_close
 * button is clicked or the user presses Esc key while dictionary tool is
 * opened.
 * This method closes the dictionary tool.
 * @private
 */
mozc.OptionPage.prototype.onDictionaryToolDoneButtonClicked_ = function() {
  this.escapeKeyHandlers_.pop();
  this.enableElementById_('dictionary_tool_open_button');
  this.hideOverlayElementById_('dictionary_tool_overlay');
  this.document_.getElementById('dictionary_tool_open_button').focus();
};

/**
 * Called when create_dictionary_button button is clicked.
 * This method displays a dialog box that prompts the user to input the new
 * dictionary name and creates a dictionary.
 * @private
 */
mozc.OptionPage.prototype.onDictionaryToolCreateButtonClicked_ = function() {
  this.showPrompt_(
      'dictionary_tool_page',
      chrome.i18n.getMessage('appName'),
      chrome.i18n.getMessage('dictionaryToolDictionaryName'),
      '',
      (function(dictName) {
        if (!dictName) {
          return;
        }
        this.sendUserDictionaryCommand_(
          {
            'type': 'CREATE_DICTIONARY',
            'session_id': this.userDictionarySessionId_,
            'dictionary_name': dictName
          },
          (function(response) {
            if (response['status'] != 'USER_DICTIONARY_COMMAND_SUCCESS') {
              this.showDictionaryToolError_(response['status']);
            } else {
              this.sendUserDictionaryCommand_(
                {'type': 'SAVE', 'session_id': this.userDictionarySessionId_},
                this.naclMozc_.sendReload.bind(this.naclMozc_, undefined));
              // TODO(horo): getStorage_ is a heavy operation. We shoud only
              // update the created dictionary.
              this.getStorage_((function() {
                this.updateDictionaryList_();
                this.document_.getElementById('user_dictionary_select').value =
                    this.dictionaries_.length - 1;
                this.updateDictionaryContent_();
              }).bind(this));
            }
          }).bind(this));
      }).bind(this));
};

/**
 * Returns the selected dictionary index in user_dictionary_select element.
 * @return {number} The selected dictionary index. The return value is NaN
 *     when there is no dictionary.
 * @private
 */
mozc.OptionPage.prototype.getSelectedDictionaryIndex_ = function() {
  return parseInt(
      this.document_.getElementById('user_dictionary_select').value,
      10);
};

/**
 * Called when rename_dictionary_button button is clicked.
 * This method displays a dialog box that prompts the user to input the new
 * dictionary name and renames the selected dictionary.
 * @private
 */
mozc.OptionPage.prototype.onDictionaryToolRenameButtonClicked_ = function() {
  var dictIndex = this.getSelectedDictionaryIndex_();
  if (isNaN(dictIndex)) {
    return;
  }
  var dictionaryId = this.dictionaries_[dictIndex]['id'];
  this.showPrompt_(
      'dictionary_tool_page',
      chrome.i18n.getMessage('appName'),
      chrome.i18n.getMessage('dictionaryToolDictionaryName'),
      this.dictionaries_[dictIndex]['name'],
      (function(dictName) {
        if (!dictName) {
          return;
        }
        this.sendUserDictionaryCommand_(
          {
            'type': 'RENAME_DICTIONARY',
            'session_id': this.userDictionarySessionId_,
            'dictionary_id': dictionaryId,
            'dictionary_name': dictName
          },
          (function(response) {
            if (response['status'] != 'USER_DICTIONARY_COMMAND_SUCCESS') {
              this.showDictionaryToolError_(response['status']);
            } else {
              this.sendUserDictionaryCommand_(
                {'type': 'SAVE', 'session_id': this.userDictionarySessionId_},
                this.naclMozc_.sendReload.bind(this.naclMozc_, undefined));
              // TODO(horo): loadStorage_ is a heavy operation. We shoud only
              // update the changed dictionary.
              this.loadStorage_();
            }
          }).bind(this));
      }).bind(this));
};

/**
 * Called when delete_dictionary_button button is clicked.
 * This method deletes the selecetd dictionary.
 * @private
 */
mozc.OptionPage.prototype.onDictionaryToolDeleteButtonClicked_ = function() {
  var dictIndex = this.getSelectedDictionaryIndex_();
  if (isNaN(dictIndex)) {
    return;
  }
  this.showConfirm_(
      'dictionary_tool_page',
      chrome.i18n.getMessage('appName'),
      chrome.i18n.getMessage('dictionaryToolDeleteDictionaryConfirm',
                             this.dictionaries_[dictIndex]['name']),
      (function(result) {
        if (!result) {
          return;
        }
        var dictionaryId = this.dictionaries_[dictIndex]['id'];
        this.sendUserDictionaryCommand_(
          {
            'type': 'DELETE_DICTIONARY',
            'session_id': this.userDictionarySessionId_,
            'dictionary_id': dictionaryId
          },
          (function(response) {
            if (response['status'] != 'USER_DICTIONARY_COMMAND_SUCCESS') {
              this.showDictionaryToolError_(response['status']);
            } else {
              this.sendUserDictionaryCommand_(
                {'type': 'SAVE', 'session_id': this.userDictionarySessionId_},
                this.naclMozc_.sendReload.bind(this.naclMozc_, undefined));
              // TODO(horo): loadStorage_ is a heavy operation. We shoud only
              // update the deleted dictionary.
              this.loadStorage_();
            }
          }).bind(this));
      }).bind(this));
};

/**
 * Called when export_dictionary_button button is clicked.
 * This method exports the selecetd dictionary.
 * @private
 */
mozc.OptionPage.prototype.onDictionaryToolExportButtonClicked_ = function() {
  var dictIndex = this.getSelectedDictionaryIndex_();
  if (isNaN(dictIndex)) {
    return;
  }
  var data = '';
  var entries = this.dictionaries_[dictIndex]['entries'];
  for (var i = 0; i < entries.length; ++i) {
    data += entries[i]['key'] + '\t' +
            entries[i]['value'] + '\t' +
            this.posNameMap_[entries[i]['pos']] + '\t' +
            entries[i]['comment'] + '\n';
  }
  var blob = new Blob([data]);
  var a = this.document_.createElement('a');
  a.href = this.window_.URL.createObjectURL(blob);
  a.download = 'user_dict.txt';
  a.style.display = 'none';
  this.document_.body.appendChild(a);
  a.click();
  this.document_.body.removeChild(a);
};

/**
 * Called when dictionary entry element is focused.
 * This method shows the input elements for 'reading' and 'word' and 'comment',
 * and the select element for 'category'.
 * @param {string} type Entry type ('reading', 'word', 'category' or 'comment').
 * @param {number} index Index of the entry.
 * @param {Event} event Event object passed from browser.
 * @private
 */
mozc.OptionPage.prototype.onDictionaryEntryFocus_ = function(type,
                                                             index,
                                                             event) {
  var dictIndex = this.getSelectedDictionaryIndex_();
  if (isNaN(dictIndex)) {
    return;
  }
  var entryNode =
      this.document_.getElementById('dictionary_tool_entry_' + index);
  if (entryNode.classList.contains('dictionary_tool_entry_selected')) {
    return;
  }
  var readingInput =
    this.document_.getElementById('dictionary_tool_reading_input_' + index);
  var wordInput =
    this.document_.getElementById('dictionary_tool_word_input_' + index);
  var categorySelect =
    this.document_.getElementById('dictionary_tool_category_input_' + index);
  var commentInput =
    this.document_.getElementById('dictionary_tool_comment_input_' + index);
  while (categorySelect.childNodes.length) {
    categorySelect.removeChild(categorySelect.firstChild);
  }
  for (var i = 0; i < this.posList_.length; ++i) {
    categorySelect.appendChild(
        this.createOptionElement_(this.posList_[i]['name'],
                                  this.posList_[i]['type']));
  }
  var entry =
      this.dictionaries_[dictIndex]['entries'][index];
  readingInput.value = entry['key'];
  wordInput.value = entry['value'];
  categorySelect.value = entry['pos'];
  commentInput.value = entry['comment'];
  entryNode.classList.add('dictionary_tool_entry_selected');
  var focusInput = this.document_.getElementById(
      'dictionary_tool_' + type + '_input_' + index);
  focusInput.focus();
  this.document_.getElementById('dictionary_tool_reading_' + index).tabIndex =
      -1;
  this.document_.getElementById('dictionary_tool_word_' + index).tabIndex =
      -1;
  this.document_.getElementById('dictionary_tool_category_' + index).tabIndex =
      -1;
  this.document_.getElementById('dictionary_tool_comment_' + index).tabIndex =
      -1;
};

/**
 * Called when dictionary entry element lost focus.
 * This method hides the input elements for 'reading' and 'word' and 'comment',
 * and the select element for 'category'. And also this method saves the entry.
 * @param {string} type Entry type ('reading', 'word', 'category' or 'comment').
 * @param {number} index Index of the entry.
 * @param {Event} event Event object passed from browser.
 * @private
 */
mozc.OptionPage.prototype.onDictionaryEntryBlur_ =
  function(type, index, event) {
  var dictIndex = this.getSelectedDictionaryIndex_();
  if (isNaN(dictIndex)) {
    return;
  }
  if (event && event.relatedTarget &&
      (event.relatedTarget.id == 'dictionary_tool_reading_input_' + index ||
       event.relatedTarget.id == 'dictionary_tool_word_input_' + index ||
       event.relatedTarget.id == 'dictionary_tool_category_input_' + index ||
       event.relatedTarget.id == 'dictionary_tool_comment_input_' + index)) {
    return;
  }
  var entryNode =
      this.document_.getElementById('dictionary_tool_entry_' + index);
  if (!entryNode.classList.contains('dictionary_tool_entry_selected')) {
    return;
  }
  var readingInput =
    this.document_.getElementById('dictionary_tool_reading_input_' + index);
  var wordInput =
    this.document_.getElementById('dictionary_tool_word_input_' + index);
  var categorySelect =
    this.document_.getElementById('dictionary_tool_category_input_' + index);
  var commentInput =
    this.document_.getElementById('dictionary_tool_comment_input_' + index);
  var readingStatic =
    this.document_.getElementById('dictionary_tool_reading_static_' + index);
  var wordStatic =
    this.document_.getElementById('dictionary_tool_word_static_' + index);
  var categoryStatic =
    this.document_.getElementById('dictionary_tool_category_static_' + index);
  var commentStatic =
    this.document_.getElementById('dictionary_tool_comment_static_' + index);

  this.document_.getElementById('dictionary_tool_reading_' + index).tabIndex =
      0;
  this.document_.getElementById('dictionary_tool_word_' + index).tabIndex =
      0;
  this.document_.getElementById('dictionary_tool_category_' + index).tabIndex =
      0;
  this.document_.getElementById('dictionary_tool_comment_' + index).tabIndex =
      0;
  var oldEntry = this.dictionaries_[dictIndex]['entries'][index];
  if (readingInput.value == oldEntry['key'] &&
      wordInput.value == oldEntry['value'] &&
      commentInput.value == oldEntry['comment'] &&
      categorySelect.value == oldEntry['pos']) {
    entryNode.classList.remove('dictionary_tool_entry_selected');
    return;
  }
  this.sendUserDictionaryCommand_(
    {
      'type': 'EDIT_ENTRY',
      'session_id': this.userDictionarySessionId_,
      'dictionary_id': this.dictionaries_[dictIndex]['id'],
      'entry_index': [index],
      'entry': {
        'key': readingInput.value,
        'value': wordInput.value,
        'comment': commentInput.value,
        'pos': categorySelect.value
      }
    },
    (function(response) {
      if (response['status'] != 'USER_DICTIONARY_COMMAND_SUCCESS') {
        this.showDictionaryToolError_(response['status']);
      }
      this.sendUserDictionaryCommand_(
        {'type': 'SAVE', 'session_id': this.userDictionarySessionId_},
        this.naclMozc_.sendReload.bind(this.naclMozc_, undefined));
      this.sendUserDictionaryCommand_(
        {
          'type': 'GET_ENTRY',
          'session_id': this.userDictionarySessionId_,
          'dictionary_id': this.dictionaries_[dictIndex]['id'],
          'entry_index': [index]
        },
        (function(response) {
          this.dictionaries_[dictIndex]['entries'][index] = response['entry'];
          var entry =
              this.dictionaries_[dictIndex]['entries'][index];
          readingStatic.innerText = entry['key'];
          wordStatic.innerText = entry['value'];
          categoryStatic.innerText = this.posNameMap_[entry['pos']];
          commentStatic.innerText = entry['comment'];
          entryNode.classList.remove('dictionary_tool_entry_selected');
        }).bind(this));
    }).bind(this));
};

/**
 * Called when dictionary_tool_entry_delete_button is clicked.
 * This method deletes the entry in the dictionary.
 * @param {number} index Index of the dictionary entry.
 * @private
 */
mozc.OptionPage.prototype.onDictionaryEntryDeleteClick_ = function(index) {
  var dictIndex = this.getSelectedDictionaryIndex_();
  if (isNaN(dictIndex)) {
    return;
  }
  this.sendUserDictionaryCommand_(
    {
      'type': 'DELETE_ENTRY',
      'session_id': this.userDictionarySessionId_,
      'dictionary_id': this.dictionaries_[dictIndex]['id'],
      'entry_index': [index]
    },
    (function(response) {
      if (response['status'] != 'USER_DICTIONARY_COMMAND_SUCCESS') {
        this.showDictionaryToolError_(response['status']);
      } else {
        this.sendUserDictionaryCommand_(
          {'type': 'SAVE', 'session_id': this.userDictionarySessionId_},
          this.naclMozc_.sendReload.bind(this.naclMozc_, undefined));
        // TODO(horo): loadStorage_ is a heavy operation. We shoud only update
        // the deleted entry.
        this.loadStorage_();
      }
    }).bind(this));
};

/**
 * Shows dictionary tool error.
 * @param {string} status Status string of dictionary tool.
 * @private
 */
mozc.OptionPage.prototype.showDictionaryToolError_ = function(status) {
  var message = mozc.DICTIONARY_TOOL_STATUS_ERRORS_[status];
  if (!message) {
    message = chrome.i18n.getMessage('dictionaryToolStatuErrorGeneral') +
              '[' + status + ']';
  }
  this.showAlert_('dictionary_tool_page',
                  'Error',
                  message);
};

/**
 * Updates the dictionary entry list in 'dictionary_tool_current_area' element
 * according to 'user_dictionary_select' element and dictionaries_.
 * @private
 */
mozc.OptionPage.prototype.updateDictionaryContent_ = function() {
  var dictIndex = this.getSelectedDictionaryIndex_();
  var currentArea =
      this.document_.getElementById('dictionary_tool_current_area');
  if (isNaN(dictIndex)) {
    for (var i = 0;; ++i) {
      var entryDiv =
        this.document_.getElementById('dictionary_tool_entry_' + i);
      if (!entryDiv) {
        break;
      }
      currentArea.removeChild(entryDiv);
    }
    this.disableElementById_('rename_dictionary_button');
    this.disableElementById_('delete_dictionary_button');
    this.disableElementById_('export_dictionary_button');
    this.disableElementById_('dictionary_tool_reading_new_input');
    this.disableElementById_('dictionary_tool_word_new_input');
    this.disableElementById_('dictionary_tool_category_new_select');
    this.disableElementById_('dictionary_tool_comment_new_input');
    return;
  }
  var dictionary = this.dictionaries_[dictIndex];
  if (dictionary['syncable']) {
    this.disableElementById_('rename_dictionary_button');
    this.disableElementById_('delete_dictionary_button');
  } else {
    this.enableElementById_('rename_dictionary_button');
    this.enableElementById_('delete_dictionary_button');
  }
  this.enableElementById_('export_dictionary_button');
  this.enableElementById_('dictionary_tool_reading_new_input');
  this.enableElementById_('dictionary_tool_word_new_input');
  this.enableElementById_('dictionary_tool_category_new_select');
  this.enableElementById_('dictionary_tool_comment_new_input');
  if (!dictionary['entries']) {
    dictionary['entries'] = [];
  }
  for (var i = 0; i < dictionary['entries'].length; ++i) {
    var entryDiv = this.document_.getElementById('dictionary_tool_entry_' + i);
    if (!entryDiv) {
      entryDiv = this.createUserDictionaryEntryDiv_(i);
    }
    currentArea.appendChild(entryDiv);
    this.document_.getElementById(
        'dictionary_tool_reading_static_' + i).innerText =
            dictionary['entries'][i]['key'];
    this.document_.getElementById(
        'dictionary_tool_word_static_' + i).innerText =
            dictionary['entries'][i]['value'];
    this.document_.getElementById(
        'dictionary_tool_category_static_' + i).innerText =
            this.posNameMap_[dictionary['entries'][i]['pos']];
    this.document_.getElementById(
        'dictionary_tool_comment_static_' + i).innerText =
            dictionary['entries'][i]['comment'];
  }
  for (var i = dictionary['entries'].length;; ++i) {
    var entryDiv = this.document_.getElementById('dictionary_tool_entry_' + i);
    if (!entryDiv) {
      break;
    }
    currentArea.removeChild(entryDiv);
  }
};

/**
 * Creates a dictionary entry element like the followings.
 * <div id="dictionary_tool_entry_INDEX" class="dictionary_tool_entry">
 *   <div id="dictionary_tool_reading_INDEX" class="dictionary_tool_reading">
 *     <div id="dictionary_tool_reading_static_INDEX" class="static_text"></div>
 *     <input id="dictionary_tool_reading_input_INDEX" type="text"
 *            class="dictionary_tool_entry_input">
 *   </div>
 *   <div id="dictionary_tool_word_INDEX" class="dictionary_tool_word">
 *     <div id="dictionary_tool_word_static_INDEX" class="static_text"></div>
 *     <input id="dictionary_tool_word_input_INDEX" type="text"
 *            class="dictionary_tool_entry_input">
 *   </div>
 *   <div id="dictionary_tool_category_INDEX" class="dictionary_tool_category">
 *     <div id="dictionary_tool_category_static_INDEX" class="static_text">
 *     <select id="dictionary_tool_category_input_INDEX"
 *             class="dictionary_tool_entry_select"></select>
 *   </div>
 *   <div id="dictionary_tool_comment_INDEX" class="dictionary_tool_comment">
 *     <div id="dictionary_tool_comment_static_INDEX" class="static_text"></div>
 *     <input id="dictionary_tool_comment_input_INDEX" type="text"
 *            class="dictionary_tool_entry_input">
 *   </div>
 *   <div class="dictionary_tool_entry_delete_button_div">
 *     <button class="dictionary_tool_entry_delete_button"></button>
 *   </div>
 * </div>
 * @param {number} index Index of the dictionary entry.
 * @return {!Element} Created element.
 * @private
 */
mozc.OptionPage.prototype.createUserDictionaryEntryDiv_ = function(index) {
  var entryDiv = this.document_.createElement('div');
  entryDiv.id = 'dictionary_tool_entry_' + index;
  entryDiv.classList.add('dictionary_tool_entry');
  var readingDiv = this.document_.createElement('div');
  var readingStaticDiv = this.document_.createElement('div');
  var readingInput = this.document_.createElement('input');
  readingDiv.id = 'dictionary_tool_reading_' + index;
  readingDiv.classList.add('dictionary_tool_reading');
  readingDiv.tabIndex = 0;
  readingDiv.addEventListener(
      'focus',
      this.onDictionaryEntryFocus_.bind(this, 'reading', index),
      true);
  readingStaticDiv.id = 'dictionary_tool_reading_static_' + index;
  readingStaticDiv.classList.add('static_text');
  readingInput.id = 'dictionary_tool_reading_input_' + index;
  readingInput.type = 'text';
  readingInput.classList.add('dictionary_tool_entry_input');
  readingInput.addEventListener(
      'blur',
      this.onDictionaryEntryBlur_.bind(this, 'reading', index),
      true);
  readingDiv.appendChild(readingStaticDiv);
  readingDiv.appendChild(readingInput);
  entryDiv.appendChild(readingDiv);
  var wordDiv = this.document_.createElement('div');
  var wordStaticDiv = this.document_.createElement('div');
  var wordInput = this.document_.createElement('input');
  wordDiv.id = 'dictionary_tool_word_' + index;
  wordDiv.classList.add('dictionary_tool_word');
  wordDiv.tabIndex = 0;
  wordDiv.addEventListener(
      'focus',
      this.onDictionaryEntryFocus_.bind(this, 'word', index),
      true);
  wordStaticDiv.id = 'dictionary_tool_word_static_' + index;
  wordStaticDiv.classList.add('static_text');
  wordInput.id = 'dictionary_tool_word_input_' + index;
  wordInput.type = 'text';
  wordInput.classList.add('dictionary_tool_entry_input');
  wordInput.addEventListener(
      'blur',
      this.onDictionaryEntryBlur_.bind(this, 'word', index),
      true);
  wordDiv.appendChild(wordStaticDiv);
  wordDiv.appendChild(wordInput);
  entryDiv.appendChild(wordDiv);
  var categoryDiv = this.document_.createElement('div');
  var categoryStaticDiv = this.document_.createElement('div');
  var categorySelect = this.document_.createElement('select');
  categoryDiv.id = 'dictionary_tool_category_' + index;
  categoryDiv.classList.add('dictionary_tool_category');
  categoryDiv.tabIndex = 0;
  categoryDiv.addEventListener(
      'focus',
      this.onDictionaryEntryFocus_.bind(this, 'category', index),
      true);
  categoryStaticDiv.id = 'dictionary_tool_category_static_' + index;
  categoryStaticDiv.classList.add('static_text');
  categorySelect.id = 'dictionary_tool_category_input_' + index;
  categorySelect.classList.add('dictionary_tool_entry_select');
  categorySelect.addEventListener(
      'blur',
      this.onDictionaryEntryBlur_.bind(this, 'category', index),
      true);
  categoryDiv.appendChild(categoryStaticDiv);
  categoryDiv.appendChild(categorySelect);
  entryDiv.appendChild(categoryDiv);
  var commentDiv = this.document_.createElement('div');
  var commentStaticDiv = this.document_.createElement('div');
  var commentInput = this.document_.createElement('input');
  commentDiv.id = 'dictionary_tool_comment_' + index;
  commentDiv.classList.add('dictionary_tool_comment');
  commentDiv.tabIndex = 0;
  commentDiv.addEventListener(
      'focus',
      this.onDictionaryEntryFocus_.bind(this, 'comment', index),
      true);
  commentStaticDiv.id = 'dictionary_tool_comment_static_' + index;
  commentStaticDiv.classList.add('static_text');
  commentInput.id = 'dictionary_tool_comment_input_' + index;
  commentInput.type = 'text';
  commentInput.classList.add('dictionary_tool_entry_input');
  commentInput.addEventListener(
      'blur',
      this.onDictionaryEntryBlur_.bind(this, 'comment', index),
      true);
  commentDiv.appendChild(commentStaticDiv);
  commentDiv.appendChild(commentInput);
  entryDiv.appendChild(commentDiv);
  var deleteDiv = this.document_.createElement('div');
  var deleteButton = this.document_.createElement('button');
  deleteDiv.classList.add('dictionary_tool_entry_delete_button_div');
  deleteButton.classList.add('dictionary_tool_entry_delete_button');
  deleteButton.addEventListener(
      'click',
      this.onDictionaryEntryDeleteClick_.bind(this, index),
      true);
  deleteDiv.appendChild(deleteButton);
  entryDiv.appendChild(deleteDiv);
  return entryDiv;
};

/**
 * Called when user_dictionary_select is changed.
 * @private
 */
mozc.OptionPage.prototype.onDictionarySelectChanged_ = function() {
  this.updateDictionaryContent_();
};

/**
 * Called when dictionary tool new entry input element lost focus.
 * If the new reading and the new word are not empty this method creates a new
 * entry in the selected dictionary.
 * @param {Event} event Event object passed from browser.
 * @private
 */
mozc.OptionPage.prototype.onDictionaryNewEntryLostFocus_ = function(event) {
  if (event && event.relatedTarget &&
      (event.relatedTarget.id == 'dictionary_tool_reading_new_input' ||
       event.relatedTarget.id == 'dictionary_tool_word_new_input' ||
       event.relatedTarget.id == 'dictionary_tool_category_new_select' ||
       event.relatedTarget.id == 'dictionary_tool_comment_new_input')) {
    return;
  }
  var dictIndex = this.getSelectedDictionaryIndex_();
  if (isNaN(dictIndex)) {
    return;
  }
  var dictionaryId = this.dictionaries_[dictIndex]['id'];
  var newReadingInput =
      this.document_.getElementById('dictionary_tool_reading_new_input');
  var newWordInput =
      this.document_.getElementById('dictionary_tool_word_new_input');
  var newCategorySelect =
      this.document_.getElementById('dictionary_tool_category_new_select');
  var newCommentInput =
      this.document_.getElementById('dictionary_tool_comment_new_input');
  var newReading = newReadingInput.value;
  var newWord = newWordInput.value;
  var newCategory = newCategorySelect.value;
  var newComment = newCommentInput.value;
  if (newReading == '' || newWord == '') {
    return;
  }
  // We don't reset the value of dictionary_tool_category_new_select to help the
  // user who want to add the same category entries.
  newReadingInput.value = '';
  newWordInput.value = '';
  newCommentInput.value = '';
  this.sendUserDictionaryCommand_({
    'type': 'ADD_ENTRY',
    'session_id': this.userDictionarySessionId_,
    'dictionary_id': dictionaryId,
    'entry': {
      'key': newReading,
      'value': newWord,
      'comment': newComment,
      'pos': newCategory
    }
  },
  (function(response) {
    if (response['status'] != 'USER_DICTIONARY_COMMAND_SUCCESS') {
      this.showDictionaryToolError_(response['status']);
    } else {
      this.sendUserDictionaryCommand_(
        {'type': 'SAVE', 'session_id': this.userDictionarySessionId_},
        this.naclMozc_.sendReload.bind(this.naclMozc_, undefined));
      // TODO(horo): loadStorage_ is a heavy operation. We shoud only update
      // the added entry.
      this.loadStorage_(newReadingInput.focus.bind(newReadingInput));
    }
  }).bind(this));
};

/**
 * Updates the sync status. This function is first called from initialize method
 * of OptionPage and periodically called using setTimeout.
 * @private
 */
mozc.OptionPage.prototype.updateSyncStatus_ = function() {
  this.naclMozc_.getCloudSyncStatus(
    (function(response) {
      if (this.isUnloaded()) {
        return;
      }
      this.timeoutID_ = setTimeout(this.updateSyncStatus_.bind(this),
                                   mozc.SYNC_STATUS_REFRESH_INTERVAL_);
      this.displaySyncStatus_(response);
    }).bind(this));
};

/**
 * Returns the 'yyyy/MM/dd hh:mm:ss' formatted string of the date object.
 * @param {!Date} date Date object to be formatted into a string.
 * @return {string} The formatted string.
 * @private
 */
mozc.OptionPage.prototype.getFormattedDateTimeString_ = function(date) {
  var year = date.getFullYear();
  var month = date.getMonth() + 1;
  var day = date.getDate();
  var hour = date.getHours();
  var min = date.getMinutes();
  var sec = date.getSeconds();
  if (month < 10) month = '0' + month;
  if (day < 10) day = '0' + day;
  if (hour < 10) hour = '0' + hour;
  if (min < 10) min = '0' + min;
  if (sec < 10) sec = '0' + sec;
  return year + '/' + month + '/' + day + ' ' + hour + ':' + min + ':' + sec;
};

/**
 * Displays sync status which is acquired from NaCl module using
 * getCloudSyncStatus.
 * @param {!Object} response Response data of GET_CLOUD_SYNC_STATUS from NaCl
 * module.
 * @private
 */
mozc.OptionPage.prototype.displaySyncStatus_ = function(response) {
  var sync_status_div = this.document_.getElementById('sync_status');
  if (!response['output'] ||
      !response['output']['cloud_sync_status']) {
    sync_status_div.innerHTML = '';
    return;
  }
  var cloud_sync_status = response['output']['cloud_sync_status'];
  var sync_global_status = cloud_sync_status['global_status'];
  var sync_message = '';
  this.document_.getElementById('sync_toggle_button').value =
      this.syncEnabled_ ?
      chrome.i18n.getMessage('configSyncStopSync') :
      chrome.i18n.getMessage('configSyncStartSync');
  if (!this.syncEnabled_ || sync_global_status == 'NOSYNC') {
    sync_message += chrome.i18n.getMessage('configSyncNoSync');
    this.enableElementById_('sync_toggle_button');
    this.disableElementById_('sync_customization_button');
  } else if (sync_global_status == 'SYNC_SUCCESS' ||
      sync_global_status == 'SYNC_FAILURE') {
    var lastSyncedTimestamp = cloud_sync_status['last_synced_timestamp'];
    if (!lastSyncedTimestamp) {
      sync_message += chrome.i18n.getMessage('configSyncNotSyncedYet');
    } else {
      sync_message += chrome.i18n.getMessage('configSyncLastSyncedTime');
      sync_message += this.getFormattedDateTimeString_(
          new Date(lastSyncedTimestamp * 1000));
    }
    if (this.lastSyncedTimestamp_ != lastSyncedTimestamp) {
      this.lastSyncedTimestamp_ = lastSyncedTimestamp;
      this.naclMozc_.getConfig(this.onConfigLoaded_.bind(this));
    }
    this.enableElementById_('sync_toggle_button');
    this.enableElementById_('sync_customization_button');
  } else if (sync_global_status == 'WAITSYNC') {
    sync_message += chrome.i18n.getMessage('configSyncWaiting');
    this.disableElementById_('sync_toggle_button');
    this.disableElementById_('sync_customization_button');
  } else if (sync_global_status == 'INSYNC') {
    sync_message += chrome.i18n.getMessage('configSyncDuringSync');
    this.disableElementById_('sync_toggle_button');
    this.disableElementById_('sync_customization_button');
  }
  var tooltip = '';
  if (cloud_sync_status['sync_errors'] &&
      cloud_sync_status['sync_errors'].length) {
    for (var i = 0; i < cloud_sync_status['sync_errors'].length; ++i) {
      if (i) {
        tooltip += '\n';
      }
      var error_code = cloud_sync_status['sync_errors'][i]['error_code'];
      if (error_code == 'AUTHORIZATION_FAIL') {
        tooltip += chrome.i18n.getMessage('configSyncAuthorizationFail');
      } else if (error_code == 'USER_DICTIONARY_NUM_ENTRY_EXCEEDED') {
        tooltip +=
            chrome.i18n.getMessage('configSyncUserDictionaryNumEntryExceeded');
      } else if (error_code == 'USER_DICTIONARY_BYTESIZE_EXCEEDED') {
        tooltip +=
            chrome.i18n.getMessage('configSyncUserDictionaryByteSizeExceeded');
      } else if (error_code == 'USER_DICTIONARY_NUM_DICTIONARY_EXCEEDED') {
        tooltip +=
            chrome.i18n.getMessage(
                'configSyncUserDictionaryNumDictionaryExceeded');
      } else {
        tooltip += chrome.i18n.getMessage('configSyncUnknownErrorFound');
      }
    }
    sync_message += ' ';
    var last_error_code = cloud_sync_status['sync_errors'][0]['error_code'];
      if (last_error_code == 'AUTHORIZATION_FAIL') {
        sync_message += chrome.i18n.getMessage('configSyncAuthorizationError');
      } else if (last_error_code == 'USER_DICTIONARY_NUM_ENTRY_EXCEEDED' ||
                 last_error_code == 'USER_DICTIONARY_BYTESIZE_EXCEEDED' ||
                 last_error_code == 'USER_DICTIONARY_NUM_DICTIONARY_EXCEEDED') {
        sync_message += chrome.i18n.getMessage('configSyncDictionaryError');
      } else {
        sync_message += chrome.i18n.getMessage('configSyncUnknownError');
      }
  }
  if (sync_status_div.innerHTML != sync_message) {
    sync_status_div.innerHTML = sync_message;
  }
  sync_status_div.title = tooltip;
};

/**
 * Sets the tabIndex of the all focusable elements (tabIndex >= 0) in the target
 * element -1 to make them unfocusable with tab key.
 * @param {string} elementId The ID of target element.
 * @private
 */
mozc.OptionPage.prototype.setChildNodesUnfocusableByTabKeyById_ =
    function(elementId) {
  var element = this.document_.getElementById(elementId);
  if (!element) {
    return;
  }
  this.setChildNodesUnfocusableByTabKey_(element);
};

/**
 * Resets the tabIndex of the all focusable elements in the target element which
 * was set to -1 with setChildNodesUnfocusableByTabKeyById_().
 * @param {string} elementId The ID of target element.
 * @private
 */
mozc.OptionPage.prototype.setChildNodesFocusableByTabKeyById_ =
    function(elementId) {
  var element = this.document_.getElementById(elementId);
  if (!element) {
    return;
  }
  this.setChildNodesFocusableByTabKey_(element);
};

/**
 * Sets the tabIndex of the all focusable elements (tabIndex >= 0) in the target
 * element -1 to make them unfocusable with tab key.
 * @param {!Element} element The target element.
 * @private
 */
mozc.OptionPage.prototype.setChildNodesUnfocusableByTabKey_ =
    function(element) {
  if (element.tabIndex >= 0) {
    element.oldTabIndex = element.tabIndex;
    element.tabIndex = -1;
  }
  if (!element.childNodes) {
    return;
  }
  for (var i = 0; i < element.childNodes.length; ++i) {
    this.setChildNodesUnfocusableByTabKey_(element.childNodes[i]);
  }
};

/**
 * Resets the tabIndex of the all focusable elements in the target element which
 * was set to -1 with setChildNodesUnfocusableByTabKey_().
 * @param {!Element} element The target element.
 * @private
 */
mozc.OptionPage.prototype.setChildNodesFocusableByTabKey_ = function(element) {
  if (element.oldTabIndex >= 0) {
    element.tabIndex = element.oldTabIndex;
    element.oldTabIndex = undefined;
  }
  if (!element.childNodes) {
    return;
  }
  for (var i = 0; i < element.childNodes.length; ++i) {
    this.setChildNodesFocusableByTabKey_(element.childNodes[i]);
  }
};

/**
 * Freezes the main div 'settings_div' to hide scroll bar.
 * @private
 */
mozc.OptionPage.prototype.freezeMainDiv_ = function() {
  var mainDiv = this.document_.getElementById('settings_div');
  if (mainDiv.classList.contains('frozen')) {
    return;
  }
  mainDiv.style.width = this.window_.getComputedStyle(mainDiv).width;
  mainDiv.oldScrollTop = this.document_.body.scrollTop;
  mainDiv.classList.add('frozen');
  var vertical_position =
      mainDiv.getBoundingClientRect().top - mainDiv.oldScrollTop;
  mainDiv.style.top = vertical_position + 'px';
  this.setChildNodesUnfocusableByTabKey_(mainDiv);
};

/**
 * Unfreezes the main div 'settings_div' to hide scroll bar.
 * @private
 */
mozc.OptionPage.prototype.unfreezeMainDiv_ = function() {
  var mainDiv = this.document_.getElementById('settings_div');
  if (!mainDiv.classList.contains('frozen')) {
    return;
  }
  this.setChildNodesFocusableByTabKey_(mainDiv);
  mainDiv.classList.remove('frozen');
  mainDiv.style.top = '';
  mainDiv.style.left = '';
  mainDiv.style.right = '';
  mainDiv.style.width = '';
  var scrollTop = mainDiv.oldScrollTop || 0;
  mainDiv.oldScrollTop = undefined;
  this.window_.scroll(0, scrollTop);
};

/**
 * Sends user dictionary command to NaCl module.
 * @param {!Object} command User dictionary command object to be sent.
 * @param {!function(Object)=} opt_callback Function to be called with results
 *     from NaCl module.
 * @private
 */
mozc.OptionPage.prototype.sendUserDictionaryCommand_ =
  function(command, opt_callback) {
  this.naclMozc_.sendUserDictionaryCommand(
      command, opt_callback);
};

