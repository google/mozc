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
 * @fileoverview This file contains NaclMozc dictionary tool implementation.
 *
 * TODO(horo): Refactor the dependency to OptionPage.
 *
 */

'use strict';

/**
 * User dictionary entry item types.
 * @const
 * @enum {string}
 * @private
 */
mozc.USER_DICTIONARY_ENTRY_ITEM_TYPE_ = {
  READING: 'key',
  WORD: 'value',
  CATEGORY: 'pos',
  COMMENT: 'comment'
};

/**
 * User dictionary entry item structure information.
 * @const
 * @type {!Array.<!Object.<{type: mozc.USER_DICTIONARY_ENTRY_ITEM_TYPE_,
 *                          divElementClass: string,
 *                          inputElementClass: string,
 *                          inputElementType: string}>>}
 * @private
 */
mozc.USER_DICTIONARY_ENTRY_ITEM_INFO_ = [
  {
    type: mozc.USER_DICTIONARY_ENTRY_ITEM_TYPE_.READING,
    divElementClass: 'dictionary_tool_reading',
    inputElementClass: 'dictionary_tool_reading_input',
    inputElementType: 'text'
  },
  {
    type: mozc.USER_DICTIONARY_ENTRY_ITEM_TYPE_.WORD,
    divElementClass: 'dictionary_tool_word',
    inputElementClass: 'dictionary_tool_word_input',
    inputElementType: 'text'
  },
  {
    type: mozc.USER_DICTIONARY_ENTRY_ITEM_TYPE_.CATEGORY,
    divElementClass: 'dictionary_tool_category',
    inputElementClass: 'dictionary_tool_category_input',
    inputElementType: 'select'
  },
  {
    type: mozc.USER_DICTIONARY_ENTRY_ITEM_TYPE_.COMMENT,
    divElementClass: 'dictionary_tool_comment',
    inputElementClass: 'dictionary_tool_comment_input',
    inputElementType: 'text'
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
      chrome.i18n.getMessage('dictionaryToolStatusErrorFileNotFound'),
  'INVALID_FILE_FORMAT':
      chrome.i18n.getMessage('dictionaryToolStatusErrorInvalidFileFormat'),
  'FILE_SIZE_LIMIT_EXCEEDED':
      chrome.i18n.getMessage('dictionaryToolStatusErrorFileSizeLimitExceeded'),
  'DICTIONARY_SIZE_LIMIT_EXCEEDED':
      chrome.i18n.getMessage(
          'dictionaryToolStatusErrorDictionarySizeLimitExceeded'),
  'ENTRY_SIZE_LIMIT_EXCEEDED':
      chrome.i18n.getMessage('dictionaryToolStatusErrorEntrySizeLimitExceeded'),
  'DICTIONARY_NAME_EMPTY':
      chrome.i18n.getMessage('dictionaryToolStatusErrorDictionaryNameEmpty'),
  'DICTIONARY_NAME_TOO_LONG':
      chrome.i18n.getMessage('dictionaryToolStatusErrorDictionaryNameTooLong'),
  'DICTIONARY_NAME_CONTAINS_INVALID_CHARACTER':
      chrome.i18n.getMessage(
          'dictionaryToolStatusErrorDictionaryNameContainsInvalidCharacter'),
  'DICTIONARY_NAME_DUPLICATED':
      chrome.i18n.getMessage(
          'dictionaryToolStatusErrorDictionaryNameDuplicated'),
  'READING_EMPTY':
      chrome.i18n.getMessage('dictionaryToolStatusErrorReadingEmpty'),
  'READING_TOO_LONG':
      chrome.i18n.getMessage('dictionaryToolStatusErrorReadingTooLong'),
  'READING_CONTAINS_INVALID_CHARACTER':
      chrome.i18n.getMessage(
          'dictionaryToolStatusErrorReadingContainsInvalidCharacter'),
  'WORD_EMPTY': chrome.i18n.getMessage('dictionaryToolStatusErrorWordEmpty'),
  'WORD_TOO_LONG':
      chrome.i18n.getMessage('dictionaryToolStatusErrorWordTooLong'),
  'WORD_CONTAINS_INVALID_CHARACTER':
      chrome.i18n.getMessage(
          'dictionaryToolStatusErrorWordContainsInvalidCharacter'),
  'IMPORT_TOO_MANY_WORDS':
      chrome.i18n.getMessage('dictionaryToolStatusErrorImportTooManyWords'),
  'IMPORT_INVALID_ENTRIES':
      chrome.i18n.getMessage('dictionaryToolStatusErrorImportInvalidEntries'),
  'NO_UNDO_HISTORY':
      chrome.i18n.getMessage('dictionaryToolStatusErrorNoUndoHistory'),
  'EMPTY_FILE':
      chrome.i18n.getMessage('dictionaryToolStatusErrorEmptyFile')
};

/**
 * Dictionary tool error message titles.
 * @const
 * @type {!Object.<string, string>}
 * @private
 */
mozc.DICTIONARY_TOOL_STATUS_ERROR_TITLES_ = {
  CREATE_DICTIONARY:
      chrome.i18n.getMessage('dictionaryToolStatusErrorTitleCreateDictionary'),
  DELETE_DICTIONARY:
      chrome.i18n.getMessage('dictionaryToolStatusErrorTitleDeleteDictionary'),
  RENAME_DICTIONARY:
      chrome.i18n.getMessage('dictionaryToolStatusErrorTitleRenameDictionary'),
  EDIT_ENTRY:
      chrome.i18n.getMessage('dictionaryToolStatusErrorTitleEditEntry'),
  DELETE_ENTRY:
      chrome.i18n.getMessage('dictionaryToolStatusErrorTitleDeleteEntry'),
  ADD_ENTRY:
      chrome.i18n.getMessage('dictionaryToolStatusErrorTitleAddEntry'),
  IMPORT_DATA:
      chrome.i18n.getMessage('dictionaryToolStatusErrorTitleImportData')
};

/**
 * An empty constructor.
 * @param {!mozc.NaclMozc} naclMozc NaCl Mozc.
 * @param {!Window} optionWindow Window object of the option page.
 * @param {!mozc.OptionPage} optionPage Option page object.
 * @constructor
 * @struct
 * @const
 */
mozc.DictionaryTool = function(naclMozc, optionWindow, optionPage) {
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
   * Option page object.
   * @type {!mozc.OptionPage}
   * @private
   */
  this.optionPage_ = optionPage;

  /**
   * Document object of the option page.
   * This value will be null when the option page is unloaded.
   * @type {Document}
   * @private
   */
  this.document_ = optionWindow.document;

  /**
   * User dictionary session id created in NaCl module. It is set when
   * dictionary tool is opened.
   * @type {string}
   * @private
   */
  this.userDictionarySessionId_ = '';

  /**
   * Pos type list. (example: {type: 'NOUN', name: '名詞'})
   * @type {!Array.<!mozc.PosType>}
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
   * Dictionary objects.
   * @type {!Array.<!mozc.UserDictionary>}
   * @private
   */
  this.dictionaries_ = [];

  /**
   * Selected dictionary index or NaN.
   * @type {number}
   * @private
   */
  this.selectedDictionaryIndex_ = NaN;
};

/**
 * Sets the pos list.
 * @param {!Array.<!mozc.PosType>} posList pos list.
 */
mozc.DictionaryTool.prototype.setPosList = function(posList) {
  this.posList_ = posList;
  var new_category_select =
    this.document_.getElementById('dictionary_tool_category_new_select');
  for (var i = 0; i < posList.length; ++i) {
    this.posNameMap_[posList[i].type] = posList[i].name;
    new_category_select.appendChild(
        this.optionPage_.createOptionElement(posList[i].name, posList[i].type));
  }
};

/**
 * Called when dictionary_tool_open_button button is clicked.
 * This method creates a dictionary session and calls LOAD dictionary command
 * and gets the user dictionary storage from NaCl Mozc module and shows a
 * dictionary tool.
 */
mozc.DictionaryTool.prototype.onDictionaryToolOpenButtonClicked = function() {
  this.optionPage_.disableElementById('dictionary_tool_open_button');
  this.sendUserDictionaryCommand_(
    this.createUserDictionaryCommand_('CREATE_SESSION'),
    /**
    * @this {!mozc.DictionaryTool}
    * @param {!mozc.UserDictionaryCommandStatus} response
    */
    (function(response) {
      if (response.session_id == undefined) {
        console.error('CREATE_SESSION error: session_id is undefined');
      } else {
        this.userDictionarySessionId_ = response.session_id;
      }
      var command = this.createUserDictionaryCommandWithId_('LOAD');
      this.sendUserDictionaryCommand_(
        command,
        this.loadStorage_.bind(
            this,
            /** @this {!mozc.DictionaryTool} */
            (function() {
              this.optionPage_.pushEscapeKeyHandler(
                  this.onDictionaryToolEscapeKeyPressed_.bind(this));
              this.optionPage_.showOverlayElementById(
                  'dictionary_tool_overlay');
              this.document_.getElementById('dictionary_tool_reading_new_input')
                  .focus();
            }).bind(this)));
    }).bind(this));
};

/**
 * Returns the name of the dictionary in the specified index.
 * @param {number} index Index of the dictionary.
 * @return {?string} The name of the dictionary or null when index is invalid.
 * @private
 */
mozc.DictionaryTool.prototype.getDictionaryName_ = function(index) {
  if (index < 0 || this.dictionaries_.length <= index) {
    return null;
  }
  var dictionaryName = this.dictionaries_[index].name;
  if (!dictionaryName) {
    return null;
  }
  return dictionaryName;
};

/**
 * Returns the dictionary index of the name.
 * @param {string} name The name of the dictionary.
 * @return {number} The found dictionary index or NaN
 * @private
 */
mozc.DictionaryTool.prototype.getDictionaryIndexByName_ = function(name) {
  for (var i = 0; i < this.dictionaries_.length; ++i) {
    if (this.dictionaries_[i].name == name) {
      return i;
    }
  }
  return NaN;
};

/**
 * Updates the dictionary list in 'dictionary_tool_dictionary_list' according to
 * dictionaries_.
 * @private
 */
mozc.DictionaryTool.prototype.updateDictionaryList_ = function() {
  var dictionaryList =
      this.document_.getElementById('dictionary_tool_dictionary_list');
  while (dictionaryList.hasChildNodes()) {
    dictionaryList.removeChild(dictionaryList.firstChild);
  }
  for (var i = 0; i < this.dictionaries_.length; ++i) {
    var dictionaryName = this.getDictionaryName_(i) || '';
    var selected = this.getSelectedDictionaryIndex_() == i;
    dictionaryList.appendChild(
        this.createUserDictionaryListItemElement_(i,
                                                  dictionaryName,
                                                  selected));
  }
  dictionaryList.appendChild(this.createUserDictionaryListNewItemElement_());
};

/**
 * Creates a dictionary list item like the followings.
 * <div id="dictionary_tool_dictionary_INDEX"
 *      class="dictionary_tool_dictionary">
 *   <div class="dictionary_tool_dictionary_name_area">
 *     <div class="dictionary_tool_dictionary_name">DICTIONARY_NAME</div>
 *   </div>
 *   <div class="dictionary_tool_dictionary_delete_button_div">
 *     <button class="dictionary_tool_dictionary_delete_button"></button>
 *   </div>
 * </div>
 * @param {number} index Index of the dictionary.
 * @param {string} dictionaryName The name of the dictionary.
 * @param {boolean} selected The dictionary is selected or not.
 * @return {!Element} Created element.
 * @private
 */
mozc.DictionaryTool.prototype.createUserDictionaryListItemElement_ =
    function(index, dictionaryName, selected) {
  var dictionaryDiv = this.document_.createElement('div');
  dictionaryDiv.id = 'dictionary_tool_dictionary_' + index;
  dictionaryDiv.classList.add('dictionary_tool_dictionary');
  var dictionaryNameAreaDiv = this.document_.createElement('div');
  dictionaryNameAreaDiv.classList.add('dictionary_tool_dictionary_name_area');
  dictionaryNameAreaDiv.addEventListener(
      'click',
      this.onDictionaryToolDictionaryNameAreaClicked_.bind(this, index),
      true);
  var dictionaryNameDiv = this.document_.createElement('div');
  dictionaryNameDiv.classList.add('dictionary_tool_dictionary_name');
  dictionaryNameDiv.appendChild(
      this.document_.createTextNode(dictionaryName));
  dictionaryNameAreaDiv.appendChild(dictionaryNameDiv);
  var dictionaryDeleteButtonDiv = this.document_.createElement('div');
  dictionaryDeleteButtonDiv.classList.add(
      'dictionary_tool_dictionary_delete_button_div');
  var dictionaryDeleteButton = this.document_.createElement('button');
  dictionaryDeleteButton.classList.add(
      'dictionary_tool_dictionary_delete_button');
  dictionaryDeleteButtonDiv.appendChild(dictionaryDeleteButton);
  dictionaryDiv.appendChild(dictionaryNameAreaDiv);
  dictionaryDiv.appendChild(dictionaryDeleteButtonDiv);
  dictionaryDeleteButton.addEventListener(
      'click',
      this.onDictionaryToolDictionaryDeleteButtonClicked_.bind(this, index),
      true);
  if (selected) {
    dictionaryDiv.classList.add('dictionary_tool_dictionary_selected');
  }
  return dictionaryDiv;
};

/**
 * Creates a dictionary list new item element like the followings.
 * <div class="dictionary_tool_dictionary">
 *   <div class="dictionary_tool_dictionary_name_area">
 *     <input class="dictionary_tool_dictionary_new_name_input" type="text">
 *   </div>
 * </div>
 * @return {!Element} Created element.
 * @private
 */
mozc.DictionaryTool.prototype.createUserDictionaryListNewItemElement_ =
    function() {
  var dictionaryDiv = this.document_.createElement('div');
  var dictionaryNameAreaDiv = this.document_.createElement('div');
  dictionaryNameAreaDiv.classList.add('dictionary_tool_dictionary_name_area');
  var inputElement = this.document_.createElement('input');
  inputElement.type = 'text';
  inputElement.classList.add('dictionary_tool_dictionary_new_name_input');
  inputElement.addEventListener('blur',
                                this.onDictionaryNewNameInputBlur_.bind(
                                    this, inputElement),
                                true);
  inputElement.addEventListener('keydown',
                                this.onDictionaryNewNameInputKeyDown_.bind(
                                    this),
                                true);
  inputElement.addEventListener('input',
                                this.onDictionaryNameInputChanged_.bind(
                                    this, inputElement, NaN),
                                true);
  inputElement.placeholder =
      chrome.i18n.getMessage('dictionaryToolNewDictionaryNamePlaceholder');
  dictionaryNameAreaDiv.appendChild(inputElement);
  dictionaryDiv.appendChild(dictionaryNameAreaDiv);
  return dictionaryDiv;
};

/**
 * Called when dictionary_tool_dictionary_new_name_input losts focus.
 * This method creates a new dictionary.
 * @param {!Element} inputElement The input element of the new dictionary name.
 * @private
 */
mozc.DictionaryTool.prototype.onDictionaryNewNameInputBlur_ =
    function(inputElement) {
  var dictName = inputElement.value;
  if (!dictName) {
    return;
  }
  if (!isNaN(this.getDictionaryIndexByName_(dictName))) {
    return;
  }
  var command =
      this.createUserDictionaryCommandWithId_('CREATE_DICTIONARY');
  command.dictionary_name = dictName;
  this.sendUserDictionaryCommand_(
    command,
    /**
    * @this {!mozc.DictionaryTool}
    * @param {!mozc.UserDictionaryCommandStatus} response
    */
    (function(response) {
      if (response.status != 'USER_DICTIONARY_COMMAND_SUCCESS') {
        this.showDictionaryToolError_(
            mozc.DICTIONARY_TOOL_STATUS_ERROR_TITLES_.CREATE_DICTIONARY,
            response.status,
            inputElement.select.bind(inputElement));
      } else {
        this.sendSaveUserDictionaryAndReloadCommand_();
        // TODO(horo): getStorage_ is a heavy operation. We shoud only
        // update the created dictionary.
        this.getStorage_(/** @this {!mozc.DictionaryTool} */ (function() {
          this.setSelectedDictionaryIndex_(this.dictionaries_.length - 1);
          this.updateDictionaryList_();
          this.updateDictionaryContent_();
        }).bind(this));
      }
    }).bind(this));
};

/**
 * Called when key is down in dictionary_tool_dictionary_new_name_input.
 * @param {Event} event Event object passed from browser.
 * @private
 */
mozc.DictionaryTool.prototype.onDictionaryNewNameInputKeyDown_ =
    function(event) {
  if (event && event.keyCode == 13) {
    var dictionaryList =
        this.document_.getElementById('dictionary_tool_dictionary_list');
    if (dictionaryList) {
      dictionaryList.focus();
    }
  }
};

/**
 * Called when the value of the dictionary name element is changed. This
 * function validates the dictionary name and change the background color of the
 * element.
 * @param {!Element} inputElement The dictionary name input element.
 * @param {number} index The index of the dictionary. If the dictionary name
 *     element is for the new dictionary this value is NaN.
 * @private
 */
mozc.DictionaryTool.prototype.onDictionaryNameInputChanged_ =
    function(inputElement, index) {
  var dictName = inputElement.value;
  var foundIndex = this.getDictionaryIndexByName_(dictName);
  var valid = isNaN(foundIndex);
  if (!(isNaN(index) || isNaN(foundIndex))) {
    valid = (foundIndex == index);
  }
  inputElement.setCustomValidity(valid ? '' : ' ');
};

/**
 * Gets the user dictionary storage from NaCl Mozc module and stores it to
 * dictionaries_ and update the dictionary list and content.
 * @param {!function()=} opt_callback Function to be called when finished.
 * @private
 */
mozc.DictionaryTool.prototype.loadStorage_ = function(opt_callback) {
  this.getStorage_(/** @this {!mozc.DictionaryTool} */ (function() {
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
mozc.DictionaryTool.prototype.getStorage_ = function(opt_callback) {
  this.sendUserDictionaryCommand_(
    this.createUserDictionaryCommandWithId_('GET_STORAGE'),
    /**
    * @this {!mozc.DictionaryTool}
    * @param {!mozc.UserDictionaryCommandStatus} response
    */
    (function(response) {
      if (response.storage == undefined) {
        this.dictionaries_ = [];
      } else {
        this.dictionaries_ = response.storage.dictionaries;
      }
      if (isNaN(this.getSelectedDictionaryIndex_()) &&
          this.dictionaries_.length) {
        this.setSelectedDictionaryIndex_(0);
      } else {
        this.setSelectedDictionaryIndex_(this.getSelectedDictionaryIndex_());
      }
      // Sets the default values of UserDictionary.
      for (var i = 0; i < this.dictionaries_.length; ++i) {
        var dictionary = this.dictionaries_[i];
        var entries = dictionary.entries;
        for (var j = 0; j < entries.length; ++j) {
          var entry = entries[j];
          entry.key = entry.key || '';
          entry.value = entry.value || '';
          entry.pos = entry.pos || 'NOUN';
          entry.comment = entry.comment || '';
        }
      }
      if (opt_callback) {
        opt_callback();
      }
    }).bind(this));
};

/**
 * Called when the user presses Esc key while dictionary tool is opened.
 * If the active element is INPUT element this method calls blur(), otherwise
 * calls onDictionaryToolDoneButtonClicked().
 * @private
 */
mozc.DictionaryTool.prototype.onDictionaryToolEscapeKeyPressed_ = function() {
  if (this.document_.activeElement.tagName == 'INPUT') {
    this.document_.activeElement.blur();
    return;
  }
  this.onDictionaryToolDoneButtonClicked();
};

/**
 * Called when dictionary_tool_done_button button or dictionary_tool_close
 * button is clicked or the user presses Esc key while dictionary tool is
 * opened and the active element is not INPUT element.
 * This method closes the dictionary tool.
 */
mozc.DictionaryTool.prototype.onDictionaryToolDoneButtonClicked = function() {
  this.optionPage_.popEscapeKeyHandler();
  this.optionPage_.enableElementById('dictionary_tool_open_button');
  this.optionPage_.hideOverlayElementById('dictionary_tool_overlay');
  this.document_.getElementById('dictionary_tool_open_button').focus();
};

/**
 * Returns the selected dictionary index.
 * @return {number} The selected dictionary index. The return value is NaN
 *     when there is no dictionary.
 * @private
 */
mozc.DictionaryTool.prototype.getSelectedDictionaryIndex_ = function() {
  return this.selectedDictionaryIndex_;
};

/**
 * Sets the selected dictionary index.
 * @param {number} index The selected dictionary index.
 * @private
 */
mozc.DictionaryTool.prototype.setSelectedDictionaryIndex_ = function(index) {
  if (isNaN(index) || this.dictionaries_.length == 0) {
    this.selectedDictionaryIndex_ = NaN;
  } else if (index < 0) {
    this.selectedDictionaryIndex_ = 0;
  } else if (index < this.dictionaries_.length) {
    this.selectedDictionaryIndex_ = index;
  } else {
    this.selectedDictionaryIndex_ = this.dictionaries_.length - 1;
  }
  for (var i = 0; i < this.dictionaries_.length; ++i) {
    var dictionaryDiv =
        this.document_.getElementById('dictionary_tool_dictionary_' + i);
    if (!dictionaryDiv) {
      continue;
    }
    if (i == this.selectedDictionaryIndex_) {
      dictionaryDiv.classList.add('dictionary_tool_dictionary_selected');
    } else {
      dictionaryDiv.classList.remove('dictionary_tool_dictionary_selected');
    }
  }
};

/**
 * Key down handler of "dictionary_tool_dictionary_list" element.
 * @param {Event} event Event object passed from browser.
 */
mozc.DictionaryTool.prototype.onDictionaryToolDictionaryListKeyDown =
    function(event) {
  var dictIndex = this.getSelectedDictionaryIndex_();
  if (isNaN(dictIndex)) {
    return;
  }
  if (event.keyCode == 40) {  // Down
    this.setSelectedDictionaryIndex_(dictIndex + 1);
  } else if (event.keyCode == 38) {  // Up
    this.setSelectedDictionaryIndex_(dictIndex - 1);
  } else if (event.keyCode == 13) {  // Enter
    this.insertDictionaryNameInputElement_(dictIndex);
  }
  if (dictIndex != this.getSelectedDictionaryIndex_()) {
    this.updateDictionaryContent_();
  }
};

/**
 * Called when a dictionary name area is clicked.
 * @param {number} dictIndex The index of the clicked dictionary list item.
 * @private
 */
mozc.DictionaryTool.prototype.onDictionaryToolDictionaryNameAreaClicked_ =
    function(dictIndex) {
  if (this.getSelectedDictionaryIndex_() == dictIndex) {
    this.insertDictionaryNameInputElement_(dictIndex);
    return;
  }
  this.setSelectedDictionaryIndex_(dictIndex);
  this.updateDictionaryContent_();
};

/**
 * Inserts the name input element of the dictionary of the index.
 * @param {number} index The index of the dictionary entry.
 * @private
 */
mozc.DictionaryTool.prototype.insertDictionaryNameInputElement_ =
    function(index) {
  var dictionary = this.dictionaries_[index];
  if (!dictionary) {
    return;
  }
  var dictionaryDiv =
      this.document_.getElementById('dictionary_tool_dictionary_' + index);
  if (!dictionaryDiv) {
    return;
  }
  var nameAreaDiv = dictionaryDiv.getElementsByClassName(
                        'dictionary_tool_dictionary_name_area')[0];
  if (!nameAreaDiv ||
      nameAreaDiv.classList.contains(
          'dictionary_tool_dictionary_name_area_selected')) {
    return;
  }
  nameAreaDiv.classList.add('dictionary_tool_dictionary_name_area_selected');
  var inputElement = this.document_.createElement('input');
  inputElement.type = 'text';
  inputElement.classList.add('dictionary_tool_dictionary_name_input');
  inputElement.addEventListener('blur',
                                this.onDictionaryNameInputBlur_.bind(
                                    this,
                                    index,
                                    inputElement,
                                    dictionary.name || ''),
                                true);
  inputElement.addEventListener('keydown',
                                this.onDictionaryNameInputKeyDown_.bind(this),
                                true);
  inputElement.addEventListener('input',
                                this.onDictionaryNameInputChanged_.bind(
                                    this, inputElement, index),
                                true);
  inputElement.value = dictionary.name || '';
  nameAreaDiv.appendChild(inputElement);
  inputElement.focus();
};

/**
 * Called when dictionary_tool_dictionary_name_input losts focus.
 * @param {number} index The index of the dictionary entry.
 * @param {!Element} inputElement The input element of the dictionary name.
 * @param {string} originalName The original dictionary name.
 * @private
 */
mozc.DictionaryTool.prototype.onDictionaryNameInputBlur_ =
    function(index, inputElement, originalName) {
  var newName = inputElement.value;
  var dictionary = this.dictionaries_[index];
  if (!dictionary || dictionary.name != originalName) {
    return;
  }
  var dictionaryDiv =
      this.document_.getElementById('dictionary_tool_dictionary_' + index);
  if (!dictionaryDiv) {
    return;
  }
  var nameAreaDiv = dictionaryDiv.getElementsByClassName(
                        'dictionary_tool_dictionary_name_area')[0];
  if (!nameAreaDiv ||
      !nameAreaDiv.classList.contains(
           'dictionary_tool_dictionary_name_area_selected')) {
    return;
  }
  var nameDiv = nameAreaDiv.getElementsByClassName(
                    'dictionary_tool_dictionary_name')[0];
  nameAreaDiv.classList.remove('dictionary_tool_dictionary_name_area_selected');
  nameAreaDiv.removeChild(inputElement);
  if (!newName || !isNaN(this.getDictionaryIndexByName_(newName))) {
    return;
  }
  nameDiv.innerText = newName;
  var command =
      this.createUserDictionaryCommandWithId_('RENAME_DICTIONARY');
  command.dictionary_id = dictionary.id;
  command.dictionary_name = newName;
  this.sendUserDictionaryCommand_(
    command,
    /**
    * @this {!mozc.DictionaryTool}
    * @param {!mozc.UserDictionaryCommandStatus} response
    */
    (function(response) {
      if (response.status != 'USER_DICTIONARY_COMMAND_SUCCESS') {
        this.showDictionaryToolError_(
            mozc.DICTIONARY_TOOL_STATUS_ERROR_TITLES_.RENAME_DICTIONARY,
            response.status);
      } else {
        this.sendSaveUserDictionaryAndReloadCommand_();
        dictionary.name = newName;
      }
      nameDiv.innerText = dictionary.name;
    }).bind(this));
};

/**
 * Called when key is down in dictionary_tool_dictionary_name_input.
 * @param {Event} event Event object passed from browser.
 * @private
 */
mozc.DictionaryTool.prototype.onDictionaryNameInputKeyDown_ = function(event) {
  if (event && event.keyCode == 13) {
    var dictionaryList =
        this.document_.getElementById('dictionary_tool_dictionary_list');
    if (dictionaryList) {
      dictionaryList.focus();
    }
  }
};

/**
 * Called when dictionary_tool_dictionary_delete_button button is clicked.
 * This method deletes the clicked dictionary.
 * @param {number} dictIndex The clicked dictionary index.
 * @private
 */
mozc.DictionaryTool.prototype.onDictionaryToolDictionaryDeleteButtonClicked_ =
    function(dictIndex) {
  this.optionPage_.showConfirm(
      'dictionary_tool_page',
      chrome.i18n.getMessage('dictionaryToolDeleteDictionaryTitle'),
      chrome.i18n.getMessage('dictionaryToolDeleteDictionaryConfirm',
                             this.dictionaries_[dictIndex].name || ''),
      /**
      * @this {!mozc.DictionaryTool}
      * @param {?boolean} result
      */
      (function(result) {
        if (!result) {
          return;
        }
        var dictionaryId = this.dictionaries_[dictIndex].id;
        var command =
            this.createUserDictionaryCommandWithId_('DELETE_DICTIONARY');
        command.dictionary_id = dictionaryId;
        this.sendUserDictionaryCommand_(
          command,
          /**
          * @this {!mozc.DictionaryTool}
          * @param {!mozc.UserDictionaryCommandStatus} response
          */
          (function(response) {
            if (response.status != 'USER_DICTIONARY_COMMAND_SUCCESS') {
              this.showDictionaryToolError_(
                  mozc.DICTIONARY_TOOL_STATUS_ERROR_TITLES_.DELETE_DICTIONARY,
                  response.status);
            } else {
              this.sendSaveUserDictionaryAndReloadCommand_();
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
 */
mozc.DictionaryTool.prototype.onDictionaryToolExportButtonClicked = function() {
  var dictIndex = this.getSelectedDictionaryIndex_();
  if (isNaN(dictIndex)) {
    return;
  }
  var data = '';
  var entries = this.dictionaries_[dictIndex].entries;
  for (var i = 0; i < entries.length; ++i) {
    var key = entries[i].key || '';
    var value = entries[i].value || '';
    var pos = entries[i].pos || 'NOUN';
    var comment = entries[i].comment || '';
    data += key + '\t' +
            value + '\t' +
            this.posNameMap_[pos] + '\t' +
            comment + '\n';
  }
  var blob = new Blob([data]);
  var a = this.document_.createElement('a');
  a.href = this.window_.URL.createObjectURL(blob);
  a.download = this.dictionaries_[dictIndex].name + '.txt';
  a.style.display = 'none';
  this.document_.body.appendChild(a);
  a.click();
  this.document_.body.removeChild(a);
};

/**
 * Called when import_dictionary_button button is clicked.
 * This method shows "Open File" dialog using a file type input element.
 */
mozc.DictionaryTool.prototype.onDictionaryToolImportButtonClicked = function() {
  var fileInput = this.document_.getElementById('dictionary_import_file');
  if (fileInput) {
    // To reset the files attribute of file type input element, we recreate the
    // input element. It is needed because 'change' event will not occur if the
    // user selects the same file twice in succession in the same input element.
    this.document_.body.removeChild(fileInput);
  }
  fileInput = this.document_.createElement('input');
  fileInput.type = 'file';
  fileInput.id = 'dictionary_import_file';
  fileInput.style.display = 'none';
  fileInput.addEventListener('change',
                             this.onDictionaryImportFileChanged_.bind(this),
                             true);
  this.document_.body.appendChild(fileInput);
  fileInput.click();
};

/**
 * Returns the entry dictionary index if the element is a descendant element of
 * a dictionary entry element. Otherwise returns NaN.
 * @param {HTMLElement} element The descendant element of a dictionary entry
 *     element.
 * @return {number} The entry index or NaN.
 * @private
 */
mozc.DictionaryTool.prototype.findDictionaryEntryIndexFromElement_ =
    function(element) {
  if (!element) {
    return NaN;
  }
  for (var elem = element; elem; elem = elem.parentElement) {
    if (elem.classList.contains('dictionary_tool_entry')) {
      return elem.dictionaryEntryIndex;
    }
  }
  return NaN;
};

/**
 * Returns the dictionary entry element of the index.
 * @param {number} index The index of the dictionary entry.
 * @return {HTMLElement} The dictionary entry element of the index or null
 * @private
 */
mozc.DictionaryTool.prototype.getDictionaryEntryElement_ = function(index) {
  return this.document_.getElementById('dictionary_tool_entry_' + index);
};

/**
 * Returns the dictionary entry sub elements of the index.
 * @param {number} index The index of the dictionary entry.
 * @return {Object.<mozc.USER_DICTIONARY_ENTRY_ITEM_TYPE_, !HTMLElement>}
 *     The sub elements of the specified index entry.
 * @private
 */
mozc.DictionaryTool.prototype.getDictionaryEntrySubElements_ = function(index) {
  var entryElem = this.getDictionaryEntryElement_(index);
  if (!entryElem) {
    return null;
  }
  var result = {};
  for (var i = 0; i < mozc.USER_DICTIONARY_ENTRY_ITEM_INFO_.length; ++i) {
    var itemInfo = mozc.USER_DICTIONARY_ENTRY_ITEM_INFO_[i];
    result[itemInfo.type] =
        entryElem.getElementsByClassName(itemInfo.divElementClass)[0];
  }
  return result;
};

/**
 * Updates the dictionary entry text of the index.
 * @param {number} index The index of the dictionary entry.
 * @param {!mozc.UserDictionaryEntry} entry The entry data.
 * @private
 */
mozc.DictionaryTool.prototype.updateDictionaryEntryText_ = function(index,
                                                                    entry) {
  var entrySubElements = this.getDictionaryEntrySubElements_(index);
  if (!entrySubElements) {
    return;
  }
  for (var i = 0; i < mozc.USER_DICTIONARY_ENTRY_ITEM_INFO_.length; ++i) {
    var itemInfo = mozc.USER_DICTIONARY_ENTRY_ITEM_INFO_[i];
    var innerText = '';
    if (itemInfo.type == mozc.USER_DICTIONARY_ENTRY_ITEM_TYPE_.CATEGORY) {
      innerText = this.posNameMap_[entry[itemInfo.type]];
    } else {
      innerText = entry[itemInfo.type];
    }
    entrySubElements[itemInfo.type]
        .getElementsByClassName('static_text')[0].innerText = innerText;
  }
};

/**
 * Sets the tab index of the dictionary entry element of the index zero to make
 * the element focusable.
 * @param {number} index The index of the dictionary entry.
 * @private
 */
mozc.DictionaryTool.prototype.setDictionaryEntryZeroTabIndex_ =
    function(index) {
  var entrySubElements = this.getDictionaryEntrySubElements_(index);
  if (!entrySubElements) {
    console.error('Can\'t get the dictionary entry sub elements at ' + index);
    return;
  }
  for (var key in mozc.USER_DICTIONARY_ENTRY_ITEM_TYPE_) {
    entrySubElements[mozc.USER_DICTIONARY_ENTRY_ITEM_TYPE_[key]].tabIndex = 0;
  }
};

/**
 * Removes the tab index of the dictionary entry element of the index to make
 * the element unfocusable.
 * @param {number} index The index of the dictionary entry.
 * @private
 */
mozc.DictionaryTool.prototype.removeDictionaryEntryTabIndex_ = function(index) {
  var entrySubElements = this.getDictionaryEntrySubElements_(index);
  if (!entrySubElements) {
    console.error('Can\'t get the dictionary entry sub elements at ' + index);
    return;
  }
  for (var key in mozc.USER_DICTIONARY_ENTRY_ITEM_TYPE_) {
    entrySubElements[mozc.USER_DICTIONARY_ENTRY_ITEM_TYPE_[key]]
      .removeAttribute('tabIndex');
  }
};

/**
 * Inserts the input elements to the dictionary entry element of the index.
 * @param {number} index The index of the dictionary entry.
 * @param {!mozc.UserDictionaryEntry} entry The entry data.
 * @private
 */
mozc.DictionaryTool.prototype.insertDictionaryEntryInputElements_ =
    function(index, entry) {
  var entrySubElements = this.getDictionaryEntrySubElements_(index);
  if (!entrySubElements) {
    console.error('Can\'t get the dictionary entry sub elements at ' + index);
    return;
  }

  for (var i = 0; i < mozc.USER_DICTIONARY_ENTRY_ITEM_INFO_.length; ++i) {
    var itemInfo = mozc.USER_DICTIONARY_ENTRY_ITEM_INFO_[i];
    var inputElement = null;
    if (itemInfo.inputElementType == 'text') {
      inputElement = this.document_.createElement('input');
      inputElement.type = 'text';
    } else if (itemInfo.inputElementType == 'select') {
      inputElement = this.document_.createElement('select');
    } else {
      console.error('Unsupported type: ' + itemInfo.inputElementType);
      continue;
    }
    inputElement.classList.add(itemInfo.inputElementClass);
    inputElement.addEventListener('blur',
                                  this.onDictionaryEntryBlur_.bind(this),
                                  true);
    if (itemInfo.type == mozc.USER_DICTIONARY_ENTRY_ITEM_TYPE_.CATEGORY) {
      for (var j = 0; j < this.posList_.length; ++j) {
        inputElement.appendChild(
            this.optionPage_.createOptionElement(this.posList_[j].name,
                                                 this.posList_[j].type));
      }
    }
    if (itemInfo.type == mozc.USER_DICTIONARY_ENTRY_ITEM_TYPE_.READING) {
      inputElement.addEventListener('input',
                                    this.onDictionaryReadingInputChanged.bind(
                                        this,
                                        inputElement),
                                    true);
    }
    inputElement.value = entry[itemInfo.type];
    entrySubElements[itemInfo.type].appendChild(inputElement);
  }
};

/**
 * Gets the dictionary entry data from  the input elements in the dictionary
 * entry element of the index.
 * @param {number} index The index of the dictionary entry.
 * @return {mozc.UserDictionaryEntry} The entry data or null.
 * @private
 */
mozc.DictionaryTool.prototype.getDictionaryEntryInputValues_ = function(index) {
  var entryElem = this.getDictionaryEntryElement_(index);
  if (!entryElem) {
    return null;
  }
  var result = /** @type {!mozc.UserDictionaryEntry} */ ({});
  for (var i = 0; i < mozc.USER_DICTIONARY_ENTRY_ITEM_INFO_.length; ++i) {
    var itemInfo = mozc.USER_DICTIONARY_ENTRY_ITEM_INFO_[i];
    var inputElem =
        entryElem.getElementsByClassName(itemInfo.inputElementClass)[0];
    if (!inputElem) {
      return null;
    }
    result[itemInfo.type] = inputElem.value;
  }
  return result;
};

/**
 * Removes the input elements in the dictionary entry element of the index.
 * @param {number} index The index of the dictionary entry.
 * @private
 */
mozc.DictionaryTool.prototype.removeDictionaryEntryInputElements_ =
    function(index) {
  var entryElem = this.getDictionaryEntryElement_(index);
  if (!entryElem) {
    return;
  }
  for (var i = 0; i < mozc.USER_DICTIONARY_ENTRY_ITEM_INFO_.length; ++i) {
    var itemInfo = mozc.USER_DICTIONARY_ENTRY_ITEM_INFO_[i];
    var inputElem =
        entryElem.getElementsByClassName(itemInfo.inputElementClass)[0];
    if (inputElem) {
      inputElem.parentElement.removeChild(inputElem);
    }
  }
};

/**
 * Called when dictionary entry element is focused.
 * This method shows the input elements for 'reading' and 'word' and 'comment',
 * and the select element for 'category'.
 * @param {mozc.USER_DICTIONARY_ENTRY_ITEM_TYPE_} type Entry type.
 * @param {Event} event Event object passed from browser.
 * @private
 */
mozc.DictionaryTool.prototype.onDictionaryEntryFocus_ = function(type,
                                                                 event) {
  if (!event) {
    console.error('Event object is null.');
    return;
  }
  var index = this.findDictionaryEntryIndexFromElement_(
      /** @type {HTMLElement} */ (event.target));
  if (isNaN(index)) {
    console.error('Can\'t detect entry index.');
    return;
  }
  var dictIndex = this.getSelectedDictionaryIndex_();
  if (isNaN(dictIndex)) {
    return;
  }
  var entryElem = this.getDictionaryEntryElement_(index);
  if (!entryElem) {
    console.error('Can\'t get dictionary entry element.');
    return;
  }
  if (entryElem.classList.contains('dictionary_tool_entry_selected')) {
    return;
  }
  var entry = this.dictionaries_[dictIndex].entries[index];
  entryElem.classList.add('dictionary_tool_entry_selected');
  if (!entry) {
    console.error('Can\'t get the entry at ' + index);
  } else {
    this.insertDictionaryEntryInputElements_(index, entry);
  }
  for (var i = 0; i < mozc.USER_DICTIONARY_ENTRY_ITEM_INFO_.length; ++i) {
    var itemInfo = mozc.USER_DICTIONARY_ENTRY_ITEM_INFO_[i];
    if (itemInfo.type == type) {
      entryElem.getElementsByClassName(itemInfo.inputElementClass)[0].focus();
    }
  }
  this.removeDictionaryEntryTabIndex_(index);
};

/**
 * Called when dictionary entry element lost focus.
 * This method hides the input elements for 'reading' and 'word' and 'comment',
 * and the select element for 'category'. And also this method saves the entry.
 * @param {Event} event Event object passed from browser.
 * @private
 */
mozc.DictionaryTool.prototype.onDictionaryEntryBlur_ = function(event) {
  if (!event) {
    console.error('Event object is null.');
    return;
  }
  var index = this.findDictionaryEntryIndexFromElement_(
      /** @type {HTMLElement} */ (event.target));
  if (isNaN(index)) {
    console.error('Can\'t detect entry index');
    return;
  }
  var dictIndex = this.getSelectedDictionaryIndex_();
  if (isNaN(dictIndex)) {
    return;
  }
  if (event && event.relatedTarget &&
      this.findDictionaryEntryIndexFromElement_(
          /** @type {HTMLElement} */ (event.relatedTarget)) == index) {
    for (var i = 0; i < mozc.USER_DICTIONARY_ENTRY_ITEM_INFO_.length; ++i) {
      var itemInfo = mozc.USER_DICTIONARY_ENTRY_ITEM_INFO_[i];
      if (event.relatedTarget.classList.contains(itemInfo.inputElementClass)) {
        return;
      }
    }
  }
  var entryElem = this.getDictionaryEntryElement_(index);
  if (!entryElem) {
    console.error('Can\'t get dictionary entry element.');
    return;
  }
  if (!entryElem.classList.contains('dictionary_tool_entry_selected')) {
    return;
  }
  this.setDictionaryEntryZeroTabIndex_(index);
  var oldEntry = this.dictionaries_[dictIndex].entries[index];
  var newEntry = this.getDictionaryEntryInputValues_(index);
  entryElem.classList.remove('dictionary_tool_entry_selected');
  this.removeDictionaryEntryInputElements_(index);
  if (!newEntry ||
      (newEntry.key == oldEntry.key &&
       newEntry.value == oldEntry.value &&
       newEntry.comment == oldEntry.comment &&
       newEntry.pos == oldEntry.pos)) {
    return;
  }
  this.updateDictionaryEntryText_(index, newEntry);
  var command = this.createUserDictionaryCommandWithId_('EDIT_ENTRY');
  command.dictionary_id = this.dictionaries_[dictIndex].id;
  command.entry_index = [index];
  command.entry = newEntry;
  this.sendUserDictionaryCommand_(
    command,
    /**
    * @this {!mozc.DictionaryTool}
    * @param {!mozc.UserDictionaryCommandStatus} response
    */
    (function(response) {
      if ((response.status != 'USER_DICTIONARY_COMMAND_SUCCESS') &&
          (response.status != 'READING_CONTAINS_INVALID_CHARACTER') &&
          (response.status != 'READING_EMPTY') &&
          (response.status != 'WORD_EMPTY')) {
        // We ignore READING_CONTAINS_INVALID_CHARACTER, READING_EMPTY and
        // WORD_EMPTY error to improve user experience.
        this.showDictionaryToolError_(
            mozc.DICTIONARY_TOOL_STATUS_ERROR_TITLES_.EDIT_ENTRY,
            response.status);
      }
      this.sendSaveUserDictionaryAndReloadCommand_();
      var command = this.createUserDictionaryCommandWithId_('GET_ENTRIES');
      command.dictionary_id = this.dictionaries_[dictIndex].id;
      command.entry_index = [index];
      this.sendUserDictionaryCommand_(
        command,
        /**
        * @this {!mozc.DictionaryTool}
        * @param {!mozc.UserDictionaryCommandStatus} response
        */
        (function(response) {
          if (response.entries == undefined || response.entries.length != 1) {
            console.error('GET_ENTRIES error');
            return;
          }
          var entry = response.entries[0];
          this.dictionaries_[dictIndex].entries[index] = entry;
          entry.key = entry.key || '';
          entry.value = entry.value || '';
          entry.pos = entry.pos || 'NOUN';
          entry.comment = entry.comment || '';
          this.updateDictionaryEntryText_(index, entry);
        }).bind(this));
    }).bind(this));
};

/**
 * Called when dictionary_tool_entry_delete_button is clicked.
 * This method deletes the entry in the dictionary.
 * @param {Event} event Event object passed from browser.
 * @private
 */
mozc.DictionaryTool.prototype.onDictionaryEntryDeleteClick_ = function(event) {
  if (!event) {
    console.error('Event object is null.');
    return;
  }
  var index = this.findDictionaryEntryIndexFromElement_(
      /** @type {HTMLElement} */ (event.target));
  if (isNaN(index)) {
    console.error('Can\'t detect entry index');
    return;
  }
  var dictIndex = this.getSelectedDictionaryIndex_();
  if (isNaN(dictIndex)) {
    return;
  }
  var command = this.createUserDictionaryCommandWithId_('DELETE_ENTRY');
  command.dictionary_id = this.dictionaries_[dictIndex].id;
  command.entry_index = [index];
  this.sendUserDictionaryCommand_(
    command,
    /**
    * @this {!mozc.DictionaryTool}
    * @param {!mozc.UserDictionaryCommandStatus} response
    */
    (function(response) {
      if (response.status != 'USER_DICTIONARY_COMMAND_SUCCESS') {
        this.showDictionaryToolError_(
            mozc.DICTIONARY_TOOL_STATUS_ERROR_TITLES_.DELETE_ENTRY,
            response.status);
        this.loadStorage_();
      } else {
        this.sendSaveUserDictionaryAndReloadCommand_();
      }
    }).bind(this));
  // Removes the entry in the dictionary.
  this.dictionaries_[dictIndex].entries.splice(index, 1);
  // Removes the entry element of the specified index.
  var entryElem = this.getDictionaryEntryElement_(index);
  if (!entryElem) {
    console.error('Can\'t get dictionary entry element.');
    return;
  }
  entryElem.parentElement.removeChild(entryElem);
  // Changes the ID and dictionaryEntryIndex property of the following entry
  // elements.
  for (var i = index + 1;; ++i) {
    entryElem = this.getDictionaryEntryElement_(i);
    if (!entryElem) {
      break;
    }
    entryElem.dictionaryEntryIndex = i - 1;
    entryElem.id = 'dictionary_tool_entry_' + (i - 1);
  }
  if (this.dictionaries_[dictIndex].entries.length == 0) {
    this.optionPage_.disableElementById('export_dictionary_button');
  }
};

/**
 * Shows dictionary tool error. The dictionary tool dialog must be the active
 * dialog when this method is called.
 * @param {string} title The title of the dialog box.
 * @param {string=} opt_status Status string of dictionary tool.
 * @param {!function()=} opt_callback Function to be called when closed.
 * @private
 */
mozc.DictionaryTool.prototype.showDictionaryToolError_ =
    function(title, opt_status, opt_callback) {
  var message = '';
  if (!opt_status) {
    message = chrome.i18n.getMessage('dictionaryToolStatusErrorGeneral');
  } else {
    message = mozc.DICTIONARY_TOOL_STATUS_ERRORS_[opt_status];
    if (!message) {
      message = chrome.i18n.getMessage('dictionaryToolStatusErrorGeneral') +
                '[' + opt_status + ']';
    }
  }
  this.optionPage_.showAlert('dictionary_tool_page',
                             title,
                             message,
                             opt_callback);
};

/**
 * Updates the dictionary entry list in 'dictionary_tool_current_area' element
 * according to selectedDictionaryIndex_ and dictionaries_.
 * @private
 */
mozc.DictionaryTool.prototype.updateDictionaryContent_ = function() {
  var dictIndex = this.getSelectedDictionaryIndex_();
  var currentArea =
      this.document_.getElementById('dictionary_tool_current_area');
  if (isNaN(dictIndex)) {
    for (var i = 0;; ++i) {
      var entryElem = this.getDictionaryEntryElement_(i);
      if (!entryElem) {
        break;
      }
      currentArea.removeChild(entryElem);
    }
    this.optionPage_.disableElementById('export_dictionary_button');
    this.optionPage_.disableElementById('import_dictionary_button');
    this.optionPage_.disableElementById('dictionary_tool_reading_new_input');
    this.optionPage_.disableElementById('dictionary_tool_word_new_input');
    this.optionPage_.disableElementById('dictionary_tool_category_new_select');
    this.optionPage_.disableElementById('dictionary_tool_comment_new_input');
    return;
  }
  var dictionary = this.dictionaries_[dictIndex];
  if (dictionary.entries.length) {
    this.optionPage_.enableElementById('export_dictionary_button');
  } else {
    this.optionPage_.disableElementById('export_dictionary_button');
  }
  this.optionPage_.enableElementById('import_dictionary_button');
  this.optionPage_.enableElementById('dictionary_tool_reading_new_input');
  this.optionPage_.enableElementById('dictionary_tool_word_new_input');
  this.optionPage_.enableElementById('dictionary_tool_category_new_select');
  this.optionPage_.enableElementById('dictionary_tool_comment_new_input');
  for (var i = 0; i < dictionary.entries.length; ++i) {
    var entry = dictionary.entries[i];
    var entryElem = this.getDictionaryEntryElement_(i);
    if (!entryElem) {
      entryElem = this.createUserDictionaryEntryElement_(i);
    }
    currentArea.appendChild(entryElem);
    this.updateDictionaryEntryText_(i, entry);
  }
  for (var i = dictionary.entries.length;; ++i) {
    var entryElem = this.getDictionaryEntryElement_(i);
    if (!entryElem) {
      break;
    }
    currentArea.removeChild(entryElem);
  }
};

/**
 * Creates a dictionary entry element like the followings. And sets
 * dictionaryEntryIndex property of dictionary_tool_entry_INDEX div to index.
 * <div id="dictionary_tool_entry_INDEX" class="dictionary_tool_entry">
 *   <div class="dictionary_tool_reading">
 *     <div class="static_text"></div>
 *   </div>
 *   <div class="dictionary_tool_word">
 *     <div class="static_text"></div>
 *   </div>
 *   <div class="dictionary_tool_category">
 *     <div class="static_text"></div>
 *   </div>
 *   <div class="dictionary_tool_comment">
 *     <div class="static_text"></div>
 *   </div>
 *   <div class="dictionary_tool_entry_delete_button_div">
 *     <button class="dictionary_tool_entry_delete_button"></button>
 *   </div>
 * </div>
 * @param {number} index Index of the dictionary entry.
 * @return {!Element} Created element.
 * @private
 */
mozc.DictionaryTool.prototype.createUserDictionaryEntryElement_ =
    function(index) {
  var entryDiv = this.document_.createElement('div');
  entryDiv.id = 'dictionary_tool_entry_' + index;
  entryDiv.classList.add('dictionary_tool_entry');
  // Sets dictionaryEntryIndex property to index.
  entryDiv.dictionaryEntryIndex = index;

  for (var i = 0; i < mozc.USER_DICTIONARY_ENTRY_ITEM_INFO_.length; ++i) {
    var itemInfo = mozc.USER_DICTIONARY_ENTRY_ITEM_INFO_[i];
    var itemDiv = this.document_.createElement('div');
    var itemStaticDiv = this.document_.createElement('div');
    itemDiv.classList.add(itemInfo.divElementClass);
    itemDiv.tabIndex = 0;
    itemDiv.addEventListener(
        'focus',
        this.onDictionaryEntryFocus_.bind(this, itemInfo.type),
        true);
    itemStaticDiv.classList.add('static_text');
    itemDiv.appendChild(itemStaticDiv);
    entryDiv.appendChild(itemDiv);
  }

  var deleteDiv = this.document_.createElement('div');
  var deleteButton = this.document_.createElement('button');
  deleteDiv.classList.add('dictionary_tool_entry_delete_button_div');
  deleteButton.classList.add('dictionary_tool_entry_delete_button');
  deleteButton.addEventListener(
      'click',
      this.onDictionaryEntryDeleteClick_.bind(this),
      true);
  deleteDiv.appendChild(deleteButton);
  entryDiv.appendChild(deleteDiv);
  return entryDiv;
};

/**
 * Called when the value of the reading input element is changed. This function
 * validates the reading and change the background color of the element.
 * @param {!Element} inputElement The reading input element.
 * @param {Event} event Event object passed from browser.
 */
mozc.DictionaryTool.prototype.onDictionaryReadingInputChanged =
    function(inputElement, event) {
  this.naclMozc_.isValidReading(inputElement.value,
      /**
      * @this {!mozc.DictionaryTool}
      * @param {!mozc.Event} response
      */
      (function(response) {
        if (inputElement.value != response.data) {
          return;
        }
        inputElement.setCustomValidity(response.result ? '' : ' ');
      }).bind(this));
};

/**
 * Called when dictionary tool new entry input element lost focus.
 * If the new reading and the new word are not empty this method creates a new
 * entry in the selected dictionary.
 * @param {Event} event Event object passed from browser.
 */
mozc.DictionaryTool.prototype.onDictionaryNewEntryLostFocus = function(event) {
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
  var dictionaryId = this.dictionaries_[dictIndex].id;
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
  this.naclMozc_.isValidReading(newReading,
      /**
      * @this {!mozc.DictionaryTool}
      * @param {!mozc.Event} response
      */
      (function(response) {
        if (!response.result) {
          return;
        }
        // We don't reset the value of dictionary_tool_category_new_select to
        // help the user who want to add the same category entries.
        newReadingInput.value = '';
        newWordInput.value = '';
        newCommentInput.value = '';
        var command = this.createUserDictionaryCommandWithId_('ADD_ENTRY');
        command.dictionary_id = dictionaryId;
        command.entry = /** @type {!mozc.UserDictionaryEntry} */ ({
          'key': newReading,
          'value': newWord,
          'comment': newComment,
          'pos': newCategory
        });
        this.sendUserDictionaryCommand_(
            command,
            /**
            * @this {!mozc.DictionaryTool}
            * @param {!mozc.UserDictionaryCommandStatus} response
            */
            (function(response) {
              if (response.status != 'USER_DICTIONARY_COMMAND_SUCCESS') {
                this.showDictionaryToolError_(
                    mozc.DICTIONARY_TOOL_STATUS_ERROR_TITLES_.ADD_ENTRY,
                    response.status);
              } else {
                this.sendSaveUserDictionaryAndReloadCommand_();
                // TODO(horo): loadStorage_ is a heavy operation. We shoud only
                // update the added entry.
                this.loadStorage_(newReadingInput.focus.bind(newReadingInput));
              }
            }).bind(this));
      }).bind(this));

};

/**
 * Creates a mozc.UserDictionaryCommand object with the specified type and
 * session_id.
 * @param {string} type Input type of mozc.Command object.
 * @return {!mozc.UserDictionaryCommand} Created mozc.UserDictionaryCommand.
 * @private
 */
mozc.DictionaryTool.prototype.createUserDictionaryCommandWithId_ =
    function(type) {
  return /** @type {!mozc.UserDictionaryCommand} */ ({
           'type': type,
           'session_id': this.userDictionarySessionId_
        });
};

/**
 * Creates a mozc.UserDictionaryCommand object with the specified type.
 * @param {string} type Input type of mozc.Command object.
 * @return {!mozc.UserDictionaryCommand} Created mozc.UserDictionaryCommand.
 * @private
 */
mozc.DictionaryTool.prototype.createUserDictionaryCommand_ =
    function(type) {
  return /** @type {!mozc.UserDictionaryCommand} */ ({'type': type});
};

/**
 * Sends user dictionary command to NaCl module.
 * @param {!mozc.UserDictionaryCommand} command User dictionary command object
 *     to be sent.
 * @param {function(!mozc.UserDictionaryCommandStatus)=} opt_callback Function
 *     to be called with results from NaCl module.
 * @private
 */
mozc.DictionaryTool.prototype.sendUserDictionaryCommand_ =
  function(command, opt_callback) {
  this.naclMozc_.sendUserDictionaryCommand(command, opt_callback);
};

/**
 * Sends save user dictionary command and reload command to NaCl module.
 * @private
 */
mozc.DictionaryTool.prototype.sendSaveUserDictionaryAndReloadCommand_ =
    function() {
  this.sendUserDictionaryCommand_(
      this.createUserDictionaryCommandWithId_('SAVE'),
      this.naclMozc_.sendReload.bind(this.naclMozc_, undefined));
};

/**
 * Returns true if charCode is non-character code.
 * @param {number} charCode The code of a character.
 * @return {boolean} Whether the charCode is non-character code.
 * @see http://en.wikipedia.org/wiki/Mapping_of_Unicode_characters#Noncharacters
 * @private
 */
mozc.DictionaryTool.prototype.isUnicodeNonCharacter_ = function(charCode) {
  return charCode > 0xfdd0 &&
         (charCode <= 0xfdef || (charCode & 0xfffe) == 0xfffe) &&
         charCode <= 0x10ffff;
};

/**
 * Returns true if text has non-character.
 * @param {string} text The string to check.
 * @return {boolean} Whether the text has non-character.
 * @see http://en.wikipedia.org/wiki/Mapping_of_Unicode_characters#Noncharacters
 * @private
 */
mozc.DictionaryTool.prototype.hasUnicodeNonCharacter_ = function(text) {
  for (var i = 0; i < text.length; ++i) {
    if (this.isUnicodeNonCharacter_(text.charCodeAt(i))) {
      return true;
    }
  }
  return false;
};

/**
 * Called when the user selects a file to import to user dictionary.
 * This method loads the user specified file using FileReader and sends
 * IMPORT_DATA user dictionary command with the read data to NaCl module and
 * closes the dictionary import dialog.
 * @private
 */
mozc.DictionaryTool.prototype.onDictionaryImportFileChanged_ = function() {
  var selectedDictionaryIndex = this.getSelectedDictionaryIndex_();
  if (isNaN(selectedDictionaryIndex)) {
    return;
  }
  var fileInput = this.document_.getElementById('dictionary_import_file');
  var file = fileInput.files[0];
  var importCommand = this.createUserDictionaryCommandWithId_('IMPORT_DATA');
  importCommand.dictionary_id = this.dictionaries_[selectedDictionaryIndex].id;
  var reader = new FileReader();
  reader.addEventListener('load',
      /**
      * @this {!mozc.DictionaryTool}
      * @param {Event} event
      */
      (function(event) {
        if (!event || !event.target) {
          this.showDictionaryToolError_(
              mozc.DICTIONARY_TOOL_STATUS_ERROR_TITLES_.IMPORT_DATA,
              'FILE_NOT_FOUND');
          return;
        } else if (this.hasUnicodeNonCharacter_(event.target.result)) {
          // NaCl module can't handle non-character. So we reject the file which
          // contains non-character here.
          this.showDictionaryToolError_(
              mozc.DICTIONARY_TOOL_STATUS_ERROR_TITLES_.IMPORT_DATA,
              'INVALID_FILE_FORMAT');
          return;
        } else if (event.target.result == '') {
          this.showDictionaryToolError_(
              mozc.DICTIONARY_TOOL_STATUS_ERROR_TITLES_.IMPORT_DATA,
              'EMPTY_FILE');
          return;
        }
        importCommand.data = event.target.result;
        this.sendUserDictionaryCommand_(
            importCommand,
            /**
            * @this {!mozc.DictionaryTool}
            * @param {!mozc.UserDictionaryCommandStatus} response
            */
            (function(response) {
              var importStatus = response.status;
              this.sendSaveUserDictionaryAndReloadCommand_();
              this.getStorage_(/** @this {!mozc.DictionaryTool} */(function() {
                if (selectedDictionaryIndex < 0) {
                  // Selects the last dictionary which is newly created.
                  this.setSelectedDictionaryIndex_(
                      this.dictionaries_.length - 1);
                } else {
                  // Selects the user specified dictionary.
                  this.setSelectedDictionaryIndex_(selectedDictionaryIndex);
                }
                this.updateDictionaryList_();
                this.updateDictionaryContent_();
                if (importStatus != 'USER_DICTIONARY_COMMAND_SUCCESS') {
                  this.showDictionaryToolError_(
                      mozc.DICTIONARY_TOOL_STATUS_ERROR_TITLES_.IMPORT_DATA,
                      importStatus);
                }
              }).bind(this));
            }).bind(this));
      }).bind(this), true);
  reader.readAsText(file);
};
