all: wpa_supplicant.a
	cd src && $(MAKE)

wpa_supplicant.a:
	git submodule update --init hostap
	cd hostap/wpa_supplicant && make -f Makefile.rpi3b
	cp hostap/wpa_supplicant/libwpa_supplicant.a src/

clean:
	cd src && $(MAKE) clean
	cd hostap/wpa_supplicant && $(MAKE) clean
	
