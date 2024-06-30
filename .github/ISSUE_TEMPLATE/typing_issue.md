---
name: Typing issue
about: Out-of-vocabulary, ranking issues, etc.
title: 'Typing issue: '
labels: ''
assignees: ''

---

## Category of the typing issue
Choose one of them (delete rest of them)
1. Out-of-vocabulary (e.g. "夕日" is not in the candidate list at all).
2. Word ranking issue (e.g. "夕日" is in the list, but ranking is lower than expected).
3. Conversion issue (e.g. "夕日がきれい" is not converted as expected).
4. Others


## Issues
Write issues to the following table. (It's in the markdown format)

| input [e.g.ゆうひ] | expected [e.g. 夕日] | actual [e.g. ユウヒ] |
| ------------ | ------------ | ------------|
| (write here) | (write here) | (optional, write here) |


## Version or commit-id
[e.g. Mozc-2.28.4960.100+24.11.oss or d50a8b9ae28c4fba265f734b38bc5ae392fe4d25]
You can get the version string by converting "Version" or "ばーじょん".


## Additional context
Add any other context about the problem here.

> [!NOTE]
>
> Typing issues will be closed when the entries are added to test cases and
> evaluations.
>
> https://github.com/google/mozc/blob/master/src/data/test/quality_regression_test/oss.tsv
> https://github.com/google/mozc/blob/master/src/data/dictionary_oss/evaluation.tsv
