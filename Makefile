NAME = pingpath

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
SRC += pinger.c parser.c stat.c dns.c whois.c
SRC += ui/style.c ui/appbar.c ui/action.c ui/option.c
SRC += tabs/ping.c tabs/log.c

OBJS = $(SRC:.c=.o)

RELEASE = -D RELEASE

all: $(NAME)

release: CFLAGS += -D RELEASE
release: $(NAME)

%.o: %.c %.h common.h
	$(CC) -c -o $@ $(CFLAGS) $<

$(NAME): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LIBS)

clean:
	rm -f $(OBJS)
	rm -f $(NAME)

## TMP: internal devs
test.o: test.c
	$(CC) -c -o $@ $(CFLAGS) $<
test: test.o
	$(CC) -o $@ test.o $(LIBS)

