all: gpboot prepare-bootstrap

gpboot: gpboot.c gp_api.c gp_api.h gp_ddr.c
	cc gpboot.c gp_api.c gp_ddr.c `pkg-config --libs --cflags libusb-1.0` -o gpboot

prepare-bootstrap: prepare-bootstrap.c hal-patch-v222.h

clean:
	rm -f *~ *.o gpboot prepare-bootstrap

