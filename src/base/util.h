// Copyright 2010-2012, Google Inc.
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

#include <string>
#include <utility>
#include <vector>

#include "base/base.h"

struct tm;

namespace mozc {

class Util {
 public:
  // String utils
  static void SplitStringUsing(const string &str,
                               const char *delm,
                               vector<string> *output);

  static void SplitStringAllowEmpty(const string &str,
                                    const char *delm,
                                    vector<string> *output);

  static void SplitStringToUtf8Chars(const string &str,
                                     vector<string> *output);

  static void SplitCSV(const string &str, vector<string> *output);

  static void JoinStrings(const vector<string> &str,
                          const char *delm,
                          string *output);

  static void AppendStringWithDelimiter(const string &delimiter,
                                        const string &append_string,
                                        string *output);

  static void StringReplace(const string &s, const string &oldsub,
                            const string &newsub, bool replace_all,
                            string *res);

  static void LowerString(string *output);
  static void UpperString(string *output);

  // Transform the first character to the upper case and tailing
  // characters to the lower cases.  ex. "abCd" => "Abcd".
  static void CapitalizeString(string *output);

  // Strip the leading/trailing white spaces from the input and stores
  // it to the output.  If the input does not have such white spaces,
  // this method just copies the input into the output.  It clears
  // the output always.
  static void StripWhiteSpaces(const string &str, string *output);

  static size_t OneCharLen(const char *src);

  static size_t CharsLen(const char *src, size_t size);

  static size_t CharsLen(const string &str) {
    return CharsLen(str.c_str(), str.size());
  }

  static char32 UTF8ToUCS4(const char *begin,
                           const char *end,
                           size_t *mblen);

  static void UCS4ToUTF8(char32 c, string *output);
  static void UCS4ToUTF8Append(char32 c, string *output);

  // The return value overflow if the UTF8-bytes represent the
  // character with code point >0xFFFF.
  static uint16 UTF8ToUCS2(const char *begin,
                           const char *end,
                           size_t *mblen);

  // Convert UCS2 code point to UTF8 string
  static void UCS2ToUTF8(uint16 c, string *output);
  static void UCS2ToUTF8Append(uint16 c, string *output);

#ifdef OS_WINDOWS
  // Returns how many wide characters are necessary in UTF-16 to represent
  // given UTF-8 string. Note that the result of this method becomes greater
  // than that of Util::CharsLen if |src| contains any character which is
  // encoded by the surrogate-pair in UTF-16.
  static size_t WideCharsLen(const char *src);
  static size_t WideCharsLen(const string &src);
  // Converts the encoding of the specified string from UTF-8 to UTF-16, and
  // vice versa.
  static int UTF8ToWide(const char *input, wstring *output);
  static int UTF8ToWide(const string &input, wstring *output);
  static int WideToUTF8(const wchar_t *input, string *output);
  static int WideToUTF8(const wstring &input, string *output);
#endif

  static void SubString(const string &src,
                        const size_t start, const size_t length,
                        string *result);

  static string SubString(const string &src,
                          const size_t start, const size_t length) {
    string result;
    SubString(src, start, length, &result);
    return result;
  }

  // Determines whether the beginning of |str| matches |prefix|.
  static bool StartsWith(const string &str, const string &prefix);

  // Determines whether the end of |str| matches |suffix|.
  static bool EndsWith(const string &str, const string &suffix);

  // Strip a heading UTF-8 BOM (binary order mark) sequence (= \xef\xbb\xbf).
  static void StripUTF8BOM(string *line);

  // return true the line starts with UTF16-LE/UTF16-BE BOM.
  static bool IsUTF16BOM(const string &line);

  // Convert the number to a string and append it to output.
  static string SimpleItoa(int32 number);

  // Convert the string to a number and return it.
  static int SimpleAtoi(const string &str);

  // Returns true if the given input_string contains only number characters
  // (regardless of halfwidth or fullwidth).
  static bool IsArabicNumber(const string &input_string);

  struct NumberString {
   public:
    enum Style {
        DEFAULT_STYLE = 0,
        // 123,456,789
        NUMBER_SEPARATED_ARABIC_HALFWIDTH,
        // "１２３，４５６，７８９"
        NUMBER_SEPARATED_ARABIC_FULLWIDTH,
        // "一億二千三百四十五万六千七百八十九"
        NUMBER_KANJI,
        // "壱億弐千参百四拾五万六千七百八拾九"
        NUMBER_OLD_KANJI,
        // "ⅠⅡⅢ"
        NUMBER_ROMAN_CAPITAL,
        // "ⅰⅱⅲ"
        NUMBER_ROMAN_SMALL,
        // "①②③"
        NUMBER_CIRCLED,
        // "0x4d2" (1234 in decimal)
        NUMBER_HEX,
        // "02322" (1234 in decimal)
        NUMBER_OCT,
        // "0b10011010010" (1234 in decimal)
        NUMBER_BIN,
        // "ニ〇〇"
        NUMBER_KANJI_ARABIC,
    };

    NumberString(const string &value, const string &description, Style style)
        : value(value),
          description(description),
          style(style) {}

    // Converted string
    string value;

    // Description of Converted String
    string description;

    // Converted Number Style
    Style style;
  };

  // Converts half-width Arabic number string to Kan-su-ji string
  //    - input_num: a string which *must* be half-width number string
  //    - output: function appends new representation into output vector.
  // value, desc and style are stored same size and same order.
  // if invalid string is set, this function do nothing.
  static bool ArabicToKanji(const string &input_num,
                            vector<Util::NumberString> *output);

  // Converts half-width Arabic number string to Separated Arabic string
  //  (e.g. 1234567890 are converted to 1,234,567,890)
  // Arguments are same as ArabicToKanji (above)
  static bool ArabicToSeparatedArabic(const string &input_num,
                                      vector<Util::NumberString> *output);

  // Converts half-width Arabic number string to full-width Arabic number string
  // Arguments are same as ArabicToKanji (above)
  static bool ArabicToWideArabic(const string &input_num,
                                 vector<Util::NumberString> *output);

  // Converts half-width Arabic number to various styles
  // Arguments are same as ArabicToKanji (above)
  //    - Roman style (i) (ii) ...
  static bool ArabicToOtherForms(const string &input_num,
                                 vector<Util::NumberString> *output);

  // Converts half-width Arabic number to various radices (2,8,16)
  // Arguments are same as ArabicToKanji (above)
  //   except input digits is smaller than 20
  static bool ArabicToOtherRadixes(const string &input_num,
                                   vector<Util::NumberString> *output);

  // Converts the string to a 32-/64-bit unsigned int.  Returns true if success
  // or false if the string is in the wrong format.
  static bool SafeStrToUInt32(const string &str, uint32 *value);
  static bool SafeStrToUInt64(const string &str, uint64 *value);
  static bool SafeHexStrToUInt32(const string &str, uint32 *value);
  static bool SafeOctStrToUInt32(const string &str, uint32 *value);

  // Converts the string to a double.  Returns true if success or false if the
  // string is in the wrong format.
  // If |str| is a hexadecimal number like "0x1234", the result depends on
  // compiler.  It returns false when compiled by VisualC++.  On the other hand
  // it returns true and sets correct value when compiled by gcc.
  static bool SafeStrToDouble(const string &str, double *value);

  // Converts the string to a float. Returns true if success or false if the
  // string is in the wrong format.
  static bool SafeStrToFloat(const string &str, float *value);
  // Converts the string to a float.
  static float StrToFloat(const string &str) {
    float value;
    Util::SafeStrToFloat(str, &value);
    return value;
  }

#ifndef SWIG
  // C++ string version of sprintf.
  static string StringPrintf(const char *format, ...)
      // Tell the compiler to do printf format string checking.
      PRINTF_ATTRIBUTE(1, 2);
#endif

  // Chop the return characters (i.e. '\n' and '\r') at the end of the
  // given line.
  static bool ChopReturns(string *line);

  // 32bit Fingerprint
  static uint32 Fingerprint32(const string &key);
#ifndef SWIG
  static uint32 Fingerprint32(const char *str, size_t length);
  static uint32 Fingerprint32(const char *str);
#endif

  static uint32 Fingerprint32WithSeed(const string &key,
                                      uint32 seed);
#ifndef SWIG
  static uint32 Fingerprint32WithSeed(const char *str,
                                      size_t length, uint32 seed);
  static uint32 Fingerprint32WithSeed(const char *str,
                                      uint32 seed);
#endif
  static uint32 Fingerprint32WithSeed(uint32 num, uint32 seed);

  // 64bit Fingerprint
  static uint64 Fingerprint(const string &key);
  static uint64 Fingerprint(const char *str, size_t length);

  static uint64 FingerprintWithSeed(const string &key, uint32 seed);

  static uint64 FingerprintWithSeed(const char *str,
                                    size_t length, uint32 seed);

  // Fill a given buffer with random characters
  static bool GetSecureRandomSequence(char *buf, size_t buf_size);
  static bool GetSecureRandomAsciiSequence(char *buf, size_t buf_size);

  // Return random variable whose range is [0..size-1].
  // This function uses rand() internally, so don't use it for
  // security-sensitive purpose.
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
  };

  // This function is provided for test.
  // The behavior of system clock can be customized by replacing this handler.
  static void SetClockHandler(Util::ClockInterface *handler);

  // Suspends the execution of the current thread until
  // the time-out interval elapses.
  static void Sleep(uint32 msec);

  // Convert Kanji numeric into Arabic numeric.
  // When the trim_leading_zeros is true, leading zeros for arabic_output
  // are trimmed off.
  // TODO(toshiyuki): This parameter is only applied for arabic_output now.
  //
  // Input: "2千五百"
  // kanji_output: "二千五百"
  // arabic output: 2500
  //
  // NormalizeNumbers() returns false if it finds non-number characters.
  // NormalizeNumbersWithSuffix() skips trailing non-number characters and
  // return them in "suffix".
  static bool NormalizeNumbers(const string &input,
                               bool trim_leading_zeros,
                               string *kanji_output,
                               string *arabic_output);

  static bool NormalizeNumbersWithSuffix(const string &input,
                                         bool trim_leading_zeros,
                                         string *kanji_output,
                                         string *arabic_output,
                                         string *suffix);

  // Japanese utils
  static void HiraganaToKatakana(const string &input,
                                 string *output);

  static void HiraganaToHalfwidthKatakana(const string &input,
                                          string *output);

  static void HiraganaToRomanji(const string &input,
                                string *output);

  static void HalfWidthAsciiToFullWidthAscii(const string &input,
                                             string *output);

  static void FullWidthAsciiToHalfWidthAscii(const string &input,
                                             string *output);

  static void HiraganaToFullwidthRomanji(const string &input,
                                         string *output);

  static void RomanjiToHiragana(const string &input,
                                string *output);

  static void KatakanaToHiragana(const string &input,
                                 string *output);

  static void HalfWidthKatakanaToFullWidthKatakana(const string &input,
                                                   string *output);

  static void FullWidthKatakanaToHalfWidthKatakana(const string &input,
                                                   string *output);

  static void FullWidthToHalfWidth(const string &input,
                                   string *output);

  static void HalfWidthToFullWidth(const string &input,
                                   string *output);

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

  static void NormalizeVoicedSoundMark(const string &input,
                                       string *output);

  // Note: this function just does charcter-by-character conversion
  // "百二十" -> 10020
  static void KanjiNumberToArabicNumber(const string &input,
                                        string *output);

  // return true if key is an open bracket.
  // if key is an open bracket, corresponding close bracket is
  // assigned
  static bool IsOpenBracket(const string &key, string *close_bracket);

  // return true if key is a close bracket.
  // if key is a close bracket, corresponding open bracket is
  // assigned.
  static bool IsCloseBracket(const string &key, string *open_bracket);

  // Code converter
#ifndef OS_WINDOWS
  static void UTF8ToEUC(const string &input, string *output);
  static void EUCToUTF8(const string &input, string *output);
#endif

  static void UTF8ToSJIS(const string &input, string *output);
  static void SJISToUTF8(const string &input, string *output);
  static bool ToUTF8(const char *from, const string &input, string *output);

  // Filesystem or user related methods are disabled on Native Client
  // environment.
#ifndef __native_client__
  // File and directory operations
  static bool CreateDirectory(const string &path);
  static bool RemoveDirectory(const string &dirname);
  static bool Unlink(const string &filename);
  static bool FileExists(const string &filename);
  static bool DirectoryExists(const string &filename);
  static bool Rename(const string &from, const string &to);

  // This function has a limitation. See comment in the .cc file.
  // This function opens a file with text mode. The return code
  // may be different between |from| and |to|.
  static bool CopyTextFile(const string &from, const string &to);

  // CopyFile uses mmap internally. |from| and |to| should
  // be identical.
  static bool CopyFile(const string &from, const string &to);

  // Return true if |filename1| and |filename2|
  // are identical.
  static bool IsEqualFile(const string &filename1,
                          const string &filename2);

  // Move/Rename file atomically.
  // Vista or Later: use Transactional NTFS API, which guarantees atomic
  // file move operation.
  // When anything wrong happen during the transactional NTFS api, execute
  // the fallback plan, which is the same as the treatment for Windows XP.
  //
  // XP: use MoveFileWx with MOVEFILE_WRITE_THROUGH, which isn't atomic but
  // almost always works as intended.
  //
  // Linux: use rename(2), which is atomic.
  //
  // Mac OSX: use rename(2), but rename(2) on Mac OSX
  // is not properly implemented, atomic rename is POSIX spec though.
  // http://www.weirdnet.nl/apple/rename.html
  static bool AtomicRename(const string &from, const string &to);

  static string JoinPath(const string &path1, const string &path2);

#ifndef SWIG
  static void JoinPath(const string &path1, const string &path2,
                       string *output);
#endif

  static string Basename(const string &filename);
  static string Dirname(const string &filename);

  // return normalized path by replacing '/' with '\\' in Windows.
  // does nothing in other platforms.
  static string NormalizeDirectorySeparator(const string &path);

  // return "~/.mozc" for Unix/Mac
  // return "%USERPROFILE%\\Local Settings\\Application\\"
  //        "Google\\Google Japanese Input" for Windows XP.
  // return "%USERPROFILE%\\AppData\\LocalLow\\"
  //        "Google\\Google Japanese Input" for Windows Vista and later.
  static string GetUserProfileDirectory();

  // return ~/Library/Logs/Mozc for Mac
  // Otherwise same as GetUserProfileDirectory().
  static string GetLoggingDirectory();

  // set user dir

  // Currently we enabled this method to release-build too because
  // some tests use this.
  // TODO(mukai,taku): find better way to hide this method in the release
  // build but available from those tests.
  static void SetUserProfileDirectory(const string &path);

  // return the directory name where the mozc server exist.
  static string GetServerDirectory();

  // return the path of the mozc server.
  static string GetServerPath();

  // Returns the directory name which holds some documents bundled to
  // the installed application package.  Typically it's
  // <server directory>/documents but it can change among platforms.
  static string GetDocumentDirectory();

  // return the username.  This function's name was GetUserName.
  // Since Windows reserves GetUserName as a macro, we have changed
  // the name to GetUserNameAsString.
  static string GetUserNameAsString();

  // return Windows SID as string.
  // On Linux and Mac, GetUserSidAsString() is equivalent to
  // GetUserNameAsString()
  static string GetUserSidAsString();


  // return DesktopName as string.
  // On Windows. return <session_id>.<DesktopStationName>.<ThreadDesktopName>
  // On Linux, return getenv("DISPLAY")
  // Mac has no DesktopName() so, just return empty string
  static string GetDesktopNameAsString();

#endif  // __native_client__

#ifdef OS_WINDOWS
  // From an early stage of the development of Mozc, we have somehow abused
  // CHECK macro assuming that any failure of fundamental APIs like
  // ::SHGetFolderPathW or ::SHGetKnownFolderPathis is worth being notified
  // as a crash.  But the circumstances have been changed.  As filed as
  // b/3216603, increasing number of instances of various applications begin
  // to use their own sandbox technology, where these kind of fundamental APIs
  // are far more likely to fail with an unexpected error code.
  // EnsureVitalImmutableDataIsAvailable is a simple fail-fast mechanism to
  // this situation.  This function simply returns false instead of making
  // the process crash if any of following functions cannot work as expected.
  // - IsVistaOrLaterCache
  // - SystemDirectoryCache
  // - ProgramFilesX86Cache
  // - LocalAppDataDirectoryCache
  // TODO(taku,yukawa): Implement more robust and reliable mechanism against
  //   sandboxed environment, where such kind of fundamental APIs are far more
  //   likely to fail.  See b/3216603.
  static bool EnsureVitalImmutableDataIsAvailable();
#endif  // OS_WINDOWS

  // Command line arguments

  // Rotate the first argv value to the end of argv.
  static void CommandLineRotateArguments(int argc, char ***argv);

  // Get a pair of key and value from argv, and returns the number of
  // arguments used for the pair of key and value.  If the argv
  // contains invalid format, this function returns false and the
  // number of checked arguments.  Otherwise returns true.
  static bool CommandLineGetFlag(int argc,
                                 char **argv,
                                 string *key,
                                 string *value,
                                 int *used_args);

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
  static bool IsScriptType(const string &str, ScriptType type);

  // return true if the string contains script_type char
  static bool ContainsScriptType(const string &str, ScriptType type);

  enum FormType {
    UNKNOWN_FORM,
    HALF_WIDTH,
    FULL_WIDTH,
    FORM_TYPE_SIZE,
  };

  // return Form type of single character
  static FormType GetFormType(char32 w);

  // return FormType of string
  static FormType GetFormType(const string &str);

  // Basically, if chraset >= JIX0212, the char is platform dependent char.
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

  // Return true if the OS is supported.
  // [OS_MACOSX] This function never returns false.
  // [OS_LINUX] This function never returns false.
  // TODO(yukawa): support Mac and Linux.
  static bool IsPlatformSupported();

#ifdef OS_WINDOWS
  // return true if the version of Windows is Vista or later.
  static bool IsVistaOrLater();

  // return true if the version of Windows is 6.1 or later.
  static bool IsWindows7OrLater();

  // return true if the version of Windows is 6.2 or later.
  static bool IsWindows8OrLater();

  // return true if the version of Windows is x64 Edition.
  static bool IsWindowsX64();

  enum IsWindowsX64Mode {
    IS_WINDOWS_X64_DEFAULT_MODE,
    IS_WINDOWS_X64_EMULATE_32BIT_MACHINE,
    IS_WINDOWS_X64_EMULATE_64BIT_MACHINE,
  };

  // For unit tests, this function overrides the behavior of |IsWindowsX64|.
  static void SetIsWindowsX64ModeForTest(IsWindowsX64Mode mode);

  // return system directory. If failed, return NULL.
  // You need not to delete the returned pointer.
  // This function is thread safe.
  static const wchar_t *GetSystemDir();

  // Load a DLL which has the specified base-name and is located in the
  // system directory.
  // If the function succeeds, the return value is a handle to the module.
  // You should call FreeLibrary with the handle.
  // If the function fails, the return value is NULL.
  static HMODULE LoadSystemLibrary(const wstring &base_filename);

  // Load a DLL which has the specified base-name and is located in the
  // Mozc server directory.
  // If the function succeeds, the return value is a handle to the module.
  // You should call FreeLibrary with the handle.
  // If the function fails, the return value is NULL.
  static HMODULE LoadMozcLibrary(const wstring &base_filename);

  // If a DLL which has the specified base-name and located in the system
  // directory is loaded in the caller process, retrieve its module handle.
  // If the function succeeds, the return value is a handle to the module
  // without incrementing its reference count so that you should not call
  // FreeLibrary with the handle.
  // If the function fails, the return value is NULL.
  static HMODULE GetSystemModuleHandle(const wstring &base_filename);

  // A variant ot GetSystemModuleHandle except that this method increments
  // reference count of the target DLL.
  static HMODULE GetSystemModuleHandleAndIncrementRefCount(
      const wstring &base_filename);

  // Retrieves version of the specified file.
  // If the function fails, returns false.
  static bool GetFileVersion(const wstring &file_fullpath,
                             int *major, int *minor, int *build, int *revision);

  // Retrieves version string of the specified file.
  // The version string consists of 4 digits separated by comma
  // like "X.YY.ZZZ.WWWW".
  // If the function fails, the return value is an empty string.
  static string GetFileVersionString(const wstring &file_fullpath);

#endif

  // return string representing os version
  // TODO(toshiyuki): Add unittests.
  static string GetOSVersionString();

  // disable IME in the current process/thread
  static void DisableIME();

  // retrieve total physical memory. returns 0 if any error occurs.
  static uint64 GetTotalPhysicalMemory();

  // read specified memory-mapped region to cause page fault.
  // this function does not consider memory alignment.
  // if |*query_quit| is or becomes true, it returns immediately.
  static void PreloadMappedRegion(const void *begin,
                                  size_t region_size_in_byte,
                                  volatile bool *query_quit);

  // check endian-ness at runtime.
  static bool IsLittleEndian();

  // Following mlock/munlock related functions work based on target environment.
  // In the case of Android, Native Client, Windows, we don't want to call
  // actual functions, so these functions do nothing and return -1. In other
  // cases, these functions call actual mlock/munlock functions and return it's
  // result.
  // On Android, page-out is probably acceptable because
  // - Smaller RAM on the device.
  // - The storage is (usually) solid state thus page-in/out is expected to
  //   be faster.
  // On Linux, in the kernel version >= 2.6.9, user process can mlock. In older
  // kernel, it fails if the process is running in user priviledge.
  // TODO(horo): Check in mac that mlock is really necessary.
  static int MaybeMLock(const void *addr, size_t len);

  static int MaybeMUnlock(const void *addr, size_t len);

  // should never be allocated.
 private:
  Util() {}
  virtual ~Util() {}
};

// Const iterator implementation to traverse on a (utf8) string as a char32
// string.
//
// Example usage:
//   string utf8_str;
//   for (ConstChar32Iterator iter(utf8_str); !iter.Done(); iter.Next()) {
//     char32 c = iter.Get();
//     ...
//   }
class ConstChar32Iterator {
 public:
  explicit ConstChar32Iterator(const string &utf8_string) {
    ptr_ = utf8_string.data();
    end_ = utf8_string.data() + utf8_string.size();
    current_ = Util::UTF8ToUCS4(ptr_, end_, &current_char_size_);
  }

  ConstChar32Iterator(const char *utf8_ptr, size_t size) {
    ptr_ = utf8_ptr;
    end_ = utf8_ptr + size;
    current_ = Util::UTF8ToUCS4(ptr_, end_, &current_char_size_);
  }

  char32 Get() const {
    DCHECK(ptr_ < end_);
    return current_;
  }

  void Next() {
    DCHECK(ptr_ < end_);
    ptr_ += current_char_size_;
    current_ = Util::UTF8ToUCS4(ptr_, end_, &current_char_size_);
  }

  bool Done() const {
    return ptr_ == end_;
  }

 private:
  const char *ptr_;
  const char *end_;
  char32 current_;
  size_t current_char_size_;

  DISALLOW_COPY_AND_ASSIGN(ConstChar32Iterator);
};

}  // namespace mozc

#endif  // MOZC_BASE_UTIL_H_
