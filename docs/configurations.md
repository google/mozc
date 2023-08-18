# Configurations

## Date format converted from "きょう"

This is an EXPERIMENTAL feature. This feature may be changed or removed in
future.

The conversion from "きょう" returns the current date in some formats like
"2021年10月1日" and "2021-10-01".

You can add an extra format by adding an entry of "DATA_FORMAT" to the user
dictionary.

| Reading     | Word                  | Category |
| ----------- | --------------------- | -------- |
| DATE_FORMAT | {YEAR}.{MONTH}.{DATE} | 名詞     |

Any category is OK so far.

| key     | value                |
| ------- | -------------------- |
| {YEAR}  | 4 digits year (2021) |
| {MONTH} | 2 digits month (10)  |
| {DATE}  | 2 digits date (01)   |
| {{}     | sinle { character    |

## Configuration path

Mozc creates configuration files under `$XDG_CONFIG_HOME/mozc` (default:
`~/.config/mozc`). If `~/.mozc` already exists, `~/.mozc` is used for backward
compatibility.

## Configuration files

`config1.db` stores configurations managed by the GUI configuration tool. The
format is binary protocol buffer of
[mozc.config.Config](https://github.com/google/mozc/blob/master/src/protocol/config.proto).

The following command can decode `config1.db` to text protocol buffer format.

```
# Decode to text proto
protoc --decode=mozc.config.Config protocol/config.proto < config1.db > config.textproto
```

```
# Encode to binary proto
protoc --encode=mozc.config.Config protocol/config.proto < config.textproto > config1.db
```

`protoc` can be built as follows

```
bazel build @com_google_protobuf//:protoc
```

## IBus

IBus specific configurations are customizable in
`~/.config/mozc/ibus_config.textproto`.

The file path may be `~/.mozc/ibus_config.textproto` if `~/.mozc` directory
already exists.

Here is the default configuration.

```
engines {
  name : "mozc-jp"
  longname : "Mozc"
  layout : "default"
  layout_variant : ""
  layout_option : ""
  rank : 80
}
active_on_launch: False
```

The variables of `engines` are mapped to the same named variables of IBus.

**Note:** After modification of `ibus_config.textproto`, `ibus write-cache; ibus
restart` might be necessary to apply changes.

### Specify the keyboard layout

`layout` represents the keyboard layout (e.g. JIS keyboard, US keyboard, etc.).

Sample configuration:

```
engines {
  name : "mozc-jp"
  longname : "Mozc"
  layout : "ja"  # or "us"
  layout_variant : ""
  layout_option : ""
  rank : 80
}
active_on_launch: False
```

### Activate Mozc on launch

`active_on_launch` represents the default input. If it's True, Hiragana input is
the default mode. `active_on_launch` is supported by 2.26.4317 or later.

Sample configuration:

```
engines {
  name : "mozc-jp"
  longname : "Mozc"
  layout :"default"
  layout_variant : ""
  layout_option : ""
  rank : 80
}
active_on_launch: True  # default is False.
```

### Multiple engines with different keyboard layouts

`engines` can be multiple entries.

```
engines {
  name : "mozc-jp"
  longname : "Mozc"
  layout : "ja"
  layout_variant : ""
  layout_option : ""
  rank : 80
}
engines {
  name : "mozc-us"
  longname : "Mozc US"
  layout : "us"
  layout_variant : ""
  layout_option : ""
  rank : 80
}
active_on_launch: False
```

### Fixed composition mode per engine

`composition_mode` specifies the composition mode every time the engine is
enabled. This is useful to switch the composition mode by using Ibus hot keys.

The available values are `DIRECT`, `HIRAGANA`, `FULL_KATAKANA`, `HALF_ASCII`, `FULL_ASCII`, `HALF_KATAKANA`, and `NONE`.
`NONE` does not change the composition mode.

`symbol` is a label to represent the engine used by Ibus.

```
engines {
  name : "mozc-on"
  longname : "Mozc:あ"
  layout : "default"
  layout_variant : ""
  layout_option : ""
  rank : 80
  symbol : "あ"
  composition_mode: HIRAGANA
}
engines {
  name : "mozc-off"
  longname : "Mozc:A_"
  layout : "default"
  layout_variant : ""
  layout_option : ""
  rank : 80
  symbol: "A"
  composition_mode: DIRECT
}
```


### Candidate window

Linux input method frameworks (e.g. IBus and Fcitx) often provide their own
candidate window UI, while Mozc also provides candidate window UI via
`mozc_renderer` process that is used in macOS and Windows as well.

When `$XDG_SESSION_TYPE` is set to `wayland`, ibus-mozc uses IBus' default
candidate window by default due to the technical limitations in Wayland.
Otherwise, Mozc's candidate window will be used by default.

To override the above behavior, set an environment variable
`MOZC_IBUS_CANDIDATE_WINDOW` to `mozc` or `ibus`.

Here is the quick comparison of two options.

#### Mozc's candidate window

 * Provides the same UI and functionality as Mozc for macOS and Windows.
 * Many technical and compatibility challenges to properly work under Wayland sessions
   ([#431](https://github.com/google/mozc/issues/431)).
 * Theme support (e.g. dark theme) is planned but not yet implemented.

#### IBus' candidate window

 * Provides a UI that is consistent with the desktop environment (e.g. theme
   support including dark mode).
 * Supports Wayland by using deep integration with the desktop shell.
 * Lacks several features that are available in Mozc for macOS and Windows.
