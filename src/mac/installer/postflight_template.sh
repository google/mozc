#!/bin/sh
CONSOLE_USER=`/usr/bin/stat -f%Su /dev/console`
/usr/bin/sudo -u $CONSOLE_USER /usr/bin/killall MozcConverter > /dev/null

if [ -n "$CONSOLE_USER" ] && [ "$CONSOLE_USER" != "root" ]; then
  /usr/bin/sudo -u "$CONSOLE_USER" "/Library/Input Methods/Mozc.app/Contents/MacOS/Mozc" --register_input_source > /dev/null 2>&1 || true
fi

/usr/bin/true
