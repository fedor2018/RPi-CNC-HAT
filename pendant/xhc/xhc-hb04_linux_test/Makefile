VERSION=V03

all: xhc-hb04
	CFLAGS=-DVERSION=$(VERSION)
#LinuxCNC HAL module

xhc-hb04: xhc-hb04.cc
	g++ $(CFLAGS) xhc-hb04.cc -o $@ -lusb-1.0 -lm

install:
	sudo install -m 0755 -g root -o root xhc-hb04 /usr/local/bin

install.debug: xhc-hb04
	cp xhc-hb04 /usr/local/bin

clean:
	rm xhc-hb04

distrib:
	cd ..; tar cvzf xhc-hb04-$(VERSION).tgz src/Makefile src/README src/xhc-hb04.cc src/xhc-hb04.hal src/*.ini
