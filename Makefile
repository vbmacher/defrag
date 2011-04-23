# Makefile for f32id
# (c) Copyright 2006, vbmacher <pjakubco@gmail.com>

TARGET = f32id
OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))
CC = gcc
CFLAGS = -Iinclude -O0 -fshort-enums -g
# -mcmodel=medium

#SUBDIRS = dir1 dir2 dir3
#.PHONY: subdirs $(SUBDIRS)
#subdirs: $(SUBDIRS)
#$(SUBDIRS):
#	make -C $@
# dir2: dir3

all: $(TARGET)

depend: $(OBJECTS:.o=.d)

$(TARGET): $(OBJECTS) 
	$(CC) $(CFLAGS) -o $@ $^; \
	xgettext -d f32id_loc -s -o f32id_loc.pot $(wildcard *.c)

%.o: %.c 
	$(CC) $(CFLAGS) -c $<

%.d: %.c
	set -e; \
	$(CC) $(CFLAGS) -MM $< | \
	sed 's/\($*\)\.o[ :]*/\1.o $@: /g' > $@;
	[ -s $@ ] || rm -f $@

-include $(OBJECTS:.o=.d)

.PHONY: clean
clean: 
	rm -f *.o *.d f32id
