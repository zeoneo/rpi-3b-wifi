# rpi-3b-wifi
Raspberry Pi 3B (32 Bit) Bare Metal Wifi Driver

This is an attempt to write bare metal wifi driver for raspberry pi by following Plan9 code.

### Build Process
- Get docker desktop for your host operating system
- Change directory to `rpi-3b-wifi`
- Invoke `./build.sh`
- Find binary in current working directory folder.

Current Status : Wifi driver NOT WORKING

### Development Plan
- [x] Set up basic utilities like printf, timers, interrupt
- [x] Use emmec controller to read sd card's contents
- [ ] Add sdhost driver to read sd card's content
- [ ] Remove EMMC dependency to read sd card
- [ ] Write basic SDIO driver for EMMC
- [ ] Use emmc + sdio to enable wifi
- [ ] Get broadcom binary driver for wifi chip
- [ ] Load broadcom driver into wifi chip
- [ ] Initialize wifi
- [ ] Send packets
- [ ] Use light wight network library to use wifi
- [ ] Write basic http hello world test example using wifi
