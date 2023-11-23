NAME = pingpath

CC ?= gcc
PKGCONFIG = $(shell which pkg-config)
LIBS = $(shell $(PKGCONFIG) --libs gtk4)
CFLAGS = $(shell $(PKGCONFIG) --cflags gtk4)
CFLAGS += -Wall
ifeq ($(CC),gcc)
CFLAGS += -fanalyzer
endif
#CFLAGS += -g

SRC = $(NAME).c menu.c pinger.c

OBJS = $(SRC:.c=.o)

all: $(NAME)

%.o: %.c %.h common.h
	echo CC=$(CC) $(CFLAGS)
	$(CC) -c -o $(@F) $(CFLAGS) $<

$(NAME): $(OBJS)
	$(CC) -o $(@F) $(OBJS) $(LIBS)

clean:
	rm -f $(OBJS)
	rm -f $(NAME)

