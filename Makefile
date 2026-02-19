PREFIX=/usr/local
PKG_CONFIG=pkg-config
CC=cc

INCS=`${PKG_CONFIG} --cflags freetype2`
LIBS=-lX11 -lXft -lXss -lm -lao
CFLAGS=-O1 -Wall -Wextra

xrest: main.c timer.c
	${CC} main.c timer.c -o xrest ${INCS} ${CFLAGS} ${LIBS} ${LDFLAGS}

all: xrest

install: all sounds
	install -m 644 -Dt ${PREFIX}/share/xrest/sounds sounds/*.wav
	install -m 755 -D xrest ${PREFIX}/bin
	@echo '	An example config file is provided in `examples/config.ini`:'
	@echo '	$$ mkdir -p ~/.config/xrest && cp examples/config.ini ~/.config/xrest'

.PHONY: all install
