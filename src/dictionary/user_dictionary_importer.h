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

#ifndef MOZC_DICTIONARY_USER_DICTIONARY_IMPORTER_H_
#define MOZC_DICTIONARY_USER_DICTIONARY_IMPORTER_H_

#include <string>
#include "base/port.h"
#include "dictionary/user_dictionary_storage.pb.h"

namespace mozc {

// An utilitiy class for importing user dictionary
// from different devices, including text files and MS-IME,
// Kotoeri, and ATOK(optional) user dictionaries.
class UserDictionaryImporter {
 public:
  // A raw entry to be read.
  struct RawEntry {
    string key;
    string value;
    string pos;
    string comment;

    void Clear() {
      key.clear();
      value.clear();
      pos.clear();
      comment.clear();
    }
  };

  // An abstract class for representing an input device
  // for user dictionary. It could be possible to import
  // dictionary from docs/spreadsheet.
  class InputIteratorInterface {
   public:
    InputIteratorInterface() {}
    virtual ~InputIteratorInterface() {}

    // return true the input iterator is available
    virtual bool IsAvailable() const = 0;

    // return true if entry is read successfully.
    // Next method doesn't nee to convert the POS of entry.
    virtual bool Next(RawEntry *raw_entry) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(InputIteratorInterface);
  };

  // An abstract class for reading a text file per line.
  // As we'd like to use QTextFileStream to load UTF16 files,
  // make an interface class for reading text per line.
  class TextLineIteratorInterface {
   public:
    TextLineIteratorInterface() {}
    virtual ~TextLineIteratorInterface() {}

    // return true text line iterator is available
    virtual bool IsAvailable() const = 0;

    // Read a line in UTF-8.
    // The TextLineIteratorInterface class takes a responsibility
    // of character set conversion. "line" must always be stored in UTF-8.
    virtual bool Next(string *line) = 0;

    // Reset the current position
    virtual void Reset() = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(TextLineIteratorInterface);
  };

  // A Wrapper for istream. Istream must be written in UTF-8 or Shift-JIS
  class IStreamTextLineIterator : public TextLineIteratorInterface {
   public:
    explicit IStreamTextLineIterator(istream *is);
    virtual ~IStreamTextLineIterator();

    virtual bool IsAvailable() const;
    virtual bool Next(string *line);
    virtual void Reset();

   private:
    istream *is_;
    DISALLOW_COPY_AND_ASSIGN(IStreamTextLineIterator);
  };

  // A wrapper for string. The string should contain utf-8 characters.
  // This class should resolve CR/LF issue.
  // This class does NOT take the ownership of the given string.
  // So it is caller's responsibility to extend the lifetime of the given
  // string until this iterator is destroyed.
  class StringTextLineIterator : public TextLineIteratorInterface {
   public:
    explicit StringTextLineIterator(const string &data);
    virtual ~StringTextLineIterator();

    virtual bool IsAvailable() const;
    virtual bool Next(string *line);
    virtual void Reset();

   private:
    const string &data_;
    int position_;
    DISALLOW_COPY_AND_ASSIGN(StringTextLineIterator);
  };

  // List of IMEs.
  enum IMEType {
    IME_AUTO_DETECT = 0,
    MOZC            = 1,
    MSIME           = 2,
    ATOK            = 3,
    KOTOERI         = 4,
    NUM_IMES        = 5,
  };

  // GuessIMEType from the first line of IME file
  // return "NUM_IMES" if the format is unknown
  static IMEType GuessIMEType(const string &line);

  // return the final IME type from user_ime_type and guessed_ime_type
  static IMEType DetermineFinalIMEType(IMEType user_ime_type,
                                       IMEType guessed_ime_type);

  // List of character encodings.
  enum EncodingType {
    ENCODING_AUTO_DETECT = 0,
    UTF8                 = 1,
    UTF16                = 2,
    SHIFT_JIS            = 3,
    NUM_ENCODINGS        = 4
  };

  // Guess Encoding Type of string
  static EncodingType GuessEncodingType(const char *str, size_t size);

  // Guess Encoding Type of file
  static EncodingType GuessFileEncodingType(const string &filename);

  // A special input iterator for reading entries from
  // TextLineIteratorInterface.
  class TextInputIterator : public InputIteratorInterface {
   public:
    TextInputIterator(IMEType ime_type,
                      TextLineIteratorInterface *iter);
    virtual ~TextInputIterator();

    virtual bool IsAvailable() const;
    virtual bool Next(RawEntry *entry);
    IMEType ime_type() const { return ime_type_; }

   private:
    IMEType ime_type_;
    TextLineIteratorInterface *iter_;
    string first_line_;

    DISALLOW_COPY_AND_ASSIGN(TextInputIterator);
  };

  enum ErrorType {
    IMPORT_NO_ERROR,
    IMPORT_NOT_SUPPORTED,
    IMPORT_TOO_MANY_WORDS,
    IMPORT_INVALID_ENTRIES,
    IMPORT_FATAL,
    IMPORT_UNKNOWN_ERROR
  };

  // Convert POS's of other IME's into Mozc's IME.
  static bool ConvertEntry(
      const RawEntry &from, user_dictionary::UserDictionary::Entry *to);

  // Import from Iterator. This is the most generic interface
  static ErrorType ImportFromIterator(
      UserDictionaryImporter::InputIteratorInterface *iter,
      user_dictionary::UserDictionary *dic);

  // Import from TextLineIterator
  static ErrorType ImportFromTextLineIterator(
      UserDictionaryImporter::IMEType ime_type,
      UserDictionaryImporter::TextLineIteratorInterface *iter,
      user_dictionary::UserDictionary *dic);

  // Import from MS-IME's user dictionary directly.
  // Only available on Windows
  static ErrorType ImportFromMSIME(user_dictionary::UserDictionary *dic);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(UserDictionaryImporter);
};
}  // namespace mozc
#endif  // MOZC_DICTIONARY_USER_DICTIONARY_IMPORTER_H_
