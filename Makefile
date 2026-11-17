CC = gcc
CFLAGS = -Wall -Wextra -std=gnu11 -O2

# `make` builds everything, `make grep` builds one program
PROGS = cat grep grep_ext wc head tail cp tee echo true false uniq template

all: $(PROGS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(PROGS)

.PHONY: all clean
