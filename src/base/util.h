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

#ifndef MOZC_BASE_UTIL_H_
#define MOZC_BASE_UTIL_H_

#include <climits>
#include <ctime>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/port.h"
#include "base/string_piece.h"

struct tm;

namespace mozc {

// SplitIterator - Iteratively splits a StringPiece to sub-StringPieces.
//
// This template class takes two template parameters, Delimiter and Option.
//
// Delimiter:
//   - SingleDelimiter: Input is splitted by only one character.  If your
//         delimiter is a single character, use this parameter because algorithm
//         is optimized for this common case.
//   - MultiDelimiter: Input is splitted by any of the specified characters.
//
// Option:
//   - SkipEmpty (default): empty pieces are ignored:
//         ",a,b,,c," -> {"a", "b", "c"}  (delimiter = ',')
//   - AllowEmpty: empty pieces are not ignored:
//         ",a,b,,c," -> {"", "a", "b", "", "c", ""}  (delimiter = ',')
//
// Usage Example:
//
// // 1. SingleDelimiter and SkipEmpty
// for (SplitIterator<SingleDelimiter> iter("this,is,,mozc", ",");
//      !iter.Done(); iter.Next()) {
//   StringPiece sp = iter.Get();  // "this", "is", and finally "mozc"
//   ...
// }
//
// // 2. SingleDelimiter and AllowEmpty
// for (SplitIterator<SingleDelimiter, AllowEmpty> iter("this,is,,mozc", ",");
//      !iter.Done(); iter.Next()) {
//   StringPiece sp = iter.Get();  // "this", "is", "", and finally "mozc"
//   ...
// }
//
// // 3. MultiDelimiter and SkipEmpty
// for (SplitIterator<MultiDelimiter> iter("this,is:\tmozc", ",:\t");
//      !iter.Done(); iter.Next()) {
//   StringPiece sp = iter.Get();  // "this", "is", and finally "mozc"
//   ...
// }
//
// // 4. MultiDelimiter and AllowEmpty
// for (SplitIterator<MultiDelimiter, AllowEmpty>
//          iter("this,is::\tmozc", ",:\t"); !iter.Done(); iter.Next()) {
//   StringPiece sp = iter.Get();  // "this", "is", "", "", and finally "mozc"
//   ...
// }
class SingleDelimiter;
class MultiDelimiter;
struct SkipEmpty {};
struct AllowEmpty {};

template <typename Delimiter, typename Option = SkipEmpty>
class SplitIterator {
 public:
  SplitIterator(StringPiece s, const char *delim);
  StringPiece Get() const;
  void Next();
  bool Done() const;
};

class Util {
 public:
  // String utils
  template <typename StringContainer>
  static void PushBackStringPiece(StringPiece s, StringContainer *container) {
    container->push_back(string());
    s.CopyToString(&container->back());
  }

  static void SplitStringUsing(StringPiece str,
                               const char *delm,
                               vector<string> *output);
  static void SplitStringUsing(StringPiece str,
                               const char *delm,
                               vector<StringPiece> *output);

  static void SplitStringAllowEmpty(StringPiece str,
                                    const char *delm,
                                    vector<string> *output);

  static void SplitStringToUtf8Chars(const string &str,
                                     vector<string> *output);

  static void SplitCSV(const string &str, vector<string> *output);

  static void JoinStrings(const vector<string> &str,
                          const char *delm,
                          string *output);
  static void JoinStringPieces(const vector<StringPiece> &str,
                               const char *delm,
                               string *output);

  static void AppendStringWithDelimiter(StringPiece delimiter,
                                        StringPiece append_string,
                                        string *output);

  static void StringReplace(StringPiece s, StringPiece oldsub,
                            StringPiece newsub, bool replace_all,
                            string *res);

  static void LowerString(string *output);
  static void UpperString(string *output);

  // Transforms the first character to the upper case and tailing characters to
  // the lower cases.  ex. "abCd" => "Abcd".
  static void CapitalizeString(string *output);

  // Returns true if the characters in [first, last) are all in lower case
  // ASCII.
  static bool IsLowerAscii(StringPiece s);

  // Returns true if the characters in [first, last) are all in upper case
  // ASCII.
  static bool IsUpperAscii(StringPiece s);

  // Returns true if the text in the rage [first, last) is capitalized ASCII.
  static bool IsCapitalizedAscii(StringPiece s);

  // Returns true if the characters in [first, last) are all in lower case ASCII
  // or all in upper case ASCII. Namely, equivalent to
  //     IsLowerAscii(first, last) || IsUpperAscii(first last)
  static bool IsLowerOrUpperAscii(StringPiece s);

  // Returns true if the text in the range [first, last) is 1) all in upper case
  // ASCII, or 2) capitalized.
  static bool IsUpperOrCapitalizedAscii(StringPiece s);

  // Strips the leading/trailing white spaces from the input and stores it to
  // the output.  If the input does not have such white spaces, this method just
  // copies the input into the output.  It clears the output always.
  static void StripWhiteSpaces(const string &str, string *output);

  static size_t OneCharLen(const char *src);

  static size_t CharsLen(const char *src, size_t size);

  static size_t CharsLen(const StringPiece str) {
    return CharsLen(str.data(), str.size());
  }

  static char32 UTF8ToUCS4(const char *begin,
                           const char *end,
                           size_t *mblen);

  // Returns true if |s| is split into |first_char32| + |rest|.
  // You can pass NULL to |first_char32| and/or |rest| to ignore the matched
  // value.
  // Returns false if an invalid UTF-8 sequence is prefixed. That is, |rest| may
  // contain any invalid sequence even when this method returns true.
  static bool SplitFirstChar32(StringPiece s,
                               char32 *first_char32,
                               StringPiece *rest);

  // Returns true if |s| is split into |rest| + |last_char32|.
  // You can pass NULL to |rest| and/or |last_char32| to ignore the matched
  // value.
  // Returns false if an invalid UTF-8 sequence is suffixed. That is, |rest| may
  // contain any invalid sequence even when this method returns true.
  static bool SplitLastChar32(StringPiece s,
                              StringPiece *rest,
                              char32 *last_char32);

  static void UCS4ToUTF8(char32 c, string *output);
  static void UCS4ToUTF8Append(char32 c, string *output);

#ifdef OS_WIN
  // Returns how many wide characters are necessary in UTF-16 to represent
  // given UTF-8 string. Note that the result of this method becomes greater
  // than that of Util::CharsLen if |src| contains any character which is
  // encoded by the surrogate-pair in UTF-16.
  static size_t WideCharsLen(StringPiece src);
  // Converts the encoding of the specified string from UTF-8 to UTF-16, and
  // vice versa.
  static int UTF8ToWide(StringPiece input, wstring *output);
  static int WideToUTF8(const wchar_t *input, string *output);
  static int WideToUTF8(const wstring &input, string *output);
#endif  // OS_WIN

  // Extracts a substring range, where both start and length are in terms of
  // UTF8 size. Note that the returned string piece refers to the same memory
  // block as the input.
  static StringPiece SubStringPiece(const StringPiece src,
                                    const size_t start, const size_t length);

  static void SubString(const StringPiece src,
                        const size_t start, const size_t length,
                        string *result);

  static string SubString(const StringPiece src,
                          const size_t start, const size_t length) {
    string result;
    SubString(src, start, length, &result);
    return result;
  }

  // Determines whether the beginning of |str| matches |prefix|.
  static bool StartsWith(const StringPiece str, const StringPiece prefix);

  // Determines whether the end of |str| matches |suffix|.
  static bool EndsWith(const StringPiece str, const StringPiece suffix);

  // Strip a heading UTF-8 BOM (binary order mark) sequence (= \xef\xbb\xbf).
  static void StripUTF8BOM(string *line);

  // return true the line starts with UTF16-LE/UTF16-BE BOM.
  static bool IsUTF16BOM(const string &line);

  // Returns true if the given |s| has only one ucs4 character, and it is
  // in the range of Android Emoji PUA.
  static bool IsAndroidPuaEmoji(const StringPiece s);

#ifndef SWIG
  // C++ string version of sprintf.
  static string StringPrintf(const char *format, ...)
      // Tell the compiler to do printf format string checking.
      PRINTF_ATTRIBUTE(1, 2);
#endif  // SWIG

  // Chop the return characters (i.e. '\n' and '\r') at the end of the
  // given line.
  static bool ChopReturns(string *line);

  // 32bit Fingerprint
  static uint32 Fingerprint32(const string &key);
#ifndef SWIG
  static uint32 Fingerprint32(const char *str, size_t length);
  static uint32 Fingerprint32(const char *str);
#endif  // SWIG

  static uint32 Fingerprint32WithSeed(const string &key,
                                      uint32 seed);
#ifndef SWIG
  static uint32 Fingerprint32WithSeed(const char *str,
                                      size_t length, uint32 seed);
  static uint32 Fingerprint32WithSeed(const char *str,
                                      uint32 seed);
#endif  // SWIG
  static uint32 Fingerprint32WithSeed(uint32 num, uint32 seed);

  // 64bit Fingerprint
  static uint64 Fingerprint(const string &key);
  static uint64 Fingerprint(const char *str, size_t length);

  static uint64 FingerprintWithSeed(const string &key, uint32 seed);

  static uint64 FingerprintWithSeed(const char *str,
                                    size_t length, uint32 seed);

  // Generate a random sequence. It uses secure method if possible, or Random()
  // as a fallback method.
  static void GetRandomSequence(char *buf, size_t buf_size);
  static void GetRandomAsciiSequence(char *buf, size_t buf_size);

  // Return random variable whose range is [0..size-1].
  // This function uses rand() internally, so don't use it for
  // security-sensitive purpose.
  // Caveats: The returned value does not have good granularity especially
  // when |size| is larger than |RAND_MAX|.
  // TODO(yukawa): Improve the granularity.
  // TODO(yukawa): Clarify the semantics when |size| is 0 or smaller.
  static int Random(int size);

  // Set the seed of Util::Random().
  static void SetRandomSeed(uint32 seed);

  // Get the current time info using gettimeofday-like functions.
  // sec: number of seconds from epoch
  // usec: micro-second passed: [0,1000000)
  static void GetTimeOfDay(uint64 *sec, uint32 *usec);

  // Get the current time info using time-like function
  // For Windows, _time64() is used.
  // For Linux/Mac, time() is used.
  static uint64 GetTime();

  // Get the current local time to current_time.  Returns true if succeeded.
  static bool GetCurrentTm(tm *current_time);
  // Get local time, which is offset_sec seconds after now. Returns true if
  // succeeded.
  static bool GetTmWithOffsetSecond(tm *time_with_offset, int offset_sec);

  // Get the system frequency to calculate the time from ticks.
  static uint64 GetFrequency();

  // Get the current ticks. It may return incorrect value on Virtual Machines.
  // If you'd like to get a value in secs, it is necessary to divide a result by
  // GetFrequency().
  static uint64 GetTicks();

#ifdef __native_client__
  // Sets the time difference between local time and UTC time in seconds.
  // We use this function in NaCl Mozc because we can't know the local timezone
  // in NaCl environment.
  static void SetTimezoneOffset(int32 timezone_offset_sec);
#endif  // __native_client__

  // Interface of the helper class.
  // Default implementation is defined in the .cc file.
  class ClockInterface {
   public:
    virtual ~ClockInterface() {}
    virtual void GetTimeOfDay(uint64 *sec, uint32 *usec) = 0;
    virtual uint64 GetTime() = 0;
    virtual bool GetTmWithOffsetSecond(time_t offset_sec, tm *output) = 0;

    // High accuracy clock.
    virtual uint64 GetFrequency() = 0;
    virtual uint64 GetTicks() = 0;
#ifdef __native_client__
    virtual void SetTimezoneOffset(int32 timezone_offset_sec) = 0;
#endif  // __native_client__
  };

  // This function is provided for test.
  // The behavior of system clock can be customized by replacing this handler.
  static void SetClockHandler(Util::ClockInterface *handler);

  // Suspends the execution of the current thread until
  // the time-out interval elapses.
  static void Sleep(uint32 msec);

  // Japanese utilities for character form transliteration.
  static void HiraganaToKatakana(const StringPiece input, string *output);
  static void HiraganaToHalfwidthKatakana(const StringPiece input,
                                          string *output);
  static void HiraganaToRomanji(const StringPiece input, string *output);
  static void HalfWidthAsciiToFullWidthAscii(const StringPiece input,
                                             string *output);
  static void FullWidthAsciiToHalfWidthAscii(const StringPiece input,
                                             string *output);
  static void HiraganaToFullwidthRomanji(const StringPiece input,
                                         string *output);
  static void RomanjiToHiragana(const StringPiece input, string *output);
  static void KatakanaToHiragana(const StringPiece input, string *output);
  static void HalfWidthKatakanaToFullWidthKatakana(const StringPiece input,
                                                   string *output);
  static void FullWidthKatakanaToHalfWidthKatakana(const StringPiece input,
                                                   string *output);
  static void FullWidthToHalfWidth(const StringPiece input, string *output);
  static void HalfWidthToFullWidth(const StringPiece input, string *output);

#ifdef SWIG
  // Since StringPiece is not supported by SWIG, provide overloads for string
  // versions.
  static void HiraganaToKatakana(const string &input, string *output) {
    HiraganaToKatakana(StringPiece(input), output);
  }
  static void RomanjiToHiragana(const string &input, string *output) {
    RomanjiToHiragana(StringPiece(input), output);
  }
  static void KatakanaToHiragana(const string &input, string *output) {
    KatakanaToHiragana(StringPiece(input), output);
  }
#endif  // SWIG

  // return true if all chars in input are both defined
  // in full width and half-width-katakana area
  static bool IsFullWidthSymbolInHalfWidthKatakana(const string &input);

  // return true if all chars are defiend in
  // half-width-katakana area.
  static bool IsHalfWidthKatakanaSymbol(const string &input);

  // return true if one or more Kana-symbol characters are
  // in the input.
  static bool IsKanaSymbolContained(const string &input);

  // return true if |input| looks like a pure English word.
  static bool IsEnglishTransliteration(const string &input);

  static void NormalizeVoicedSoundMark(StringPiece input, string *output);

  // return true if key is an open bracket.
  // if key is an open bracket, corresponding close bracket is
  // assigned
  static bool IsOpenBracket(const string &key, string *close_bracket);

  // return true if key is a close bracket.
  // if key is a close bracket, corresponding open bracket is
  // assigned.
  static bool IsCloseBracket(const string &key, string *open_bracket);

  // Code converter
#ifndef OS_WIN
  static void UTF8ToEUC(const string &input, string *output);
  static void EUCToUTF8(const string &input, string *output);
#endif  // OS_WIDNWOS

  static void UTF8ToSJIS(const string &input, string *output);
  static void SJISToUTF8(const string &input, string *output);
  static bool ToUTF8(const char *from, const string &input, string *output);

  static void EncodeURI(const string &input, string *output);
  static void DecodeURI(const string &input, string *output);

  // Make a string for CGI parameters from params and append it to
  // base.  The result looks like:
  //   <base><key1>=<encoded val1>&<key2>=<encoded val2>
  // The base is supposed to end "?" or "&".
  static void AppendCGIParams(const vector<pair<string, string> > &params,
                              string *base);

  // Escape any characters into \x prefixed hex digits.
  // ex.  "ABC" => "\x41\x42\x43".
  static void Escape(const string &input, string *output);

  // Escape any characters into % prefixed hex digits.
  // ex. "ABC" => "%41%42%43"
  static void EscapeUrl(const string &input, string *output);
  static string EscapeUrl(const string &input);

  // Escape/Unescape unsafe html characters such as <, > and &.
  static void EscapeHtml(const string &text, string *res);
  static void UnescapeHtml(const string &text, string *res);

  // Escape unsafe CSS characters like <.  Note > and & are not
  // escaped becaused they are operands of CSS.
  static void EscapeCss(const string &text, string *result);

  enum ScriptType {
    UNKNOWN_SCRIPT,
    KATAKANA,
    HIRAGANA,
    KANJI,
    NUMBER,
    ALPHABET,
    EMOJI,
    SCRIPT_TYPE_SIZE,
  };

  // return script type of w
  static ScriptType GetScriptType(char32 w);

  // return script type of first character in [begin, end)
  // This function finds the first UTF-8 chars and returns its script type.
  // The length of the character will be returned in *mblen.
  // This function calls GetScriptType(char32) internally.
  static ScriptType GetScriptType(const char *begin, const char *end,
                                  size_t *mblen);

  // return script type of first character in str
  static ScriptType GetFirstScriptType(const string &str);

  // return script type of string. all chars in str must be
  // KATAKANA/HIRAGANA/KANJI/NUMBER or ALPHABET.
  // If str has mixed scripts, this function returns UNKNOWN_SCRIPT
  static ScriptType GetScriptType(const string &str);

  // The same as GetScryptType(), but it ignores symbols
  // in the |str|.
  static ScriptType GetScriptTypeWithoutSymbols(const string &str);

  // return true if all script_type in str is "type"
  static bool IsScriptType(const StringPiece str, ScriptType type);

  // return true if the string contains script_type char
  static bool ContainsScriptType(const StringPiece str, ScriptType type);

  // See 'Unicode Standard Annex #11: EAST ASIAN WIDTH'
  // http://www.unicode.org/reports/tr11/
  // http://www.unicode.org/Public/UNIDATA/EastAsianWidth.txt
  enum FormType {
    UNKNOWN_FORM,
    HALF_WIDTH,  // [Na] and [H] in 'Unicode Standard Annex #11'
    FULL_WIDTH,  // Any other characters
    FORM_TYPE_SIZE,
  };

  // return Form type of single character.
  // This function never returns UNKNOWN_FORM.
  static FormType GetFormType(char32 w);

  // return FormType of string.
  // return UNKNOWN_FORM if |str| contains both HALF_WIDTH and FULL_WIDTH.
  static FormType GetFormType(const string &str);

  // Basically, if charset >= JIX0212, the char is platform dependent char.
  enum CharacterSet {
    ASCII,         // ASCII (simply ucs4 <= 0x007F)
    JISX0201,      // defined at least in 0201 (can be in 0208/0212/0213/CP9232)
    JISX0208,      // defined at least in 0208 (can be in 0212/0213/CP932)
    JISX0212,      // defined at least in 0212 (can be in 0213/CP932)
    JISX0213,      // defined at least in 0213 (can be in CP932)
    CP932,         // defined only in CP932, not in JISX02*
    UNICODE_ONLY,  // defined only in UNICODE, not in JISX* nor CP932
    CHARACTER_SET_SIZE,
  };

  // return CharacterSet
  static CharacterSet GetCharacterSet(char32 ucs4);

  // return CharacterSet of string.
  // if the given string contains multiple charasets, return
  // the maximum character set.
  static CharacterSet GetCharacterSet(const string &str);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Util);
};

// Const iterator implementation to traverse on a (utf8) string as a char32
// string.
//
// Example usage:
//   string utf8;
//   for (ConstChar32Iterator iter(utf8); !iter.Done(); iter.Next()) {
//     char32 c = iter.Get();
//     ...
//   }
class ConstChar32Iterator {
 public:
  explicit ConstChar32Iterator(StringPiece utf8_string);
  char32 Get() const;
  void Next();
  bool Done() const;

 private:
  StringPiece utf8_string_;
  char32 current_;
  bool done_;

  DISALLOW_COPY_AND_ASSIGN(ConstChar32Iterator);
};

// Const reverse iterator implementation to traverse on a (utf8) string as a
// char32 string.
//
// Example usage:
//   string utf8;
//   for (ConstChar32ReverseIterator iter(utf8); !iter.Done(); iter.Next()) {
//     char32 c = iter.Get();
//     ...
//   }
class ConstChar32ReverseIterator {
 public:
  explicit ConstChar32ReverseIterator(StringPiece utf8_string);
  char32 Get() const;
  void Next();
  bool Done() const;

 private:
  StringPiece utf8_string_;
  char32 current_;
  bool done_;

  DISALLOW_COPY_AND_ASSIGN(ConstChar32ReverseIterator);
};

// Actual definitions of delimiter classes.
class SingleDelimiter {
 public:
  explicit SingleDelimiter(const char *delim) : delim_(*delim) {}
  bool Contains(char c) const { return c == delim_; }

 private:
  const char delim_;
  DISALLOW_COPY_AND_ASSIGN(SingleDelimiter);
};

class MultiDelimiter {
 public:
  static const size_t kTableSize = UCHAR_MAX / 8;

  explicit MultiDelimiter(const char* delim);

  bool Contains(char c) const {
    const unsigned char uc = static_cast<unsigned char>(c);
    return lookup_table_[uc >> 3] & (1 << (uc & 0x07));
  }

 private:
  // Bit field for looking up delimiters. Think of this as a 256-bit array where
  // n-th bit is set to 1 if the delimiters contain a character whose unsigned
  // char code is n.
  unsigned char lookup_table_[kTableSize];
  DISALLOW_COPY_AND_ASSIGN(MultiDelimiter);
};

// Declarations of the partial specializations of SplitIterator for two options.
// Implementations are explicitly instantiated in util.cc.
template <typename Delimiter>
class SplitIterator<Delimiter, SkipEmpty> {
 public:
  SplitIterator(StringPiece s, const char *delim);
  StringPiece Get() const { return StringPiece(sp_begin_, sp_len_); }
  bool Done() const { return sp_begin_ == end_; }
  void Next();

 private:
  const char *const end_;
  const Delimiter delim_;
  const char *sp_begin_;
  StringPiece::size_type sp_len_;

  DISALLOW_COPY_AND_ASSIGN(SplitIterator);
};

template <typename Delimiter>
class SplitIterator<Delimiter, AllowEmpty> {
 public:
  SplitIterator(StringPiece s, const char *delim);
  StringPiece Get() const { return StringPiece(sp_begin_, sp_len_); }
  bool Done() const { return done_; }
  void Next();

 private:
  const char *const end_;
  const Delimiter delim_;
  const char *sp_begin_;
  StringPiece::size_type sp_len_;
  bool done_;

  DISALLOW_COPY_AND_ASSIGN(SplitIterator);
};

}  // namespace mozc

#endif  // MOZC_BASE_UTIL_H_
