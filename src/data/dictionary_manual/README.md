# data/dictionary_manual

This directory contains word entries to be added to the main word dictionary.

## TSV files (e.g. places.tsv, words.tsv)

Entries are added to the main dictionary with the following adjustments.

*   The POS (e.g. 名詞) are converted to the POS-ID (e.g. 1843).
*   THe cost is assigned to the medium cost of whole the same POS words.

Those adjustments are processed by
[dictionary/gen_aux_dictionary.py](https://github.com/google/mozc/blob/master/src/dictionary/gen_aux_dictionary.py)

If the same entries are already in the main word dictionary, entries in this
directory is not used. If you need more controls, you might want to use
`aux_dictionary.tsv` and `dictionary_filter.tsv`.

*   https://github.com/google/mozc/blob/master/src/data/dictionary_oss/aux_dictionary.tsv
*   https://github.com/google/mozc/blob/master/src/data/dictionary_oss/dictionary_filter.tsv

## domain.txt

This is the same format and used as a part of the main word dictionary.

We recommend TSV files rather than this file, since the values for POS and cost
will be changed per dictionary update.
