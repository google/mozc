About Branding
==============

# Differences between Google Japanese Input and Mozc

| Product Name | Google Japanese Input Stable Version                                 | Google Japanese Input Development Version                                 | Mozc |
|:-------------|:---------------------------------------------------------------------|:--------------------------------------------------------------------------|:-----|
| Branding     | Google Japanese Input (Google 日本語入力)                            | Google Japanese Input (Google 日本語入力)                                 | Mozc |
| Release Form | Binary Installer                                                     | Binary Installer                                                          | Source code |
| License Agreement | Available on the installation page                              | Available on the installation page                                        | 3-Clause BSD License (except for third-party code) |
| Target Platform | Microsoft Windows Apple OS X Android Google Chrome OS             | Microsoft Windows, Apple OS X                                             | Android OS, Microsoft Windows, Apple OS X, GNU/Linux, Chromium OS |
| Version String | GoogleJapaneseInput-1.x.yyyy.0 (Windows), GoogleJapaneseInput-1.x.yyyy.1 (OS X), GoogleJapaneseInput-1.x.yyyy.3 (Android), GoogleJapaneseInput-1.x.yyyy.4 (Chrome OS) | GoogleJapaneseInput-1.x.yyyy.100 (Windows), GoogleJapaneseInput-1.x.yyyy.101 (OS X) | Mozc-1.x.yyyy.102 |
| Logo         | Colorful                                                             | Colorful                                                                  | Orange |
| Code Location | Google internal                                                     | Google internal                                                           | [GitHub](https://github.com/google/mozc) |
| System Dictionary | Google internal                                                 | Google internal                                                           | [Mozc dictionary](../src/data/dictionary_oss/README.txt) |
| Usage Dictionary | [Japanese Usage Dictionary](https://github.com/hiroyuki-komatsu/japanese-usage-dictionary) | [Japanese Usage Dictionary](https://github.com/hiroyuki-komatsu/japanese-usage-dictionary) | [Japanese Usage Dictionary](https://github.com/hiroyuki-komatsu/japanese-usage-dictionary) |
| Collocation Data | Google internal                                                  | Google internal                                                           | [Only examples are available](../src/data/dictionary_oss/collocation.txt) |
| Reading Correction Data | Google internal                                           | Google internal                                                           | [Only an example is available](../src/data/dictionary_oss/reading_correction.tsv) |
| Suggestion Filter | Google internal                                                 | Google internal                                                           | [Only an example is available](../src/data/dictionary_oss/suggestion_filter.txt) |
| Japanese ZIP Code Dictionary | Included                                             | Included                                                                  | Not included. [You can add it by yourself](../src/data/dictionary_oss/README.txt) |
| Quality Assurance | New releases are tested by Google before sending to users       | Basically the same to Stable but some additional QA tests are skipped before sending to users | No official QA by Google |
| Crash Reporting | Chrome OS: Integrated into the OS settings Other platforms: No by default. You can turn it on | Yes                                           | No   |
| User Metrics Reporting | Chrome OS: No  Other platforms: No by default. You can turn it on    | Yes                                                             | No   |
| Google Update | Android : No Other platforms: Yes                                   | Yes                                                                       | No   |
| Other Online Features | Not supported yet                                           | Cloud Handwriting                                                         | Nothing |
