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
  (いんたーねっと->Internet)
- Open source version doesn't include Japanese postal code dictionary .

You can add zip code dictionary by follows:
1. Download zip code data from http://www.post.japanpost.jp/zipcode/download.html
2. Extract them
3. Update zip_code_seed.tsv by
  ../../dictionary/gen_zip_code_seed.py \
   --zip_code=KEN_ALL.CSV --jigyosyo=JIGYOSYO.CSV > ./zip_code_seed.tsv
