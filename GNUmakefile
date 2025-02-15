
NAME     = pingpath
GROUP    = net.tools
ASSETS   = assets
ICONNAME = $(ASSETS)/icons/$(NAME)
CONF_SRC = $(CONF_DST).sample
CONF_DST = $(NAME).conf

PREFIX    ?= /usr/local
BASEDIR    = $(DESTDIR)$(PREFIX)
SHRDIR     = $(BASEDIR)/share
MANDIR    ?= $(SHRDIR)/man/man1
DSCDIR    ?= $(SHRDIR)/applications
SVGICODIR ?= $(SHRDIR)/icons/hicolor/scalable/apps
SMPLDIR   ?= $(SHRDIR)/doc/$(NAME)/examples

DESC      = $(NAME).desktop
DESC_SRC  = $(ASSETS)/$(DESC)
DESC_DST  = $(GROUP).$(DESC)

BUILDDIR  = _build

PKGS = gtk4

CC ?= gcc
CFLAGS += -Wall
CFLAGS += -Wextra
CFLAGS += -D_GNU_SOURCE
CFLAGS += -I.
CFLAGS += -include config.h
LIBS = -lm

ifndef NO_JSON
PKGS += json-glib-1.0
CFLAGS += -DWITH_JSON
endif

ifndef NO_DND
CFLAGS += -DWITH_DND
endif

ifndef NO_PLOT
CFLAGS += -DWITH_PLOT
PKGS += gl
PKGS += cglm
PKGS += epoxy
endif

ifdef WITH_SNAPHINT
CFLAGS += -DWITH_SNAPHINT
endif

ifdef PINGDIR
CFLAGS += -DPINGDIR='"$(PINGDIR)"'
endif

ifndef NO_SECURE_GETENV
CFLAGS += -DHAVE_SECURE_GETENV
endif
ifndef NO_LOCALTIME_R
CFLAGS += -DHAVE_LOCALTIME_R
endif
ifndef NO_USELOCALE
CFLAGS += -DHAVE_USELOCALE
endif

CFLAGS += $(shell $(PKGCONFIG) --cflags $(PKGS))
LIBS += $(shell $(PKGCONFIG) --libs $(PKGS))

ifdef DEBUG
CFLAGS += -g
ifeq ($(CC),gcc)
CFLAGS += -ggdb
endif
CFLAGS += -fno-omit-frame-pointer
endif

ifeq ($(DEBUG),asan)
LIBS += -lasan
CFLAGS += -fanalyzer
CFLAGS += -fsanitize=address
endif

# PKGCONFIG = $(shell which pkg-config)
PKGCONFIG = pkg-config

SRC = $(NAME).c common.c
SRC += pinger.c parser.c stat.c series.c dns.c whois.c cli.c
SRC += ui/style.c ui/appbar.c ui/action.c ui/option.c
SRC += ui/clipboard.c ui/notifier.c
SRC += tabs/aux.c tabs/ping.c tabs/graph.c tabs/log.c
ifndef NO_PLOT
SRC += tabs/plot.c tabs/plot_aux.c tabs/plot_pango.c
endif

ALLO   = $(NAME)
INST   = base_install
OBJS   = $(SRC:%.c=$(BUILDDIR)/%.o)
MOPO   =
PO_SRC =

ifndef NO_NLS
PO_SRC += po/es.po
PO_SRC += po/it.po
PO_SRC += po/pt.po
PO_SRC += po/uk.po
CFLAGS += -DWITH_NLS -DLOCALEDIR='"$(SHRDIR)/locale"'
MOPO   += $(PO_SRC:%.po=$(BUILDDIR)/%.mo)
INST   += nls_install
ALLO   += $(MOPO)
endif

all: $(ALLO)

$(NAME): config $(OBJS)
	@echo '  LD  $@'
	@$(CC) -o $@ $(OBJS) $(LDFLAGS) $(LIBS)

$(OBJS): $(BUILDDIR)/%.o: %.c common.h
	@mkdir -p $(@D)
	@echo '  CC  $@'
	@$(CC) -c -o $@ $(CFLAGS) $<

$(MOPO): $(BUILDDIR)/%.mo: %.po
	@mkdir -p $(@D)
	@echo '  GEN $@'
	@msgfmt -o $@ $<

config:
	@touch config.h

clean:
	rm -f -- $(NAME) config.h
	rm -rf -- $(BUILDDIR)

install: $(INST)

base_install: $(NAME)
	install -m 755 -d $(BASEDIR)/bin $(MANDIR) $(SMPLDIR) $(DSCDIR) $(SVGICODIR)
	install -m 755 -s $(NAME) $(BASEDIR)/bin/
	install -m 644 $(NAME).1 $(MANDIR)/
	install -m 644 -T $(CONF_SRC) $(SMPLDIR)/$(CONF_DST)
	install -m 644 -T $(DESC_SRC) $(DSCDIR)/$(DESC_DST)
	install -m 644 $(ICONNAME).svg $(SVGICODIR)/

nls_install: $(MOPO)
	install -m 644 $< $(SHRDIR)/locale/$(basename $(notdir $<))/LC_MESSAGES/$(NAME).mo

