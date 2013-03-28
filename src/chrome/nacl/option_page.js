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
 * An empty constructor.
 * @param {!mozc.NaclMozc} naclMozc NaCl Mozc.
 * @param {!HTMLDocument} domDocument Document object of the option page.
 * @constructor
 */
mozc.OptionPage = function(naclMozc, domDocument) {
  /**
   * NaclMozc object.
   * @type {mozc.NaclMozc}
   * @private
   */
  this.naclMozc_ = naclMozc;

  /**
   * Document object of the option page.
   * @type {!HTMLDocument}
   * @private
   */
  this.document_ = domDocument;
};

/**
 * Initializes the option page.
 */
mozc.OptionPage.prototype.initialize = function() {
  this.naclMozc_.callWhenInitialized(
      this.naclMozc_.getConfig.bind(
          this.naclMozc_,
          this.initPages_.bind(this))
  );
};

/**
 * Creates an TR element including title string and element.
 * @param {string} title Title string.
 * @param {!Element} element Element object to be inserted.
 * @return {!Element} Created TR element.
 * @private
 */
mozc.OptionPage.prototype.createTrElement_ = function(title, element) {
  var tr = this.document_.createElement('tr');
  var labelTh = this.document_.createElement('th');
  labelTh.appendChild(this.document_.createTextNode(title));
  var elementTd = this.document_.createElement('td');
  elementTd.appendChild(element);
  tr.appendChild(labelTh);
  tr.appendChild(elementTd);
  return tr;
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
 * @param {Object} response Response data of GET_CONFIG from NaCl module.
 * @private
 */
mozc.OptionPage.prototype.initPages_ = function(response) {
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
    var checkbox = this.document_.createElement('input');
    checkbox.type = 'checkbox';
    checkbox.id = optionCheckbox.id;
    checkbox.checked = value;
    checkbox.addEventListener('change', this.saveConfig_.bind(this), true);
    var label = this.document_.createElement('label');
    label.setAttribute('for', optionCheckbox.id);
    label.appendChild(this.document_.createTextNode(optionCheckbox.name));
    var checkboxDiv = this.document_.createElement('div');
    checkboxDiv.appendChild(checkbox);
    checkboxDiv.appendChild(label);
    this.document_.body.appendChild(checkboxDiv);
  }

  var optionTable = this.document_.createElement('table');

  for (var i = 0; i < mozc.OPTION_SELECTIONS_.length; ++i) {
    var optionSelection = mozc.OPTION_SELECTIONS_[i];
    var value = (config[optionSelection.configId] != undefined) ?
                config[optionSelection.configId] :
                mozc.DEFAULT_CONFIG_[optionSelection.configId];
    var select = this.document_.createElement('select');
    select.id = optionSelection.id;
    select.addEventListener('change', this.saveConfig_.bind(this), true);
    for (var j = 0; j < optionSelection.items.length; ++j) {
      select.appendChild(this.createOptionElement_(
          optionSelection.items[j].name,
          optionSelection.items[j].value));
      if (optionSelection.items[j].value == value) {
        select.selectedIndex = j;
      }
    }
    optionTable.appendChild(this.createTrElement_(
        optionSelection.name, select));
  }

  for (var i = 0; i < mozc.OPTION_NUMBERS_.length; ++i) {
    var optionNumber = mozc.OPTION_NUMBERS_[i];
    var value = (config[optionNumber.configId] != undefined) ?
                config[optionNumber.configId] :
                mozc.DEFAULT_CONFIG_[optionNumber.configId];
    var number = this.document_.createElement('input');
    number.id = optionNumber.id;
    number.min = optionNumber.min;
    number.max = optionNumber.max;
    number.type = 'number';
    number.value = value;
    number.addEventListener('change', this.saveConfig_.bind(this), true);
    optionTable.appendChild(this.createTrElement_(optionNumber.name, number));
  }
  this.document_.body.appendChild(optionTable);

  this.document_.body.appendChild(this.document_.createElement('hr'));
  var creditDiv = this.document_.createElement('div');
  creditDiv.innerHTML = chrome.i18n.getMessage('configCreditsDescription');
  this.document_.body.appendChild(creditDiv);
  var ossCreditDiv = this.document_.createElement('div');
  ossCreditDiv.innerHTML =
      chrome.i18n.getMessage('configOssCreditsDescription');
  this.document_.body.appendChild(ossCreditDiv);
};

/**
 * Saves the configuration.
 * @private
 */
mozc.OptionPage.prototype.saveConfig_ = function() {
  var config = {};
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
  console.log(config);
  this.naclMozc_.setConfig(
      config,
      (function() {this.naclMozc_.sendReload()}).bind(this));
};
