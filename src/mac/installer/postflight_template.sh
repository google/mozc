#!/bin/sh
CONSOLE_USER=`/usr/bin/stat -f%Su /dev/console`
/usr/bin/sudo -u $CONSOLE_USER /usr/bin/killall MozcConverter > /dev/null
/usr/bin/sudo -u $CONSOLE_USER /usr/bin/killall MozcRenderer > /dev/null
/usr/bin/sudo -u $CONSOLE_USER /usr/bin/killall Mozc > /dev/null
/usr/bin/true
