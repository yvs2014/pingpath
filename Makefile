
NAME = pingpath
GROUP = net.tools
DESC = $(NAME).desktop
DESC_SRC = assets/$(DESC)
DESC_DST = $(GROUP).$(DESC)
BASEICONAME = assets/icons/$(NAME)
ICO_SIZES = 48 512
CONF_DST = $(NAME).conf
CONF_SRC = $(CONF_DST).sample

PREFIX ?= /usr/local
BASEDIR = $(DESTDIR)$(PREFIX)
SHRDIR  = $(BASEDIR)/share
MANDIR ?= $(SHRDIR)/man/man1
DSCDIR ?= $(SHRDIR)/applications
BASEICODIR ?= $(SHRDIR)/icons/hicolor
SVGICODIR  ?= $(BASEICODIR)/scalable/apps
SMPLDIR ?= $(SHRDIR)/doc/$(NAME)/examples

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

NO_PPGL = 1 # at dev stage
ifndef NO_PPGL
CFLAGS += -DWITH_PPGL
PKGS += gl
PKGS += cglm
PKGS += epoxy
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
#CFLAGS += -fsanitize=undefined
#CFLAGS += -fsanitize=thread
#CFLAGS += -fsanitize=float-divide-by-zero
#CFLAGS += -fsanitize=float-cast-overflow
#ifeq ($(CC),clang)
#CFLAGS += -fsanitize=memory
#endif
endif

PKGCONFIG = $(shell which pkg-config)

SRC = $(NAME).c common.c
SRC += pinger.c parser.c stat.c dns.c whois.c cli.c
SRC += ui/style.c ui/appbar.c ui/action.c ui/option.c
SRC += ui/clipboard.c ui/notifier.c
SRC += tabs/ping.c tabs/graph.c tabs/log.c
#define NO_PPGL
ifndef NO_PPGL
SRC += tabs/ppgl.c tabs/ppgl_aux.c tabs/ppgl_pango.c
endif

OBJS = $(SRC:.c=.o)

define ICO_INST
mkdir -p $(1) && install -T -m 644 $(2) $(1)/$(NAME).png;
endef

all: $(NAME)

%.o: %.c %.h common.h
	$(CC) -c -o $@ $(CFLAGS) $<

$(NAME): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS) $(LIBS)

clean:
	rm -f $(OBJS)
	rm -f $(NAME)

install: $(NAME)
	@mkdir -p $(BASEDIR)/bin
	install -m 755 $(NAME) $(BASEDIR)/bin
	@mkdir -p $(MANDIR)
	install -m 644 $(NAME).1 $(MANDIR)/
	gzip -f $(MANDIR)/$(NAME).1
	@mkdir -p $(SMPLDIR)
	install -T -m 644 $(CONF_SRC) $(SMPLDIR)/$(CONF_DST)
	@mkdir -p $(DSCDIR)
	install -T -m 644 $(DESC_SRC) $(DSCDIR)/$(DESC_DST)
	@mkdir -p $(SVGICODIR)
	install -m 644 $(BASEICONAME).svg $(SVGICODIR)
#	$(foreach sz,$(ICO_SIZES),$(call ICO_INST,$(BASEICODIR)/$(sz)x$(sz)/apps,$(BASEICONAME).$(sz).png))

