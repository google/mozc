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
 * @type {Array.<!Object>}
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
  }
];

/**
 * Option checkbox information data.
 * @const
 * @type {Array.<!Object>}
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
 * @type {Array.<!Object>}
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
    id: 'history_learning_level',
    configId: 'history_learning_level',
    name: chrome.i18n.getMessage('configHistoryLarningLevel'),
    items: [
      {
        name: chrome.i18n.getMessage('configHistoryLarningLevelDefault'),
        value: 'DEFAULT_HISTORY'
      },
      {
        name: chrome.i18n.getMessage('configHistoryLarningLevelReadOnly'),
        value: 'READ_ONLY'
      },
      {
        name: chrome.i18n.getMessage('configHistoryLarningLevelOff'),
        value: 'NO_HISTORY'
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
 * @type {Array.<!Object>}
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
 * @type {Array.<!Object>}
 * @private
 */
mozc.OPTION_BUTTONS_ = [
  {
    id: 'clear_user_history',
    name: chrome.i18n.getMessage('configClearUserHistory')
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
    id: 'sync_toggle_button',
    name: chrome.i18n.getMessage('configSyncStartSync')
  },
  {
    id: 'sync_customization_button',
    name: chrome.i18n.getMessage('configSyncCustomization')
  }
];

/**
 * Option sync checkbox information data.
 * @const
 * @type {Array.<!Object>}
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
 * An empty constructor.
 * @param {!mozc.NaclMozc} naclMozc NaCl Mozc.
 * @param {!HTMLDocument} domDocument Document object of the option page.
 * @param {!Object} consoleObject Console object of the option page.
 * @constructor
 */
mozc.OptionPage = function(naclMozc, domDocument, consoleObject) {
  /**
   * NaclMozc object.
   * This value will be null when the option page is unloaded.
   * @type {mozc.NaclMozc}
   * @private
   */
  this.naclMozc_ = naclMozc;

  /**
   * Document object of the option page.
   * This value will be null when the option page is unloaded.
   * @type {HTMLDocument}
   * @private
   */
  this.document_ = domDocument;

  /**
   * Console object of the option page.
   * This value will be null when the option page is unloaded.
   * @type {Object}
   * @private
   */
  this.console_ = consoleObject;

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
    this.naclMozc_.getConfig(this.onConfigLoaded_.bind(this));
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

  // A checkbox (id:"CHECK_BOX_ID") is in a DIV (id:"CHECK_BOX_ID_div") and has
  // a label (id:"CHECK_BOX_ID_label").
  // <div id="CHECK_BOX_ID_div">
  //   <span>
  //     <input type="checkbox" id="CHECK_BOX_ID">
  //     <span>
  //       <label for="CHECK_BOX_ID" id="CHECK_BOX_ID_label">...</label>
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

  // A selection (id:"SELECTION_ID") is in a DIV (id:"SELECTION_ID_div") and has
  // a label (id:"SELECTION_ID_label")
  // <div id="SELECTION_ID_div">
  //   <span class="controlled-setting-with-label">
  //     <span class="selection-label" id="SELECTION_ID_label">...</span>
  //     <select id="SELECTION_ID"></select>
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
  // A selection (id:"NUMBER_ID") is in a DIV (id:"NUMBER_ID_div") and has a
  // label (id:"NUMBER_ID_label")
  // <div id="NUMBER_ID_div">
  //   <span class="controlled-setting-with-label">
  //     <span class="selection-label" id="NUMBER_ID_label">...</span>
  //     <select id="NUMBER_ID"></select>
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

  // A button (id:"BUTTON_ID") is in a DIV (id:"BUTTON_ID_div").
  // <div id="BUTTON_ID_div">
  //   <span class="controlled-setting-with-label">
  //     <input type="button" id="BUTTON_ID" value="...">
  //   </span>
  // </div>
  for (var i = 0; i < mozc.OPTION_BUTTONS_.length; ++i) {
    var optionButton = mozc.OPTION_BUTTONS_[i];
    var buttonElement = this.document_.getElementById(optionButton.id);
    buttonElement.value = optionButton.name;
    buttonElement.addEventListener('click',
                                   this.onButtonClick_.bind(this,
                                                            optionButton.id),
                                   true);
  }

  // A sync checkbox (id:"CHECK_BOX_ID") has a label (id:"CHECK_BOX_ID_label").
  // <span>
  //   <input type="checkbox" id="CHECK_BOX_ID">
  //   <span>
  //     <label for="CHECK_BOX_ID" id="CHECK_BOX_ID_label">...</label>
  //   </span>
  // </span>
  for (var i = 0; i < mozc.OPTION_SYNC_CHECKBOXES_.length; ++i) {
    var optionCheckbox = mozc.OPTION_SYNC_CHECKBOXES_[i];
    this.document_.getElementById(optionCheckbox.id + '_label').innerHTML =
        optionCheckbox.name;
  }

  // Removes cloud_sync_div if cloud sync is not enabled.
  if (!mozc.ENABLE_CLOUD_SYNC_) {
    this.document_.getElementById('settings_div').removeChild(
      this.document_.getElementById('cloud_sync_div'));
  }
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
 * Shows the dom element which is specified by id in the option page.
 * @param {string} id The element ID.
 * @private
 */
mozc.OptionPage.prototype.showElementById_ = function(id) {
  this.document_.getElementById(id).style.visibility = 'visible';
};

/**
 * Hides the dom element which is specified by id in the option page.
 * @param {string} id The element ID.
 * @private
 */
mozc.OptionPage.prototype.hideElementById_ = function(id) {
  this.document_.getElementById(id).style.visibility = 'hidden';
};

/**
 * Called when a button is clicked.
 * @param {string} buttonId The clicked button ID.
 * @private
 */
mozc.OptionPage.prototype.onButtonClick_ = function(buttonId) {
  if (buttonId == 'clear_user_history') {
    this.onClearUserHistoryClicked_();
  } else if (buttonId == 'sync_config_cancel') {
    this.onSyncConfigCancelClicked_();
  } else if (buttonId == 'sync_config_ok') {
    this.onSyncConfigOkClicked_();
  } else if (buttonId == 'sync_toggle_button') {
    this.onSyncToggleButtonClicked_();
  } else if (buttonId == 'sync_customization_button') {
    this.onSyncCustomizationButtonClicked_();
  }
};

/**
 * Called when clear_user_history button is clicked.
 * @private
 */
mozc.OptionPage.prototype.onClearUserHistoryClicked_ = function() {
  if (!confirm(chrome.i18n.getMessage('configClearUserHistoryMessage'))) {
    return;
  }
  this.disableElementById_('clear_user_history');
  this.naclMozc_.clearUserHistory((function(response) {
    this.naclMozc_.clearUserPrediction((function(response) {
      this.enableElementById_('clear_user_history');
    }).bind(this));
  }).bind(this));
};

/**
 * Called when sync_config_cancel button is clicked.
 * @private
 */
mozc.OptionPage.prototype.onSyncConfigCancelClicked_ = function() {
  this.naclMozc_.getCloudSyncStatus((function(res) {
    this.displaySyncStatus_(res);
    this.hideElementById_('sync_config_overlay');
  }).bind(this));
};

/**
 * Called when sync_config_ok button is clicked.
 * @private
 */
mozc.OptionPage.prototype.onSyncConfigOkClicked_ = function() {
  if (!this.document_.getElementById('sync_settings').checked &&
      !this.document_.getElementById('sync_user_dictionary').checked) {
    // Stop sync
    this.naclMozc_.addAuthCode(
        {'access_token': ''},
        (function() {
          this.naclMozc_.getCloudSyncStatus((function(res) {
            this.displaySyncStatus_(res);
            this.hideElementById_('sync_config_overlay');
          }).bind(this));
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
    this.naclMozc_.setConfig(
        config,
        (function() {
          this.naclMozc_.sendReload();
          this.getAuthTokenAndStartCloudSync_(
            this.naclMozc_.getCloudSyncStatus.bind(
                this.naclMozc_,
                (function(res) {
                  this.displaySyncStatus_(res);
                  this.hideElementById_('sync_config_overlay');
                }).bind(this)));
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
      (function(token) {
        this.naclMozc_.addAuthCode(
            {'access_token': token},
            this.naclMozc_.startCloudSync.bind(this.naclMozc_, opt_callback));
      }).bind(this));
};

/**
 * Called when sync_toggle_button button is clicked.
 * @private
 */
mozc.OptionPage.prototype.onSyncToggleButtonClicked_ = function() {
  this.disableElementById_('sync_toggle_button');
  this.disableElementById_('sync_customization_button');
  this.naclMozc_.getCloudSyncStatus((function(response) {
    if (response['output']['cloud_sync_status']['global_status'] == 'NOSYNC') {
      this.document_.getElementById('sync_settings').checked = true;
      this.document_.getElementById('sync_user_dictionary').checked = true;
      this.showElementById_('sync_config_overlay');
    } else {
      // Stop sync
      this.naclMozc_.addAuthCode(
          {'access_token': ''},
          (function() {
            this.naclMozc_.getCloudSyncStatus(
                this.displaySyncStatus_.bind(this));
          }).bind(this));
    }
  }).bind(this));
};

/**
 * Called when sync_customization_button button is clicked.
 * @private
 */
mozc.OptionPage.prototype.onSyncCustomizationButtonClicked_ = function() {
  this.disableElementById_('sync_toggle_button');
  this.disableElementById_('sync_customization_button');
  this.naclMozc_.getConfig((function(response) {
    var sync_config = response['output']['config']['sync_config'];
    this.document_.getElementById('sync_settings').checked =
        sync_config && sync_config['use_config_sync'];
    this.document_.getElementById('sync_user_dictionary').checked =
        sync_config && sync_config['use_user_dictionary_sync'];
    this.showElementById_('sync_config_overlay');
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
  if (sync_global_status == 'SYNC_SUCCESS' ||
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
    this.document_.getElementById('sync_toggle_button').value =
        chrome.i18n.getMessage('configSyncStopSync');
    this.enableElementById_('sync_toggle_button');
    this.enableElementById_('sync_customization_button');
  } else if (sync_global_status == 'WAITSYNC') {
    this.document_.getElementById('sync_toggle_button').value =
        chrome.i18n.getMessage('configSyncStopSync');
    sync_message += chrome.i18n.getMessage('configSyncWaiting');
    this.disableElementById_('sync_toggle_button');
    this.disableElementById_('sync_customization_button');
  } else if (sync_global_status == 'INSYNC') {
    this.document_.getElementById('sync_toggle_button').value =
        chrome.i18n.getMessage('configSyncStopSync');
    sync_message += chrome.i18n.getMessage('configSyncDuringSync');
    this.disableElementById_('sync_toggle_button');
    this.disableElementById_('sync_customization_button');
  } else if (sync_global_status == 'NOSYNC') {
    this.document_.getElementById('sync_toggle_button').value =
        chrome.i18n.getMessage('configSyncStartSync');
    sync_message += chrome.i18n.getMessage('configSyncNoSync');
    this.enableElementById_('sync_toggle_button');
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
                'configSyncUserDictionaryNumDicrionaryExceeded');
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
