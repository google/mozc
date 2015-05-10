Surrounding Text Support in Mozc
================================

Objective
---------

Utilize surrounding text information to achieve more efficient and intelligent text input experience.

Design Highlights
-----------------

### Temporary history invalidation

Mozc converter internally maintains history segments mainly for users who input Japanese sentence with segments in fragments segments. Imagine that a user input an example sentence “今日は良い天気です” as 3 segments as follows.

  1. kyouha (今日は) -> convert -> commit
  1. yoi (良い) -> convert -> commit
  1. tennkidesu (天気です) -> convert -> commit

At the step 3, Mozc converter takes the result of 1 and 2 into consideration when "tennkidesu" is converted. However, this approach may not work well when the caret position is moved but the Mozc converter cannot notice it. In order to work around this situation, Mozc converter can read the preceding text and check if the internal history information is consistent with the preceding text. If they are inconsistent, history segments should be invalidated.

### History reconstruction

In order to improve the conversion quality when preceding text and history segment are mismatched, it would be nice if we can reconstruct (or emulate) history segments from the preceding text.

In this project, reconstruct segments that consists of only number or alphabet as a first step. Reconstructing more variety of tokens will be future work.

Following table describe the mappings from a preceding text to key/value and POS (Part-of-speech) ID.

| Preceding Text     | Key     | Value     | POS     |
|:-------------------|:--------|:----------|:--------|
| "10"               | "10"    | "10"      | Number  |
| "10 "              | "10"    | "10"      | Number  |
| "1 10 "            | "10"    | "10"      | Number  |
| "C60"              | "60"    | "60"      | Number  |
| "abc"              | "abc"   | "abc"     | UniqueNoun |
| "this is"          | "is"    | "is"      | UniqueNoun |
| "あ"              | N/A     | N/A       | N/A     |

Scope
-----

Here is the list of typical cases when preceding text and history segment are mismatched.

  * Multiple users are writing the same document. (e.g. Google Document)
  * A user prefers to turn IME off when he/she input alphanumeric characters. e.g. He/she inputs "今日は Andy に会う" as following steps:
    1. Turn IME on
    1. Type "kyouha" then convert it to "今日は"
    1. Turn IME off
    1. Type " Andy "
    1. Turn IME on
    1. Type "niau" then convert it to "に会う"
  * Caret position is moved by mouse.

Surrounding text has been available in the following OSes and frameworks:
  * Windows OS
    * Microsoft Internet Explorer
    * Google Chrome 17+
    * Mozilla Firefox
    * Microsoft Office
    * Windows Presentation Foundation (WPF)
  * Apple OS X
  * Android OS
  * Chromium OS

Here is the list of other possible usages of surrounding text in future projects.

  * Language detection.
  * Character width (narrow/wide) adjustment.
  * Personal name recognition (e.g., SNS screen names)

Risk
----

Some buggy applications that wrongly handle surrounding text event may become unstable. Basically there should be no privacy risk because applications are expected to hide sensitive text such as password from IME.

Production Impact
-----------------

Available on Windows, Apple OS X, Chromium OS and Linux desktop.  No impact for Android platform.

Release History
---------------

  * Initial release: 1.11.1490.10x

Reference
---------

  * [chrome.input.ime](http://developer.chrome.com/extensions/input.ime.html)
