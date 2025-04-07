PREFIX?=/usr/X11R6
CFLAGS?=-Os -pedantic -Wall
BINDIR?=/usr/local/bin

all:
	$(CC) $(CFLAGS) -I$(PREFIX)/include tinywm.c -L$(PREFIX)/lib -lX11 -o tinywm

install: tinywm
	install -Dm755 tinywm $(BINDIR)/tinywm

uninstall:
	rm -f $(BINDIR)/tinywm

clean:
	rm -f tinywm