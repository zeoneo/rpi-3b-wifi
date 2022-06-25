# rpi-3b-wifi
Raspberry Pi 3B (32 Bit) Bare Metal Wifi Driver

This is an attempt to write bare metal wifi driver for raspberry pi by following Plan9 code.

### Build Process
- Get docker desktop for your host operating system
- Change directory to `rpi-3b-wifi`
- Invoke `./build.sh`
- Find binary in current working directory.
- `IMAGE` directory has all the files which can be directly copied to sd card of PI to test this code.

Current Status : Wifi driver NOT WORKING

### Development Plan
- [x] Set up basic utilities like printf, timers, interrupt
- [x] Use emmec controller to read sd card's contents
- [x] Add sdhost driver to read sd card's content
- [x] Remove EMMC dependency to read sd card
- [x] Write basic SDIO driver for EMMC
- [x] Use emmc + sdio to enable wifi
- [x] Get broadcom binary driver for wifi chip
- [x] Load broadcom driver into wifi chip
- [ ] Join network with no security
- [ ] Join network with wpa2/psk security
- [ ] Initialize wifi
- [ ] Send packets
- [ ] Use light wight network library to use wifi
- [ ] Write basic http hello world test example using wifi


make DISABLE_EXP=1 to disable experimental

- Troubleshooting

Prefetch exception handler from dwelch67/mmu prints
last value of R0 = 0000A1E0
DFSR= 000008A7 => [2:0] = 0b111 => translation fault page from manual.
IFSR= 00001629
lr = 00008700
instruction = E92D4010 => e92d4010 push {r4, lr}
00008040

dump of data abort exception
00022680 00000001 00001629 00010584 E1D400B7 00022220
Last Ro value, DFSR, IFSR, LR, instruction, i don't know this

| Last Ro value 	| DFSR     	| IFSR     	| LR       	| instruction 	| Unkwown  	|
|---------------	|----------	|----------	|----------	|-------------	|----------	|
| 00022680      	| 00000001 	| 00001629 	| 00010584 	| E1D400B7    	| 00022220 	|


Format source code using:


``` make format ```