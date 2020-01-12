# rpi-3b-wifi
Raspberry Pi 3B (32 Bit) Bare Metal Wifi Driver

This is an attempt to write bare metal wifi driver for raspberry pi by following Plan9 code.

### Build Process
- Get docker desktop for your host operating system
- Change directory to `rpi-3b-wifi`
- Invoke `./build.sh`
- Find binary in current working directory folder.

Current Status : NOT WORKING

-- As of now this code can display root directory contents.

-- First task is to remove dependency of EMMC controller to read sd card.
-- So I will try to use uboot's sdhost implementation to read sd card.
-- Next I will try to enable wifi using EMMC controller. I will refer richard miller's wifi driver for PLAN9.

