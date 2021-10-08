// Copyright 2010-2021, Google Inc.
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
#include <cstdint>
#include <ctime>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include "base/double_array.h"
#include "base/port.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"

namespace mozc {

// SplitIterator - Iteratively splits a absl::string_view to
// sub-absl::string_views.
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
//   absl::string_view sp = iter.Get();  // "this", "is", and finally "mozc"
//   ...
// }
//
// // 2. SingleDelimiter and AllowEmpty
// for (SplitIterator<SingleDelimiter, AllowEmpty> iter("this,is,,mozc", ",");
//      !iter.Done(); iter.Next()) {
//   absl::string_view sp = iter.Get();  // "this", "is", "", and finally "mozc"
//   ...
// }
//
// // 3. MultiDelimiter and SkipEmpty
// for (SplitIterator<MultiDelimiter> iter("this,is:\tmozc", ",:\t");
//      !iter.Done(); iter.Next()) {
//   absl::string_view sp = iter.Get();  // "this", "is", and finally "mozc"
//   ...
// }
//
// // 4. MultiDelimiter and AllowEmpty
// for (SplitIterator<MultiDelimiter, AllowEmpty>
//          iter("this,is::\tmozc", ",:\t"); !iter.Done(); iter.Next()) {
//   absl::string_view sp = iter.Get();  // "this", "is", "", "", and finally
//   "mozc"
//   ...
// }
class SingleDelimiter;
class MultiDelimiter;
struct SkipEmpty {};
struct AllowEmpty {};

template <typename Delimiter, typename Option = SkipEmpty>
class SplitIterator {
 public:
  SplitIterator(absl::string_view s, const char *delim);
  absl::string_view Get() const;
  void Next();
  bool Done() const;
};

class Util {
 public:
  // String utils
  static void SplitStringUsing(absl::string_view str, const char *delim,
                               std::vector<std::string> *output);
  static void SplitStringUsing(absl::string_view str, const char *delim,
                               std::vector<absl::string_view> *output);

  static void SplitStringAllowEmpty(absl::string_view str, const char *delim,
                                    std::vector<std::string> *output);

  static void SplitStringToUtf8Chars(absl::string_view str,
                                     std::vector<std::string> *output);

  static void SplitCSV(const std::string &input,
                       std::vector<std::string> *output);

  template <typename Range>
  static std::string JoinStrings(const Range &range, absl::string_view delim) {
    return absl::StrJoin(range, delim);
  }

  template <typename Range>
  static void JoinStrings(const Range &range, absl::string_view delim,
                          std::string *output) {
    output->assign(absl::StrJoin(range, delim));
  }

  template <typename T>
  static std::string JoinStrings(std::initializer_list<T> il,
                                 absl::string_view delim) {
    return absl::StrJoin(il, delim);
  }

  // Concatenate s1 and s2 then store it to output.
  // s1 or s2 should not be a reference into output.
  static void ConcatStrings(absl::string_view s1, absl::string_view s2,
                            std::string *output);

  static void AppendStringWithDelimiter(absl::string_view delimiter,
                                        absl::string_view append_string,
                                        std::string *output);

  static void StringReplace(absl::string_view s, absl::string_view oldsub,
                            absl::string_view newsub, bool replace_all,
                            std::string *res);

  static void LowerString(std::string *str);
  static void UpperString(std::string *str);

  // Transforms the first character to the upper case and tailing characters to
  // the lower cases.  ex. "abCd" => "Abcd".
  static void CapitalizeString(std::string *str);

  // Returns true if the characters in [first, last) are all in lower case
  // ASCII.
  static bool IsLowerAscii(absl::string_view s);

  // Returns true if the characters in [first, last) are all in upper case
  // ASCII.
  static bool IsUpperAscii(absl::string_view s);

  // Returns true if the text in the rage [first, last) is capitalized ASCII.
  static bool IsCapitalizedAscii(absl::string_view s);

  // Returns true if the characters in [first, last) are all in lower case ASCII
  // or all in upper case ASCII. Namely, equivalent to
  //     IsLowerAscii(first, last) || IsUpperAscii(first last)
  static bool IsLowerOrUpperAscii(absl::string_view s);

  // Returns true if the text in the range [first, last) is 1) all in upper case
  // ASCII, or 2) capitalized.
  static bool IsUpperOrCapitalizedAscii(absl::string_view s);

  // Strips the leading/trailing white spaces from the input and stores it to
  // the output.  If the input does not have such white spaces, this method just
  // copies the input into the output.  It clears the output always.
  static void StripWhiteSpaces(const std::string &input, std::string *output);

  static size_t OneCharLen(const char *src);

  // Returns the lengths of [src, src+size] encoded in UTF8.
  static size_t CharsLen(const char *src, size_t size);

  static size_t CharsLen(absl::string_view str) {
    return CharsLen(str.data(), str.size());
  }

  // Converts the first character of UTF8 string starting at |begin| to UCS4.
  // The read byte length is stored to |mblen|.
  static char32 Utf8ToUcs4(const char *begin, const char *end, size_t *mblen);
  static char32 Utf8ToUcs4(absl::string_view s) {
    size_t mblen = 0;
    return Utf8ToUcs4(s.data(), s.data() + s.size(), &mblen);
  }

  // Converts a UCS4 code point to UTF8 string.
  static void Ucs4ToUtf8(char32 c, std::string *output);

  // Converts a UCS4 code point to UTF8 string and appends it to |output|, i.e.,
  // |output| is not cleared.
  static void Ucs4ToUtf8Append(char32 c, std::string *output);

  // Converts a UCS4 code point to UTF8 and stores it to char array.  The result
  // is terminated by '\0'.  Returns the byte length of converted UTF8 string.
  // REQUIRES: The output buffer must be longer than 7 bytes.
  static size_t Ucs4ToUtf8(char32 c, char *output);

  // Returns true if |s| is split into |first_char32| + |rest|.
  // You can pass nullptr to |first_char32| and/or |rest| to ignore the matched
  // value.
  // Returns false if an invalid UTF-8 sequence is prefixed. That is, |rest| may
  // contain any invalid sequence even when this method returns true.
  static bool SplitFirstChar32(absl::string_view s, char32 *first_char32,
                               absl::string_view *rest);

  // Returns true if |s| is split into |rest| + |last_char32|.
  // You can pass nullptr to |rest| and/or |last_char32| to ignore the matched
  // value.
  // Returns false if an invalid UTF-8 sequence is suffixed. That is, |rest| may
  // contain any invalid sequence even when this method returns true.
  static bool SplitLastChar32(absl::string_view s, absl::string_view *rest,
                              char32 *last_char32);

  // Returns true if |s| is a valid UTF8.
  static bool IsValidUtf8(absl::string_view s);

#ifdef OS_WIN
  // Returns how many wide characters are necessary in UTF-16 to represent
  // given UTF-8 string. Note that the result of this method becomes greater
  // than that of Util::CharsLen if |src| contains any character which is
  // encoded by the surrogate-pair in UTF-16.
  static size_t WideCharsLen(absl::string_view src);
  // Converts the encoding of the specified string from UTF-8 to UTF-16, and
  // vice versa.
  static int Utf8ToWide(absl::string_view input, std::wstring *output);
  static int WideToUtf8(const wchar_t *input, std::string *output);
  static int WideToUtf8(const std::wstring &input, std::string *output);
#endif  // OS_WIN

  // Extracts a substring range, where both start and length are in terms of
  // UTF8 size. Note that the returned string view refers to the same memory
  // block as the input.
  static absl::string_view Utf8SubString(absl::string_view src, size_t start,
                                         size_t length);
  // This version extracts the substring to the end.
  static absl::string_view Utf8SubString(absl::string_view src, size_t start);

  // Extracts a substring of length |length| starting at |start|.
  // Note: |start| is the start position in UTF8, not byte position.
  static void Utf8SubString(absl::string_view src, size_t start, size_t length,
                            std::string *result);

  // Determines whether the beginning of |str| matches |prefix|.
  static bool StartsWith(absl::string_view str, absl::string_view prefix);

  // Determines whether the end of |str| matches |suffix|.
  static bool EndsWith(absl::string_view str, absl::string_view suffix);

  // Strip a heading UTF-8 BOM (binary order mark) sequence (= \xef\xbb\xbf).
  static void StripUtf8Bom(std::string *line);

  // return true the line starts with UTF16-LE/UTF16-BE BOM.
  static bool IsUtf16Bom(const std::string &line);

  // Returns true if the given |s| has only one ucs4 character, and it is
  // in the range of Android Emoji PUA.
  static bool IsAndroidPuaEmoji(absl::string_view s);

  template <typename... Args>
  static std::string StringPrintf(const absl::FormatSpec<Args...> &format,
                                  const Args &...args) {
    return absl::StrFormat(format, args...);
  }

  // Chop the return characters (i.e. '\n' and '\r') at the end of the
  // given line.
  static bool ChopReturns(std::string *line);

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
  static void SetRandomSeed(uint32_t seed);

  // Suspends the execution of the current thread until
  // the time-out interval elapses.
  static void Sleep(uint32_t msec);

  // Japanese utilities for character form transliteration.
  static void ConvertUsingDoubleArray(const japanese_util_rule::DoubleArray *da,
                                      const char *table,
                                      absl::string_view input,
                                      std::string *output);
  static void HiraganaToKatakana(absl::string_view input, std::string *output);
  static void HiraganaToHalfwidthKatakana(absl::string_view input,
                                          std::string *output);
  static void HiraganaToRomanji(absl::string_view input, std::string *output);
  static void HalfWidthAsciiToFullWidthAscii(absl::string_view input,
                                             std::string *output);
  static void FullWidthAsciiToHalfWidthAscii(absl::string_view input,
                                             std::string *output);
  static void HiraganaToFullwidthRomanji(absl::string_view input,
                                         std::string *output);
  static void RomanjiToHiragana(absl::string_view input, std::string *output);
  static void KatakanaToHiragana(absl::string_view input, std::string *output);
  static void HalfWidthKatakanaToFullWidthKatakana(absl::string_view input,
                                                   std::string *output);
  static void FullWidthKatakanaToHalfWidthKatakana(absl::string_view input,
                                                   std::string *output);
  static void FullWidthToHalfWidth(absl::string_view input,
                                   std::string *output);
  static void HalfWidthToFullWidth(absl::string_view input,
                                   std::string *output);

  // Returns true if all chars in input are both defined
  // in full width and half-width-katakana area
  static bool IsFullWidthSymbolInHalfWidthKatakana(const std::string &input);

  // Returns true if all chars are defiend in half-width-katakana area.
  static bool IsHalfWidthKatakanaSymbol(const std::string &input);

  // Returns true if one or more Kana-symbol characters are in the input.
  static bool IsKanaSymbolContained(const std::string &input);

  // Returns true if |input| looks like a pure English word.
  static bool IsEnglishTransliteration(const std::string &value);

  static void NormalizeVoicedSoundMark(absl::string_view input,
                                       std::string *output);

  // Returns true if key is an open bracket.  If key is an open bracket,
  // corresponding close bracket is assigned.
  static bool IsOpenBracket(absl::string_view key, std::string *close_bracket);

  // Returns true if key is a close bracket.  If key is a close bracket,
  // corresponding open bracket is assigned.
  static bool IsCloseBracket(absl::string_view key, std::string *open_bracket);

  static void EncodeUri(const std::string &input, std::string *output);
  static void DecodeUri(const std::string &input, std::string *output);

  // Make a string for CGI parameters from params and append it to
  // base.  The result looks like:
  //   <base><key1>=<encoded val1>&<key2>=<encoded val2>
  // The base is supposed to end "?" or "&".
  static void AppendCgiParams(
      const std::vector<std::pair<std::string, std::string> > &params,
      std::string *base);

  // Escape any characters into \x prefixed hex digits.
  // ex.  "ABC" => "\x41\x42\x43".
  static void Escape(absl::string_view input, std::string *output);
  static std::string Escape(absl::string_view input);
  static bool Unescape(absl::string_view input, std::string *output);

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
  static ScriptType GetFirstScriptType(absl::string_view str);

  // return script type of string. all chars in str must be
  // KATAKANA/HIRAGANA/KANJI/NUMBER or ALPHABET.
  // If str has mixed scripts, this function returns UNKNOWN_SCRIPT
  static ScriptType GetScriptType(absl::string_view str);

  // The same as GetScryptType(), but it ignores symbols
  // in the |str|.
  static ScriptType GetScriptTypeWithoutSymbols(const std::string &str);

  // return true if all script_type in str is "type"
  static bool IsScriptType(absl::string_view str, ScriptType type);

  // return true if the string contains script_type char
  static bool ContainsScriptType(absl::string_view str, ScriptType type);

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
  static FormType GetFormType(const std::string &str);

  // Returns true if all characters of `str` are ASCII (U+00 - U+7F).
  static bool IsAscii(absl::string_view str);

  // Returns true if all characters of `str` are JIS X 0208.
  static bool IsJisX0208(absl::string_view str);

  // Serializes uint64 into a string of eight byte.
  static std::string SerializeUint64(uint64_t x);

  // Deserializes a string serialized by SerializeUint64.  Returns false if the
  // length of s is not eight or s is in an invalid format.
  static bool DeserializeUint64(absl::string_view s, uint64_t *x);

  // Checks endian-ness at runtime.
  static bool IsLittleEndian();

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
  explicit ConstChar32Iterator(absl::string_view utf8_string);
  char32 Get() const;
  void Next();
  bool Done() const;

 private:
  absl::string_view utf8_string_;
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
  explicit ConstChar32ReverseIterator(absl::string_view utf8_string);
  char32 Get() const;
  void Next();
  bool Done() const;

 private:
  absl::string_view utf8_string_;
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
  static constexpr size_t kTableSize = UCHAR_MAX / 8;

  explicit MultiDelimiter(const char *delim);

  bool Contains(char c) const {
    const unsigned char uc = static_cast<unsigned char>(c);
    return (lookup_table_[uc >> 3] & (1 << (uc & 0x07))) != 0;
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
  SplitIterator(absl::string_view s, const char *delim);
  absl::string_view Get() const {
    return absl::string_view(sp_begin_, sp_len_);
  }
  bool Done() const { return sp_begin_ == end_; }
  void Next();

 private:
  const char *const end_;
  const Delimiter delim_;
  const char *sp_begin_;
  absl::string_view::size_type sp_len_;

  DISALLOW_COPY_AND_ASSIGN(SplitIterator);
};

template <typename Delimiter>
class SplitIterator<Delimiter, AllowEmpty> {
 public:
  SplitIterator(absl::string_view s, const char *delim);
  absl::string_view Get() const {
    return absl::string_view(sp_begin_, sp_len_);
  }
  bool Done() const { return done_; }
  void Next();

 private:
  const char *const end_;
  const char *sp_begin_;
  absl::string_view::size_type sp_len_;
  const Delimiter delim_;
  bool done_;

  DISALLOW_COPY_AND_ASSIGN(SplitIterator);
};

}  // namespace mozc

#endif  // MOZC_BASE_UTIL_H_
