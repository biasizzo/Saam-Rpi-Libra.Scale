SOURCE := $(wildcard *.c)
BINARY := $(patsubst %.c, %, $(SOURCE))
FLAGS := -O2
CC := gcc

.PHONY: all run clean

all: $(BINARY)

%: %.c
	$(CC) $(FLAGS) -o $@ $<

install: $(BINARY)
	install -m 755 $(BINARY) -t /usr/local/sbin
