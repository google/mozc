# Configurations

## IBus

IBus specific configurations are customizable in `~/.config/mozc/ibus_config.textproto`.

The file path may be `~/.mozc/ibus_config.textproto` if `~/.mozc` directry already exists.


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

**Note:**
After modification of `ibus_config.textproto`,
`ibus write-cache; ibus restart` might be necessary to apply changes.


### Specify the keyboard layout

`layout` represents the keyboard layout (e.g. JIS keyboard, US keyboard, etc.).

Sample configuration
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

`active_on_launch` represents the default input.
If it's True, Hiragana input is the default mode.

Sample configuration
```
engines {
  name : "mozc-jp"
  longname : "Mozc"
  layout : "default"
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
