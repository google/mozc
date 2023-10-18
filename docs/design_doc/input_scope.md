Input Scope Support in Mozc
===========================

Objective
---------

Make Windows Mozc client aware of InputScope feature on Windows.

Starting with Windows 8, InputScope is the recommended way to set the conversion mode of Japanese IME and ImmSetConversionStatus API is virtually deprecated in per user input mode.

> Thus, Japanese Microsoft IME ignores the change of conversion modes made by calling
> ImmSetConversionStatus when used in per user mode. This API is used in the IME mode
> property of .NET framework. InputScope is recommended in order to set the IME mode
> under per user mode in Windows 8. - "[Switch text input changed from per-thread to per-user](http://msdn.microsoft.com/en-us/library/windows/desktop/hh994466.aspx)"

Microsoft IME shipped with Windows Vista and later actually behaves like this. As for Mozc, Mozc client’s should behave like MS-IME shipped with Windows 11 22H2 in terms of open/close mode and conversion mode for each InputScope.
This is the first step to support such context-awareness. As future work, Mozc should be able to behave more appropriately and adoptively based on each input context.

Design Highlights
-----------------

When an input field is focused and it has InputScope, Mozc should change its on/off mode and conversion mode as follows to be compatible with MS-IME shipped with Windows 11 22H2.

| InputScope | Expected Input Mode |
|:-----------|:--------------------|
| `IS_URL`, `IS_EMAIL_USERNAME`, `IS_EMAIL_SMTPEMAILADDRESS`, `IS_DIGITS`, `IS_NUMBER`, `IS_PASSWORD`, `IS_TELEPHONE_FULLTELEPHONENUMBER`, `IS_TELEPHONE_COUNTRYCODE`, `IS_TELEPHONE_AREACODE`, `IS_TELEPHONE_LOCALNUMBER`, `IS_TIME_FULLTIME`, `IS_TIME_HOUR`, `IS_TIME_MINORSEC`, `IS_ALPHANUMERIC_HALFWIDTH` | Direct Mode (IME Off) |
| `IS_HIRAGANA` | Hiragana Mode (IME On) |
| `IS_NUMBER_FULLWIDTH`, `IS_ALPHANUMERIC_FULLWIDTH` | Fullwidth Alphanumeric Mode (IME On) |
| `IS_KATAKANA_HALFWIDTH` | Halfwidth Katakana Mode (IME On) |
| `IS_KATAKANA_FULLWIDTH` | Fullwidth Katakana Mode (IME On) |

From usability perspectives, user experience offered by InputScope should be consistent with that in password field. It means that the change caused by InputScope should be temporal and volatile. This requires two different mode sets 1) mode visible from Mozc client and 2) mode visible from TSF should be managed.

Here is an example of mode transitions.

| Action | Mozc On/Off | Mozc Mode | TSF On/Off | TSF Mode |
|:-----------|:----------------|:--------------|:---------------|:-------------|
| Turn IME on | On              | Hiragana      | On             | Hiragana     |
| ↓        |                 |               |                |              |
| Enter email field | Off             | Hiragana      | On             | Hiragana     |
| ↓        |                 |               |                |              |
| Enter search field | On              | Hiragana      | On             | Hiragana     |
| ↓        |                 |               |                |              |
| Turn IME off | Off             | Hiragana      | Off            | Hiragana     |
| ↓        |                 |               |                |              |
| Enter Halfwidth Katakan field | On              | Halfwidth Katakana | Off            | Hiragana     |

When on/off mode and/or conversion mode that are visible from Mozc is changed by InputScope, Mode Indicator will be popped up near the caret location so that a user can immediately get to know the new input mode.

Scope
-----

InputScope has been supported in the following frameworks and environment:

  * Windows Presentation Foundation (WPF)
  * Microsoft Internet Explorer 10 on Windows 8+
  * Chromium 26+ on Windows Vista and later
  * Firefox 23+ on Windows Vista and later

Here is the mapping table from HTML5 input types to InputScope on Windows.

| HTML5 Forms Input Type | Internet Explorer 10 | Chromium 26 | Firefox 23 |
|:-----------------------|:---------------------|:------------|:-----------|
| text                   | `IS_DEFAULT`         | `IS_DEFAULT` | `IS_DEFAULT` |
| number                 | `IS_NUMBER`          | `IS_NUMBER` | `IS_NUMBER` |
| email                  | `IS_EMAIL_SMTPEMAILADDRESS` | `IS_EMAIL_SMTPEMAILADDRESS` | `IS_EMAIL_SMTPEMAILADDRESS` |
| password               | `IS_PASSWORD`        | `IS_PASSWORD` | `IS_PASSWORD` |
| color                  | `IS_DEFAULT`         | `IS_DEFAULT` | `IS_DEFAULT` |
| date                   | `IS_DEFAULT`         | `IS_DEFAULT` | `IS_DATE_FULLDATE` |
| datetime               | `IS_DEFAULT`         | `IS_DEFAULT` | `IS_DATE_FULLDATE` |
| datetime-local         | `IS_DEFAULT`         | `IS_DEFAULT` | `IS_DATE_FULLDATE` |
| month                  | `IS_DEFAULT`         | `IS_DEFAULT` | `IS_DATE_FULLDATE` |
| search                 | `IS_SEARCH`          | `IS_SEARCH`  | `IS_SEARCH` |
| tel                    | `IS_TELEPHONE_FULLTELEPHONENUMBER` | `IS_TELEPHONE_FULLTELEPHONENUMBER` | `IS_TELEPHONE_FULLTELEPHONENUMBER`,  `IS_TELEPHONE_LOCALNUMBER` |
| time                   | `IS_DEFAULT`         | `IS_DEFAULT` | `IS_TIME_FULLTIME` |
| url                    | `IS_URL`             | `IS_URL`    | `IS_URL`    |
| week                   | `IS_DEFAULT`         | `IS_DEFAULT` | `IS_DATE_FULLDATE` |
| range                  | `IS_DEFAULT`         | `IS_DEFAULT` | `IS_DEFAULT` |
| range                  | `IS_DEFAULT`         | `IS_DEFAULT` | `IS_DEFAULT` |

Chromium OS might want to follow this mapping in future if the feedback to this project is positive. To achieve this, we need to add Chromium version of InputScope into [chrome.input.ime](http://developer.chrome.com/extensions/input.ime.html) API.

Risk
----

Some user may dislike this kind of automatic mode changing and want a configuration option to disable it, while it is not configurable yet.

Production Impact
-----------------

Available only for users who are using Mozc in TSF Mode. This means that only Windows 8+ user will be able to use this feature with Mozc. No impact to all other platforms.

Release History
---------------

  * 1.11.1490.10x: Initial release
  * 2.29.5250.10x
    * [#818](https://github.com/google/mozc/issues/818): Remapped `IS_ALPHANUMERIC_HALFWIDTH` to direct mode
    * [#826](https://github.com/google/mozc/issues/826): Reflect `InputFocus` on every focus change.

Reference
---------

  * [Input Scopes - TSF Aware](http://blogs.msdn.com/tsfaware/archive/2007/07/10/input-scopes.aspx)
  * [Improving Recognition Results - Improving Recognition Results](http://msdn.microsoft.com/en-us/library/ms698133.aspx)
  * [Input modalities - 4.10.19 Attributes common to form controls - HTML Standard - WhatWG](http://www.whatwg.org/specs/web-apps/current-work/multipage/association-of-controls-and-forms.html#input-modalities:-the-inputmode-attribute)

Full list of InputScope (as of Windows 8)

```
IS_DEFAULT = 0
IS_URL = 1
IS_FILE_FULLFILEPATH = 2
IS_FILE_FILENAME = 3
IS_EMAIL_USERNAME = 4
IS_EMAIL_SMTPEMAILADDRESS = 5
IS_LOGINNAME = 6
IS_PERSONALNAME_FULLNAME = 7
IS_PERSONALNAME_PREFIX = 8
IS_PERSONALNAME_GIVENNAME = 9
IS_PERSONALNAME_MIDDLENAME = 10
IS_PERSONALNAME_SURNAME = 11
IS_PERSONALNAME_SUFFIX = 12
IS_ADDRESS_FULLPOSTALADDRESS = 13
IS_ADDRESS_POSTALCODE = 14
IS_ADDRESS_STREET = 15
IS_ADDRESS_STATEORPROVINCE = 16
IS_ADDRESS_CITY = 17
IS_ADDRESS_COUNTRYNAME = 18
IS_ADDRESS_COUNTRYSHORTNAME = 19
IS_CURRENCY_AMOUNTANDSYMBOL = 20
IS_CURRENCY_AMOUNT = 21
IS_DATE_FULLDATE = 22
IS_DATE_MONTH = 23
IS_DATE_DAY = 24
IS_DATE_YEAR = 25
IS_DATE_MONTHNAME = 26
IS_DATE_DAYNAME = 27
IS_DIGITS = 28
IS_NUMBER = 29
IS_ONECHAR = 30
IS_PASSWORD = 31
IS_TELEPHONE_FULLTELEPHONENUMBER = 32
IS_TELEPHONE_COUNTRYCODE = 33
IS_TELEPHONE_AREACODE = 34
IS_TELEPHONE_LOCALNUMBER = 35
IS_TIME_FULLTIME = 36
IS_TIME_HOUR = 37
IS_TIME_MINORSEC = 38
IS_NUMBER_FULLWIDTH = 39
IS_ALPHANUMERIC_HALFWIDTH = 40
IS_ALPHANUMERIC_FULLWIDTH = 41
IS_CURRENCY_CHINESE = 42
IS_BOPOMOFO = 43
IS_HIRAGANA = 44
IS_KATAKANA_HALFWIDTH = 45
IS_KATAKANA_FULLWIDTH = 46
IS_HANJA = 47
IS_HANGUL_HALFWIDTH = 48
IS_HANGUL_FULLWIDTH = 49
IS_SEARCH = 50
IS_FORMULA = 51
IS_SEARCH_INCREMENTAL = 52
IS_CHINESE_HALFWIDTH = 53
IS_CHINESE_FULLWIDTH = 54
IS_NATIVE_SCRIPT = 55
```

See [InputScope enumeration](http://msdn.microsoft.com/en-us/library/windows/desktop/ms538181.aspx) for details.
