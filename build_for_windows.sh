#!/usr/bin/sh

pwd

gcc -I/usr/include/libusb-1.0 daemon/xvcpico.c -o xvcd-pico.exe -lusb-1.0

find /bin -name cygwin1.dll -exec cp {} . \;

cp /usr/bin/cygusb-1.0.dll .
