# -*- coding: utf-8 -*-
# Copyright 2010-2014, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Generates partial supported languages messages.

  % python gen_partial_supported_messages.py --outdir=/path/to/outdir \
    --version_file=version.txt
"""

__author__ = "horo"

import json
import logging
import optparse
import os

from build_tools import mozc_version


_JP_KEYBOARD_TRANSLATIONS = {
    'am': 'Google ጃፓንኛ ግቤት (ለጃፓንኛ ቁልፍ ሰሌዳ)',
    'ar': 'أسلوب Google للإدخال الياباني (للوحة المفاتيح اليابانية)',
    'bg': 'Google Редактор с метод за въвеждане на японски (за японска '
    'клавиатура)',
    'bn': 'Google জাপানিয় ইনপুট (জাপানি কীবোর্ডের জন্য)',
    'ca': 'Mètode d\'entrada japonès de Google (per a teclat japonès)',
    'cs': 'Zadávání japonštiny Google (pro japonskou klávesnici)',
    'da': 'Googles japanske indtastning (til japansk tastatur)',
    'de': 'Google Japanese Input (für japanische Tastatur)',
    'el': 'Εισαγωγή Ιαπωνικών Google (για πληκτρολόγιο Ιαπωνικών)',
    'en_GB': 'Google Japanese Input (for Japanese keyboard)',
    'es_419': 'Entrada en japonés de Google (para teclado japonés)',
    'es': 'Entrada de japonés de Google (para teclado japonés)',
    'et': 'Google\'i jaapanikeelne sisestus (jaapani klaviatuuri jaoks)',
    'fa': 'ورودی ژاپنی Google (برای صفحه‌کلید ژاپنی)',
    'fil': 'Pag-input na Japanese ng Google (para sa Japanese na keyboard)',
    'fi': 'Googlen japanilainen syöttötapa (japanilaiselle näppäimistölle)',
    'fr': 'Mode de saisie Google du japonais (pour clavier japonais)',
    'gu': 'Google જાપાનીઝ ઇનપુટ (જાપાનીઝ કીબોર્ડ માટે)',
    'hi': 'Google जापानी इनपुट (जापानी कीबोर्ड के लिए)',
    'hr': 'Google japanski unos (za japanske tipkovnice)',
    'hu': 'Google japán beviteli mód (japán billentyűzetre)',
    'id': 'Masukan Jepang Google (untuk keyboard Jepang)',
    'it': 'Metodo di immissione giapponese Google (per tastiera giapponese)',
    'iw': 'קלט יפני של Google (למקלדת יפנית)',
    'kn': 'Google ಜಪಾನೀಸ್ ಇನ್‌ಪುಟ್ (ಜಪಾನೀಸ್ ಕೀಬೋರ್ಡ್‌ಗಾಗಿ)',
    'ko': 'Google 일본어 입력(일본어 키보드용)',
    'lt': '„Google“ įvestis japonų k. (skirta japonų k. simbolių klaviatūrai'
    ')',
    'lv': 'Google japāņu valodas ievade (japāņu valodas tastatūrai)',
    'ml': 'Google ജാപ്പനീസ് ഇന്‍‌പുട്ട് (ജാപ്പനീസ് കീബോര്‍‌ഡിന് മാത്രം)',
    'mr': 'Google जपानी इनपुट (जपानी कीबोर्डसाठी)',
    'ms': 'Input Jepun Google (untuk papan kekunci Jepun)',
    'nl': 'Japanse invoer van Google (voor Japans toetsenbord)',
    'no': 'Google-inndatametode for japansk (for japansk tastatur)',
    'pl': 'japoński – Google (dla klawiatury japońskiej)',
    'pt_BR': 'Entrada japonesa do Google (para teclado japonês)',
    'pt_PT': 'Introdução japonesa do Google (para teclados japoneses)',
    'ro': 'Metoda de introducere Google pentru japoneză (la tastatura pentru '
    'japoneză)',
    'ru': 'японская раскладка Google (для японской клавиатуры)',
    'sk': 'Japonská metóda vstupu Google (pre japonskú klávesnicu)',
    'sl': 'Googlov japonski način vnosa (za japonsko tipkovnico)',
    'sr': 'Google јапански метод уноса (за јапанску тастатуру)',
    'sv': 'Googles japanska inmatning (för japanskt tangentbord)',
    'sw': 'Ingizo la Kijapani la Google (kwa kibodi ya Kijapani)',
    'ta': 'Google ஜப்பானிய உள்ளீடு (ஜப்பானிய விசைப்பலகைக்காக)',
    'te': 'Google జపనీయుల ఇన్‌పుట్ (జపనీయుల కీబోర్డ్ కోసం)',
    'th': 'การป้อนข้อมูลภาษาญี่ปุ่นของ Google (สำหรับแป้นพิมพ์ภาษาญี่ปุ่น)',
    'tr': 'Google Japonca Girişi (Japonca klavye için)',
    'uk': 'Метод введення Google для японської мови (японська клавіатура)',
    'vi': 'Phương pháp Nhập tiếng Nhật của Google (dành cho bàn phím Nhật)',
    'zh_CN': 'Google 日语输入法（日语键盘）',
    'zh_TW': 'Google 日文輸入法 (適用於日文鍵盤)',
}

_US_KEYBOARD_TRANSLATIONS = {
    'am': 'Google ጃፓንኛ ግቤት (ለአሜሪካ ቁልፍ ሰሌዳ)',
    'ar': 'أسلوب الإدخال الياباني لـ Google'
    '(للوحة المفاتيح بالولايات المتحدة)',
    'bg': 'Google Редактор с метод за въвеждане на японски (за американска '
    'клавиатура)',
    'bn': 'Google জাপানিয় ইনপুট (ইউএস কীবোর্ডের জন্য)',
    'ca': 'Mètode d\'entrada japonès de Google (per a teclat nord-americà)',
    'cs': 'Zadávání japonštiny Google (pro americkou klávesnici)',
    'da': 'Googles japanske indtastning (til amerikansk tastatur)',
    'de': 'Google Japanese Input (für US-Tastatur)',
    'el': 'Εισαγωγή Ιαπωνικών Google (για πληκτρολόγιο ΗΠΑ)',
    'en_GB': 'Google Japanese Input (for US keyboard)',
    'es_419': 'Entrada en japonés de Google (para teclado estadounidense)',
    'es': 'Entrada de japonés de Google (para teclado estadounidense)',
    'et': 'Google\'i jaapanikeelne sisestus (USA klaviatuuri jaoks)',
    'fa': 'ورودی ژاپنی Google (برای صفحه‌کلید آمریکایی)',
    'fil': 'Pag-input na Japanese ng Google (para sa US na keyboard)',
    'fi': 'Googlen japanilainen syöttötapa (yhdysvaltalaiselle '
    'näppäimistölle)',
    'fr': 'Mode de saisie Google du japonais (pour clavier américain)',
    'gu': 'Google જાપાનીઝ ઇનપુટ (યુએસ કીબોર્ડ માટે)',
    'hi': 'Google जापानी इनपुट (यूएस कीबोर्ड के लिए)',
    'hr': 'Google japanski unos (za američke tipkovnice)',
    'hu': 'Google japán beviteli mód (US billentyűzetre)',
    'id': 'Masukan Jepang Google (untuk keyboard AS)',
    'it': 'Metodo di immissione giapponese Google (per tastiera USA)',
    'iw': 'קלט יפני של Google (למקלדת אמריקאית)',
    'kn': 'Google ಜಪಾನೀಸ್ ಇನ್‍‌ಪುಟ್ (ಯುಎಸ್ ಕೀಬೋರ್ಡ್‌ಗಾಗಿ)',
    'ko': 'Google 일본어 입력(미국 키보드용)',
    'lt': '„Google“ įvestis japonų k. (skirta JAV klaviatūrai)',
    'lv': 'Google japāņu valodas ievade (ASV tastatūrai)',
    'ml': 'Google ജാപ്പനീസ് ഇന്‍‌പുട്ട് (യു‌എസ് കീബോര്‍‌ഡിന് മാത്രം)',
    'mr': 'Google जपानी इनपुट (यूएस कीबोर्डसाठी)',
    'ms': 'Input Jepun Google (untuk papan kekunci AS)',
    'nl': 'Japanse invoer van Google (voor Amerikaans toetsenbord)',
    'no': 'Google-inndatametode for japansk (for amerikansk tastatur)',
    'pl': 'japoński – Google (dla klawiatury amerykańskiej)',
    'pt_BR': 'Entrada do Google em japonês (para teclado dos EUA)',
    'pt_PT': 'Introdução japonesa do Google (para teclado dos EUA)',
    'ro': 'Metoda de introducere Google pentru japoneză (la tastatura '
    'americană)',
    'ru': 'японская раскладка Google (для клавиатуры США)',
    'sk': 'Japonská metóda vstupu Google (pre americkú klávesnicu)',
    'sl': 'Googlov japonski način vnosa (za ameriško tipkovnico)',
    'sr': 'Google јапански метод уноса (за америчку тастатуру)',
    'sv': 'Googles japanska inmatning (för amerikanskt tangentbord)',
    'sw': 'Ingizo la Kijapani la Google (kwa kibodi ya Marekani)',
    'ta': 'Google ஜப்பானிய உள்ளீடு (யுஎஸ் விசைப்பலகைக்காக)',
    'te': 'Google జపనీయుల ఇన్‌పుట్ (యుఎస్ కీబోర్డ్ కోసం)',
    'th': 'การป้อนข้อมูลภาษาญี่ปุ่นของ Google (สำหรับแป้นพิมพ์สหรัฐฯ)',
    'tr': 'Google Japonca Girişi (ABD klavye için)',
    'uk': 'Метод введення Google для японської мови (клавіатура США)',
    'vi': 'Phương pháp Nhập tiếng Nhật của Google (dành cho bàn phím Hoa Kỳ)',
    'zh_CN': 'Google 日语输入法（美式键盘）',
    'zh_TW': 'Google 日文輸入法 (適用於美式鍵盤)',
}

_APP_NAME_TRANSLATIONS = {
    'am': 'Google ጃፓንኛ ግቤት',
    'ar': 'أسلوب الإدخال الياباني لـ Google',
    'bg': 'Google Редактор с метод за въвеждане на японски',
    'bn': 'Google জাপানিয় ইনপুট',
    'ca': 'Mètode d\'entrada japonès de Google',
    'cs': 'Zadávání japonštiny Google',
    'da': 'Googles japanske indtastning',
    'de': 'Google Japanese Input',
    'el': 'Εισαγωγή Ιαπωνικών Google',
    'en_GB': 'Google Japanese Input',
    'es_419': 'Entrada en japonés de Google',
    'es': 'Entrada de japonés de Google',
    'et': 'Google\'i jaapanikeelne sisestus',
    'fa': 'ورودی ژاپنی Google',
    'fil': 'Pag-input na Japanese ng Google',
    'fi': 'Googlen japanilainen syöttötapa',
    'fr': 'Mode de saisie Google du japonais',
    'gu': 'Google જાપાનીઝ ઇનપુટ',
    'hi': 'Google जापानी इनपुट',
    'hr': 'Google japanski unos',
    'hu': 'Google japán beviteli mód',
    'id': 'Masukan Jepang Google',
    'it': 'Metodo di immissione giapponese Google',
    'iw': 'קלט יפני של Google',
    'kn': 'Google ಜಪಾನೀಸ್ ಇನ್‍‌ಪುಟ್',
    'ko': 'Google 일본어 입력',
    'lt': '„Google“ įvestis japonų k.',
    'lv': 'Google japāņu valodas ievade',
    'ml': 'Google ജാപ്പനീസ് ഇന്‍‌പുട്ട് ',
    'mr': 'Google जपानी इनपुट (यूएस कीबोर्डसाठी)',
    'ms': 'Input Jepun Google',
    'nl': 'Japanse invoer van Google',
    'no': 'Google-inndatametode for japansk',
    'pl': 'japoński – Google',
    'pt_BR': 'Entrada do Google em japonês',
    'pt_PT': 'Introdução japonesa do Google',
    'ro': 'Metoda de introducere Google pentru japoneză',
    'ru': 'японская раскладка Google',
    'sk': 'Japonská metóda vstupu Google',
    'sl': 'Googlov japonski način vnosa',
    'sr': 'Google јапански метод уноса',
    'sv': 'Googles japanska inmatning',
    'sw': 'Ingizo la Kijapani la Google',
    'ta': 'Google ஜப்பானிய உள்ளீடு',
    'te': 'Google జపనీయుల ఇన్‌పుట్',
    'th': 'การป้อนข้อมูลภาษาญี่ปุ่นของ Google',
    'tr': 'Google Japonca Girişi',
    'uk': 'Метод введення Google для японської мови',
    'vi': 'Phương pháp Nhập tiếng Nhật của Google',
    'zh_CN': 'Google 日语输入法',
    'zh_TW': 'Google 日文輸入法',
}

_APP_NAME_DEV = 'Google Japanese Input Dev Channel'

_SUPPORTED_LOCALES = ['am', 'ar', 'bg', 'bn', 'ca', 'cs', 'da', 'de', 'el',
                      'en_GB', 'es_419', 'es', 'et', 'fa', 'fil', 'fi', 'fr',
                      'gu', 'hi', 'hr', 'hu', 'id', 'it', 'iw', 'kn', 'ko',
                      'lt', 'lv', 'ml', 'mr', 'ms', 'nl', 'no', 'pl', 'pt_BR',
                      'pt_PT', 'ro', 'ru', 'sk', 'sl', 'sr', 'sv', 'sw', 'ta',
                      'te', 'th', 'tr', 'uk', 'vi', 'zh_CN', 'zh_TW']


def ParseOptions():
  """Parse command line options.

  Returns:
    An options data.
  """
  parser = optparse.OptionParser()
  parser.add_option('--version_file', dest='version_file')
  parser.add_option('--outdir', dest='outdir')

  (options, _) = parser.parse_args()
  return options


def GetTranslationMessages(version, locale):
  """Returns the translation messages."""
  messages = {}
  # Dev channel is not supported.
  if not version.IsDevChannel():
    messages['appName'] = {}
    messages['appName']['message'] = _APP_NAME_TRANSLATIONS[locale]
    messages['appNameJPKeyboard'] = {}
    messages['appNameJPKeyboard']['message'] = _JP_KEYBOARD_TRANSLATIONS[locale]
    messages['appNameUSKeyboard'] = {}
    messages['appNameUSKeyboard']['message'] = _US_KEYBOARD_TRANSLATIONS[locale]
  else:
    messages['appName'] = {}
    messages['appName']['message'] = _APP_NAME_DEV
  return messages


def main():
  """The main function."""
  options = ParseOptions()
  if options.version_file is None:
    logging.error('--version_file is not specified.')
    exit(-1)
  if options.outdir is None:
    logging.error('--outdir is not specified.')
    exit(-1)

  version = mozc_version.MozcVersion(options.version_file)
  for locale in _SUPPORTED_LOCALES:
    dest_path = os.path.join(options.outdir, locale)
    if not os.path.exists(dest_path):
      os.mkdir(dest_path)
    with open(os.path.join(dest_path, 'messages.json'), 'w') as f:
      messages = GetTranslationMessages(version, locale)
      output = json.dumps(messages, indent=2, separators=(',', ': '))
      f.write(output)


if __name__ == '__main__':
  main()
