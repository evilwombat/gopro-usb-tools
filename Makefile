all: gpboot prepare-bootstrap

gpboot: gpboot.c gp_api.c gp_api.h gp_ddr.c gp_lcd.c gp_lcd.h
	cc gpboot.c gp_api.c gp_ddr.c gp_lcd.c `pkg-config --libs --cflags libusb-1.0` -o gpboot

prepare-bootstrap: prepare-bootstrap.c fw-patch.h

clean:
	rm -f *~ *.o gpboot prepare-bootstrap

