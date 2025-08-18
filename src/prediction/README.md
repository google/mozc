# prediction

`prediction` is the directory for the word prediction module.

## Terminology

### suggestion

Candidate words while text composing.

Note, in mobile config (i.e. Gboard with virtual keyboard), prediction is always
used instead of suggestion.

### prediction

Candidate words after the `predict` command. The `predict` command is typically
assigned to the candidate expansion button in virtual keyboard, and the TAB key
in physical keyboard.

Note, in mobile config (i.e. Gboard with virtual keyboard), prediction is always
used instead of suggestion.

### zero query suggestion

Candidate words after the `commit` command. (e.g. "hello" → "world", "あけまして" →
"おめでとう").

This is the same meaning with "next word prediction".

## Tools

### gen_zero_query_data

`gen_zero_query_data` generates the data files for zero query suggestions.

`@zero_query` suffix to the `mozc_dataset` rule executes this tool.

For example

```
bazel //data_manager/oss:mozc_dataset_for_oss@zero_query
```
