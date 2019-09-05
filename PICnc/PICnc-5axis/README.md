PICnc-5axis
========

PICnc 5 axis is a PIC32 based hardware step generator board that is designed for MachineKit based off kinsamanka's PICnc-V2 project. 

Expanded channels + GPIOs due to the use of a 44 pin package PIC32. 
No intergrated drivers. Outputs step + dir for external high power driver modules. 
All IOs on screw terminal blocks.
Also added support for laser cutters with isolated laser_on and laser_PWM outputs and related hardware inhibits.

Folders:
HAL contains code to build the HAL driver for LinuxCNC
firmware contains code to build the PIC32 firmware 


More info:
Logic, BoM, and files needed to build the hardware side can be found here: http://www.wire2wire.org/PICnc_5axis/PICnc_5axis.html

Thread for the PICnc base code and help: http://www.raspberrypi.org/forums/viewtopic.php?f=37&t=33809

My documentation for installing Raspbian and Machinekit for use with above can be found here: https://docs.google.com/document/d/1qywnRqKlxesJSEX0jnTapQYujmW3PFJz6HwiM8bZ16o/edit?usp=sharing

My thread for my laser cutter rebuild that started this project can be found here: http://www.cnczone.com/forums/laser-hardware/237638-rebuild-log-universal-laser-systems-25ps.html
