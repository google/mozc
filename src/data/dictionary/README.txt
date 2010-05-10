Open source mozc dictionary is different from the dictionary
used for Google Japanese Input. The differences are as follows:

- Large vocabulary set generated from the Web corpus is not included.
- The vocabulary set is basically the same as that of IPA-dic.
- Google manually added some extra adjective/verbs which are not in IPA-dic.
- It contains many Katakana words, which are collected as unknown words
  during the text processing over the Web data.
- To improve the conversion quality of Mozc, several compound words
  (like 社員証, 再起動) are added. We basically collected these compounds
  with a set of POS rules.
- Open source version doesn't include Katakana transliterations.
  (あんどろいど->Android)
- Open source version doesn't include Japanese postal code dictionary .
