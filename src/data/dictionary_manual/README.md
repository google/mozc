# data/dictionary_manual

This directory contains word entries to be added to the main dictionary.

The data here are used for proactive fixes before the main dictionary is
updated.

## TSV files (e.g. places.tsv, words.tsv)

Entries are added to the main dictionary with the following adjustments:

*   The POS (e.g., 名詞) is converted to a POS ID (e.g., 1843).
*   The cost is set to the median cost of all words sharing the same POS.

These adjustments are performed by
[dictionary/gen_aux_dictionary.py](https://github.com/google/mozc/blob/master/src/dictionary/gen_aux_dictionary.py).

If the same entries already exist in the main dictionary, the entries in this
directory are ignored. For more control, you may want to use
`aux_dictionary.tsv` and `dictionary_filter.tsv`.

*   https://github.com/google/mozc/blob/master/src/data/dictionary_oss/aux_dictionary.tsv
*   https://github.com/google/mozc/blob/master/src/data/dictionary_oss/dictionary_filter.tsv

## domain.txt

This file uses the same format as the main dictionary and is used as part of it.

We recommend using the TSV files instead, as the POS IDs and cost values
typically change with each dictionary update.
