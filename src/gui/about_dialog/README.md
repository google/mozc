# gui/about_dialog

`About dialog` is a small GUI application to show the current version.

## .qtts files

We use `.qtts` for Qt translation files rather than `.ts` to avoid the confusion
with TypeScript files.

## .qm files

`.qm` files are binary data converted from the `.qtts` files. Since the build
process does not convert the files, you need to manually update `.qm` files when
you update `.qtts` files.

```shell
lrelease about_dialog_en.qtts -qm about_dialog_en.qm
```
