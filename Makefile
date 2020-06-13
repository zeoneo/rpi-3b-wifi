all:
	cd src && $(MAKE) DISABLE_EXP=1

wpa_supplicant.a:
	cd hostap/wpa_supplicant && make -f Makefile.rpi3b
	cp hostap/wpa_supplicant/libwpa_supplicant.a src/

clean:
	cd src && $(MAKE) clean
	cd hostap/wpa_supplicant && $(MAKE) clean
	