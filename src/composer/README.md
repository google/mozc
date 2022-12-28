# composer

`composer` is a module of transliteration from user input to Hiragana text and
text editor to let the user modify input text.

Here are some typical processes by `composer`.

* Transliteration from keyboard to Hiragana text (e.g. "neko" to "ねこ").
* Text edit of the composing text such as cursor motion and backspace.
* Transliteration from the composing text to other forms
  (e.g. full-width alphabet)

In Mozc, the words `composition`, `preedit` and `composing text` are used
as the same meaning.

## Files

* `composer.h`: Interface for other modules, mainly for `session`.
* `table.h`: Transliteration rules.
* `internal/composision.h`: Handler of the composing text.
* `internal/char_chunk.h`: Minimal unit of the composing text.
                           (e.g. "あ", "きゃ", etc.)

## References

* `session` uses `composer` as a member of `ImeContext`.
* `data/preedit/` contains transliteration rules used by `composer` via `table`.
