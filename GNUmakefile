
NAME     = pingpath
NAME2    = $(NAME)2
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

DESC  = $(NAME).desktop
DESC_SRC  = $(ASSETS)/$(DESC)
DESC_DST  = $(GROUP).$(DESC)
DESC2 = $(NAME2).desktop
DESC2_SRC = $(ASSETS)/$(DESC2)
DESC2_DST = $(GROUP).$(DESC2)

OBJDIR  = _build
OBJDIR2 = $(OBJDIR)2

PKGS = gtk4

CC ?= gcc
CFLAGS += -Wall
CFLAGS += -I.
LIBS = -lm

ifndef NO_JSON
PKGS += json-glib-1.0
CFLAGS += -DWITH_JSON
endif

ifndef NO_DND
CFLAGS += -DWITH_DND
endif

ifdef PLOT
CFLAGS += -DWITH_PLOT
PKGS += gl
PKGS += cglm
PKGS += epoxy
APP = $(NAME)
BUILDDIR = $(OBJDIR)
else
APP = $(NAME2)
BUILDDIR = $(OBJDIR2)
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
ifdef PLOT
SRC += tabs/plot.c tabs/plot_aux.c tabs/plot_pango.c
endif

OBJS = $(SRC:%.c=$(BUILDDIR)/%.o)

all:
	@echo 'make $(NAME2) with 2D graphs'
	@make $(NAME2)
	@echo 'make $(NAME) with 2D graphs and 3D plots'
	@make PLOT=1 $(NAME)

$(OBJS): $(BUILDDIR)/%.o: %.c common.h
	@mkdir -p $(@D)
	@echo '  CC $@'
	@$(CC) -c -o $@ $(CFLAGS) $<

$(APP): $(OBJS)
	@echo '  LD $@'
	@$(CC) -o $@ $(OBJS) $(LDFLAGS) $(LIBS)

clean:
	rm -f -- $(NAME) $(NAME2)
	rm -rf -- $(OBJDIR) $(OBJDIR2)

install: $(NAME) $(NAME2)
	install -m 755 -d $(BASEDIR)/bin $(MANDIR) $(SMPLDIR) $(DSCDIR) $(SVGICODIR)
	install -m 755 -s $(NAME) $(BASEDIR)/bin/
	install -m 755 -s $(NAME2) $(BASEDIR)/bin/
	install -m 644 $(NAME).1 $(MANDIR)/
	gzip -f $(MANDIR)/$(NAME).1
	ln -srf $(MANDIR)/$(NAME).1.gz $(MANDIR)/$(NAME2).1.gz
	install -m 644 -T $(CONF_SRC) $(SMPLDIR)/$(CONF_DST)
	install -m 644 -T $(DESC_SRC) $(DSCDIR)/$(DESC_DST)
	install -m 644 -T $(DESC2_SRC) $(DSCDIR)/$(DESC2_DST)
	install -m 644 $(ICONNAME).svg $(SVGICODIR)/

