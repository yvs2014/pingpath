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

SRC = $(NAME).c pinger.c parser.c stat.c ui/style.c ui/appbar.c ui/action.c ui/option.c tabs/ping.c valid.c aux.c
OBJS = $(SRC:.c=.o)

all: $(NAME)

%.o: %.c %.h common.h aux.h
	$(CC) -c -o $@ $(CFLAGS) $<

$(NAME): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LIBS)

clean:
	rm -f $(OBJS)
	rm -f $(NAME)

