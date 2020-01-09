CC = gcc
AR = ar
ARFLAGS = crs
CFLAGS = -Wall -Wextra
ifeq ($(DEBUG),1)
CFLAGS += -g
else
CFLAGS += -O3
endif

.PHONY : all

all : libcpeg.a

SOURCES = terms.c

HEADERS = libcpeg.h libcpeg_terms.h

OBJECTS = $(SOURCES:.c=.o)

$(OBJECTS) : $(HEADERS) Makefile

libcpeg.a :  $(OBJECTS)
	$(AR) $(ARFLAGS) $@ $^

.PHONY : clean

clean:
	rm -f $(OBJECTS)
	rm -f libcpeg.a
