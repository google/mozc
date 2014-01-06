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
    id: 'clear_history_title',
    name: chrome.i18n.getMessage('configClearHistoryTitle')
  },
  {
    id: 'clear_history_message',
    name: chrome.i18n.getMessage('configClearHistoryMessage')
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
    id: 'dictionary_tool_dictionary_list_title',
    name: chrome.i18n.getMessage('dictionaryToolDictionaryListTitle')
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
  },
  {
    id: 'dictionary_tool_dictionary_menu_title',
    name: chrome.i18n.getMessage('dictionaryToolMenuTitle')
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
    id: 'dictionary_tool_open_button',
    name: chrome.i18n.getMessage('configDictionaryToolButton')
  },
  {
    id: 'export_dictionary_button',
    name: chrome.i18n.getMessage('dictionaryToolExportButton')
  },
  {
    id: 'import_dictionary_button',
    name: chrome.i18n.getMessage('dictionaryToolImportButton')
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
 * An empty constructor.
 * @param {!mozc.NaclMozc} naclMozc NaCl Mozc.
 * @param {!Window} optionWindow Window object of the option page.
 * @constructor
 * @struct
 * @const
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
   * Stack of Esc key handlers. An Esc key handler is pushed to it when an
   * dialog　box is opend and the handler will be popped when the dialog box is
   * closed. It is used to close the dialog when the user presses Esc key.
   * @type {!Array.<!Function>}
   * @private
   */
  this.escapeKeyHandlers_ = [];

  /**
   * Dictionary tool object.
   * @type {!mozc.DictionaryTool}
   * @private
   */
  this.dictionaryTool_ = new mozc.DictionaryTool(naclMozc, optionWindow, this);
};

/**
 * Initializes the option page.
 */
mozc.OptionPage.prototype.initialize = function() {
  this.window_.addEventListener('beforeunload',
                                this.unload_.bind(this),
                                true);
  if (!this.naclMozc_) {
    console.error('The option page is already unloaded.');
    return;
  }
  this.initPages_();
  this.naclMozc_.callWhenInitialized((function() {
    this.naclMozc_.getPosList((function(message) {
      this.dictionaryTool_.setPosList(message['posList']);
      this.naclMozc_.getConfig((function(response) {
        this.onConfigLoaded_(response);
        this.document_.body.style.visibility = 'visible';
      }).bind(this));
    }).bind(this));
    // Outputs the version information to JavaScript console.
    this.naclMozc_.getVersionInfo((function(message) {
      this.console_.log(JSON.stringify(message));
    }).bind(this));
  }).bind(this));
};

/**
 * Unloads the option page.
 * @private
 */
mozc.OptionPage.prototype.unload_ = function() {
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
 */
mozc.OptionPage.prototype.createOptionElement = function(name, value) {
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
      selectionElement.appendChild(this.createOptionElement(
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
      selectionElement.appendChild(this.createOptionElement(j, j));
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

  // Disables clear_history_ok button according to check boxes in Clear History
  // dialog.
  this.document_.getElementById('clear_history_conversion_history')
      .addEventListener('change',
                        this.updateClearHistoryOkButton_.bind(this),
                        true);
  this.document_.getElementById('clear_history_suggestion_history')
      .addEventListener('change',
                        this.updateClearHistoryOkButton_.bind(this),
                        true);

  this.document_.getElementById('dictionary_tool_dictionary_list')
      .addEventListener(
          'keydown',
          this.dictionaryTool_.onDictionaryToolDictionaryListKeyDown.bind(
              this.dictionaryTool_),
          true);
  var newReadingInput =
      this.document_.getElementById('dictionary_tool_reading_new_input');
  newReadingInput.addEventListener(
      'input',
      this.dictionaryTool_.onDictionaryReadingInputChanged.bind(
          this.dictionaryTool_, newReadingInput),
      true);
  newReadingInput.addEventListener(
      'blur',
      this.dictionaryTool_.onDictionaryNewEntryLostFocus.bind(
          this.dictionaryTool_),
      true);
  this.document_.getElementById('dictionary_tool_word_new_input')
      .addEventListener(
          'blur',
          this.dictionaryTool_.onDictionaryNewEntryLostFocus.bind(
              this.dictionaryTool_),
          true);
  this.document_.getElementById('dictionary_tool_category_new_select')
      .addEventListener(
          'blur',
          this.dictionaryTool_.onDictionaryNewEntryLostFocus.bind(
              this.dictionaryTool_),
          true);
  this.document_.getElementById('dictionary_tool_comment_new_input')
      .addEventListener(
          'blur',
          this.dictionaryTool_.onDictionaryNewEntryLostFocus.bind(
              this.dictionaryTool_),
          true);

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
 * Pop Esc key handler.
 */
mozc.OptionPage.prototype.popEscapeKeyHandler = function() {
  this.escapeKeyHandlers_.pop();
};

/**
 * Push Esc key handler.
 * @param {!Function} handler Esc key handler.
 */
mozc.OptionPage.prototype.pushEscapeKeyHandler = function(handler) {
  this.escapeKeyHandlers_.push(handler);
};

/**
 * Enables the dom element which is specified by id in the option page.
 * @param {string} id The element ID.
 */
mozc.OptionPage.prototype.enableElementById = function(id) {
  this.document_.getElementById(id).disabled = false;
};

/**
 * Disables the dom element which is specified by id in the option page.
 * @param {string} id The element ID.
 */
mozc.OptionPage.prototype.disableElementById = function(id) {
  this.document_.getElementById(id).disabled = true;
};

/**
 * Shows the dom element which is specified by id in the option page.
 * @param {string} id The element ID.
 * @private
 */
mozc.OptionPage.prototype.showElementById_ = function(id) {
  this.document_.getElementById(id).style.display = 'inline';
};

/**
 * Hides the dom element which is specified by id in the option page.
 * @param {string} id The element ID.
 * @private
 */
mozc.OptionPage.prototype.hideElementById_ = function(id) {
  this.document_.getElementById(id).style.display = 'none';
};

/**
 * Shows the overlay element which is specified by id in the option page.
 * @param {string} id The element ID.
 */
mozc.OptionPage.prototype.showOverlayElementById = function(id) {
  this.freezeMainDiv_();
  this.document_.getElementById(id).style.visibility = 'visible';
};

/**
 * Hides the overlay element which is specified by id in the option page.
 * @param {string} id The element ID.
 */
mozc.OptionPage.prototype.hideOverlayElementById = function(id) {
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
 */
mozc.OptionPage.prototype.showConfirm = function(callerPageId,
                                                 title,
                                                 message,
                                                 callback) {
  this.setChildNodesUnfocusableByTabKeyById(callerPageId);
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

  confirmOkCancelDiv.appendChild(confirmOkButton);
  confirmOkCancelDiv.appendChild(confirmCancelButton);
  section.appendChild(confirmTitle);
  section.appendChild(confirmMessage);
  section.appendChild(confirmOkCancelDiv);
  confirmPage.appendChild(closeDiv);
  confirmPage.appendChild(section);
  confirmOverlay.appendChild(confirmPage);

  var okCallback =
      (function(overlay, pageId, callbackFunc) {
        this.popEscapeKeyHandler();
        this.document_.body.removeChild(overlay);
        this.setChildNodesFocusableByTabKeyById(pageId);
        callbackFunc(true);
      }).bind(this, confirmOverlay, callerPageId, callback);
  var cancelCallback =
      (function(overlay, pageId, callbackFunc) {
        this.popEscapeKeyHandler();
        this.document_.body.removeChild(overlay);
        this.setChildNodesFocusableByTabKeyById(pageId);
        callbackFunc(false);
      }).bind(this, confirmOverlay, callerPageId, callback);
  closeDiv.addEventListener('click', cancelCallback, true);
  confirmOkButton.addEventListener('click', okCallback, true);
  confirmCancelButton.addEventListener('click', cancelCallback, true);
  this.pushEscapeKeyHandler(cancelCallback);
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
 */
mozc.OptionPage.prototype.showPrompt = function(callerPageId,
                                                title,
                                                message,
                                                defaultValue,
                                                callback) {
  this.setChildNodesUnfocusableByTabKeyById(callerPageId);
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

  promptOkCancelDiv.appendChild(promptOkButton);
  promptOkCancelDiv.appendChild(promptCancelButton);
  section.appendChild(promptTitle);
  section.appendChild(promptMessage);
  section.appendChild(promptInputDiv);
  section.appendChild(promptOkCancelDiv);
  promptPage.appendChild(closeDiv);
  promptPage.appendChild(section);
  promptOverlay.appendChild(promptPage);

  var okCallback =
      (function(overlay, pageId, callbackFunc, input) {
        this.popEscapeKeyHandler();
        this.document_.body.removeChild(overlay);
        this.setChildNodesFocusableByTabKeyById(pageId);
        callbackFunc(input.value);
      }).bind(this, promptOverlay, callerPageId, callback, promptInput);
  var cancelCallback =
      (function(overlay, pageId, callbackFunc) {
        this.popEscapeKeyHandler();
        this.document_.body.removeChild(overlay);
        this.setChildNodesFocusableByTabKeyById(pageId);
        callbackFunc(null);
      }).bind(this, promptOverlay, callerPageId, callback);
  closeDiv.addEventListener('click', cancelCallback, true);
  promptOkButton.addEventListener('click', okCallback, true);
  promptCancelButton.addEventListener('click', cancelCallback, true);
  this.pushEscapeKeyHandler(cancelCallback);
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
 */
mozc.OptionPage.prototype.showAlert = function(callerPageId,
                                               title,
                                               message,
                                               opt_callback) {
  this.setChildNodesUnfocusableByTabKeyById(callerPageId);
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
        this.popEscapeKeyHandler();
        this.document_.body.removeChild(overlay);
        this.setChildNodesFocusableByTabKeyById(pageId);
        if (opt_callbackFunc) {
          opt_callbackFunc();
        }
      }).bind(this, alertOverlay, callerPageId, opt_callback);
  closeDiv.addEventListener('click', okCallback, true);
  alertOkButton.addEventListener('click', okCallback, true);

  this.pushEscapeKeyHandler(okCallback);
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
  } else if (buttonId == 'dictionary_tool_open_button') {
    this.dictionaryTool_.onDictionaryToolOpenButtonClicked();
  } else if (buttonId == 'dictionary_tool_done_button') {
    this.dictionaryTool_.onDictionaryToolDoneButtonClicked();
  } else if (buttonId == 'dictionary_tool_close') {
    this.dictionaryTool_.onDictionaryToolDoneButtonClicked();
  } else if (buttonId == 'export_dictionary_button') {
    this.dictionaryTool_.onDictionaryToolExportButtonClicked();
  } else if (buttonId == 'import_dictionary_button') {
    this.dictionaryTool_.onDictionaryToolImportButtonClicked();
  }
};

/**
 * Called when clear_history_open button is clicked.
 * This method opens clear history dialog.
 * @private
 */
mozc.OptionPage.prototype.onClearHistoryOpenClicked_ = function() {
  this.pushEscapeKeyHandler(this.onClearHistoryCancelClicked_.bind(this));
  this.document_.getElementById('clear_history_conversion_history').checked =
      true;
  this.document_.getElementById('clear_history_suggestion_history').checked =
      true;
  this.showOverlayElementById('clear_history_overlay');
  this.document_.getElementById('clear_history_cancel').focus();
};

/**
 * Disables clear_history_ok button according to check boxes in Clear History
 * dialog.
 * @private
 */
mozc.OptionPage.prototype.updateClearHistoryOkButton_ = function() {
  this.document_.getElementById('clear_history_ok').disabled =
      !this.document_.getElementById('clear_history_conversion_history')
          .checked &&
      !this.document_.getElementById('clear_history_suggestion_history')
          .checked;
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
  this.popEscapeKeyHandler();
  this.hideOverlayElementById('clear_history_overlay');
  this.document_.getElementById('clear_history_open').focus();
};

/**
 * Called when clear_history_cancel button or clear_history_close button is
 * clicked or the user presses Esc key while clear history dialog is opened.
 * This method closes the clear history dialog.
 * @private
 */
mozc.OptionPage.prototype.onClearHistoryCancelClicked_ = function() {
  this.popEscapeKeyHandler();
  this.hideOverlayElementById('clear_history_overlay');
  this.document_.getElementById('clear_history_open').focus();
};

/**
 * Sets the tabIndex of the all focusable elements (tabIndex >= 0) in the target
 * element -1 to make them unfocusable with tab key.
 * @param {string} elementId The ID of target element.
 */
mozc.OptionPage.prototype.setChildNodesUnfocusableByTabKeyById =
    function(elementId) {
  var element = this.document_.getElementById(elementId);
  if (!element) {
    return;
  }
  this.setChildNodesUnfocusableByTabKey_(element);
};

/**
 * Resets the tabIndex of the all focusable elements in the target element which
 * was set to -1 with setChildNodesUnfocusableByTabKeyById().
 * @param {string} elementId The ID of target element.
 */
mozc.OptionPage.prototype.setChildNodesFocusableByTabKeyById =
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
  var scrollTop = mainDiv.oldScrollTop || 0;
  mainDiv.oldScrollTop = undefined;
  this.window_.scroll(0, scrollTop);
};
