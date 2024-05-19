
NAME     = pingpath
NAME3    = $(NAME)3
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
DESC3 = $(NAME3).desktop
DESC3_SRC = $(ASSETS)/$(DESC3)
DESC3_DST = $(GROUP).$(DESC3)

OBJDIR  = _build
OBJDIR3 = $(OBJDIR)3

PKGS = gtk4

CC ?= gcc
CFLAGS = -Wall
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
APP = $(NAME3)
BUILDDIR = $(OBJDIR3)
else
APP = $(NAME)
BUILDDIR = $(OBJDIR)
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

PKGCONFIG = $(shell which pkg-config)

SRC = $(NAME).c common.c
SRC += pinger.c parser.c stat.c series.c dns.c whois.c cli.c
SRC += ui/style.c ui/appbar.c ui/action.c ui/option.c
SRC += ui/clipboard.c ui/notifier.c
SRC += tabs/ping.c tabs/graph.c tabs/log.c
ifdef PLOT
SRC += tabs/plot.c tabs/plot_aux.c tabs/plot_pango.c
endif

OBJS = $(SRC:%.c=$(BUILDDIR)/%.o)

all:
	@echo 'make $(NAME)'
	@make $(NAME)
	@echo 'make $(NAME3) with 3D plot'
	@make PLOT=1 $(NAME3)

$(OBJS): $(BUILDDIR)/%.o: %.c common.h
	@mkdir -p $(@D)
	@echo '  CC   $@'
	@$(CC) -c -o $@ $(CFLAGS) $<

$(APP): $(OBJS)
	@echo '  CCLD $@'
	@$(CC) -o $@ $(OBJS) $(LDFLAGS) $(LIBS)

clean:
	rm -f -- $(NAME) $(NAME3)
	rm -rf -- $(OBJDIR) $(OBJDIR3)

install: $(NAME) $(NAME3)
	@mkdir -p $(BASEDIR)/bin
	install -m 755 $(NAME) $(BASEDIR)/bin
	install -m 755 $(NAME3) $(BASEDIR)/bin
	@mkdir -p $(MANDIR)
	install -m 644 $(NAME).1 $(MANDIR)/
	gzip -f $(MANDIR)/$(NAME).1
	ln -srf $(MANDIR)/$(NAME).1.gz $(MANDIR)/$(NAME3).1.gz
	@mkdir -p $(SMPLDIR)
	install -T -m 644 $(CONF_SRC) $(SMPLDIR)/$(CONF_DST)
	@mkdir -p $(DSCDIR)
	install -T -m 644 $(DESC_SRC) $(DSCDIR)/$(DESC_DST)
	install -T -m 644 $(DESC3_SRC) $(DSCDIR)/$(DESC3_DST)
	@mkdir -p $(SVGICODIR)
	install -m 644 $(ICONNAME).svg $(SVGICODIR)

