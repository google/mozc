# AUX dictionary

AUX dictionary is a mechanism to add word entries to data/dictionary_oss/dictionary0x.txt
without running the internal training pipeline.

Pull requests under this directory are acceptable.

## Format of aux dictionary

| key      | value    | base_key | base_value | cost_offset |
| :------: | :------: | :------: | :--------: |:-----------:|
| あるぱか | アルパカ | かぴばら | カピバラ | -1          |

* aux_dictionary.tsv is a list of additional words to be added to Mozc's word dictionary.
* key and value (i.e. アルパカ) are fields for the new word.
* base_key and base_value (i.e. カピバラ) are fields of the reference word already in the dictionary.
* cost_offset is the difference from the base cost. (i.e. -1 makes the new word prioritized than the base word).
* other fields of the new word (e.g. lid, rid , cost) are copied from the reference word.