GenericHID GUI
==============

Dependencies
------------

On Ubuntu, this command will installed the required build packages.  There is
a total of about 200MB on a fresh system.

sudo apt-get install build-essential qt4-qmake libqt4-dev libusb-1.0-0-dev gcc-avr binutils-avr gdb-avr avr-libc avrdude


Building
========

linux
-----

> qmake
> make
> sudo make install


Win32
-----

A windows build is currently not supported until I decide what to do with 
libusb.  libusb-0.1-win32 does not support Win7+.  libusb-1.0-win32 has some 
significant differences to the linux version to make it work.  It also doesn't
support running as a general usb access library anymore either.

GenericHID firmware
===================

The firmware is now part of the build.  3 versions are built and put in the bin 
directory... usbkey.hex (at90usb1287@8MHz), generic16.hex (at90usb1287@16MHz)
and teensypp2 (at90usb1286@16Mhz using the half-kay bootloader).

