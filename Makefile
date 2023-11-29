NAME = pingpath

CC ?= gcc
PKGCONFIG = $(shell which pkg-config)
LIBS = $(shell $(PKGCONFIG) --libs gtk4)
LIBS += -lm
CFLAGS = $(shell $(PKGCONFIG) --cflags gtk4)
CFLAGS += -Wall
ifeq ($(CC),gcc)
CFLAGS += -fanalyzer
endif
#CFLAGS += -g
#CFLAGS += -Ibase
#CFLAGS += -Iui

SRC = $(NAME).c pinger.c parser.c stat.c appbar.c styles.c valid.c aux.c

OBJS = $(SRC:.c=.o)

all: $(NAME)

%.o: %.c %.h common.h aux.h
	echo CC=$(CC) $(CFLAGS)
	$(CC) -c -o $(@F) $(CFLAGS) $<

$(NAME): $(OBJS)
	$(CC) -o $(@F) $(OBJS) $(LIBS)

clean:
	rm -f $(OBJS)
	rm -f $(NAME)

