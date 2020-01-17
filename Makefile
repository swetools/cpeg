CC = gcc
AR = ar
ARFLAGS = crs
CPPFLAGS = -I.
LDFLAGS = -L.
CFLAGS = -Wall -Wextra
ifeq ($(DEBUG),1)
CFLAGS += -g
else
CFLAGS += -O3
endif

TEST_CPPFLAGS = -DLIBCPEG_TESTING=1
TEST_CFLAGS = --coverage
GCOV = gcov
GCOV_FLAGS = 


.PHONY : all

all : libcpeg.a

SOURCES = terms.c

HEADERS = libcpeg.h libcpeg_terms.h

OBJECTS = $(SOURCES:.c=.o)

TESTS = terms

$(OBJECTS) : $(HEADERS) Makefile

libcpeg.a :  $(OBJECTS)
	$(AR) $(ARFLAGS) $@ $^

TEST_OBJECTS = $(OBJECTS:.o=.tst.o)

TEST_APPS = $(addprefix tests/,$(TESTS))

libcpeg.tst.a : $(TEST_OBJECTS)
	$(AR) $(ARFLAGS) $@ $^

%.tst.o : %.c
	$(CC) -c -o $@ $(TEST_CPPFLAGS) $(CPPFLAGS) $(TEST_CFLAGS) $(CFLAGS) $<

tests/% : tests/%.tst.o libcpeg.tst.a
	$(CC) -o $@ $(TEST_CFLAGS) $(CFLAGS) $(LDFLAGS) $^

.PHONY : clean

clean:
	rm -f $(OBJECTS)
	rm -f $(TEST_OBJECTS)
	rm -f *.a
	rm -f $(TEST_APPS)
	rm -f $(addsuffix .tst.o,$(TEST_APPS))
	rm -f *.gcda tests/*.gcda
	rm -f *.gcno tests/*.gcno
	rm -f *.gcov

.PHONY : check

check: $(TEST_APPS)
	set -e; for e in $(TEST_APPS); do ./$$e; done
	$(GCOV) $(GCOV_FLAGS) -o tests $(TEST_OBJECTS)
