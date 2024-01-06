
NAME = pingpath
GROUP = net.tools
DESC = $(NAME).desktop
DESC_SRC = assets/$(DESC)
DESC_DST = $(GROUP).$(DESC)
BASEICONAME = assets/icons/$(NAME)
ICO_SIZES = 48 512

PREFIX ?= /usr/local
BASEDIR = $(DESTDIR)$(PREFIX)
MANDIR ?= $(BASEDIR)/share/man/man1
DSCDIR ?= $(BASEDIR)/share/applications
BASEICODIR ?= $(BASEDIR)/share/icons/hicolor
SVGICODIR  ?= $(BASEICODIR)/scalable/apps

CC ?= gcc
LIBS = $(shell $(PKGCONFIG) --libs gtk4)
LIBS += -lm
CFLAGS = -I.
CFLAGS += $(shell $(PKGCONFIG) --cflags gtk4)
CFLAGS += -Wall
ifeq ($(CC),gcc)
CFLAGS += -fanalyzer
endif
#CFLAGS += -g

PKGCONFIG = $(shell which pkg-config)

SRC = $(NAME).c common.c
SRC += pinger.c parser.c stat.c dns.c whois.c cli.c
SRC += ui/style.c ui/appbar.c ui/action.c ui/option.c
SRC += ui/clipboard.c ui/notifier.c
SRC += tabs/ping.c tabs/log.c

OBJS = $(SRC:.c=.o)

define ICO_INST
mkdir -p $(1) && install -T -m 644 $(2) $(1)/$(NAME).png;
endef

all: $(NAME)

%.o: %.c %.h common.h
	$(CC) -c -o $@ $(CFLAGS) $<

$(NAME): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LIBS)

clean:
	rm -f $(OBJS)
	rm -f $(NAME)

install: $(NAME)
	@mkdir -p $(BASEDIR)/bin
	install -m 755 $(NAME) $(BASEDIR)/bin
	@mkdir -p $(MANDIR)
	install -m 644 $(NAME).1 $(MANDIR)/
	gzip -f $(MANDIR)/$(NAME).1
	@mkdir -p $(DSCDIR)
	install -T -m 644 $(DESC_SRC) $(DSCDIR)/$(DESC_DST)
	@mkdir -p $(SVGICODIR)
	install -m 644 $(BASEICONAME).svg $(SVGICODIR)
#	$(foreach sz,$(ICO_SIZES),$(call ICO_INST,$(BASEICODIR)/$(sz)x$(sz)/apps,$(BASEICONAME).$(sz).png))

