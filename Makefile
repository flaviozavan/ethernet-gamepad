CFLAGS=-Wall `sdl-config --cflags`
LDLIBS=`sdl-config --libs`
PREFIX=/usr/local

all: ethernet-gamepad-client ethernet-gamepad-server

clean:
	rm -f ethernet-gamepad-client ethernet-gamepad-server

install:
	install ethernet-gamepad-client $(PREFIX)/bin/ethernet-gamepad-client
	install ethernet-gamepad-server $(PREFIX)/bin/ethernet-gamepad-server
	install -D bg.bmp $(PREFIX)/share/ethernet-gamepad/bg.bmp

uninstall:
	rm -Rf $(PREFIX)/bin/ethernet-gamepad-client \
		$(PREFIX)/bin/ethernet-gamepad-server \
		$(PREFIX)/share/ethernet-gamepad
