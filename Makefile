CC = gcc
AR = ar
ARFLAGS = crs
CPPFLAGS = -I.
LDFLAGS = -L.
CFLAGS = -Wall -Wextra -Werror
ifeq ($(DEBUG),1)
CFLAGS += -g
else
CFLAGS += -O3
endif
CQC_INCLUDE = ../cqc

TEST_CPPFLAGS = -DLIBCPEG_TESTING=1 -I$(CQC_INCLUDE)
TEST_CFLAGS = --coverage
GCOV = gcov
GCOV_FLAGS =


.PHONY : all

all : libcpeg.a

SOURCES = terms.c memattr.c

HEADERS = libcpeg.h libcpeg_terms.h libcpeg_memattr.h

OBJECTS = $(SOURCES:.c=.o)

libcpeg.a :  $(OBJECTS)
	$(AR) $(ARFLAGS) $@ $^

TEST_OBJECTS = $(patsubst %.c,tests/%.tst.o,$(SOURCES))

TEST_APPS = $(patsubst %.c,tests/%,$(SOURCES))

tests/%.tst.o : %.c
	$(CC) -c -o $@ $(TEST_CPPFLAGS) $(CPPFLAGS) $(TEST_CFLAGS) $(CFLAGS) $<

tests/% : tests/%.tst.o
	$(CC) -o $@ $(TEST_CFLAGS) $(CFLAGS) $(filter %.o,$^) \
				$(TEST_LDFLAGS) $(LDFLAGS)

$(OBJECTS) $(TEST_OBJECTS) : $(HEADERS) Makefile

$(TEST_OBJECTS) : $(CQC_INCLUDE)/cqc.h

tests/terms : memattr.o

tests/memattr : terms.o

.PHONY : clean

clean:
	rm -f $(OBJECTS)
	rm -f $(TEST_OBJECTS)
	rm -f *.a
	rm -f $(TEST_APPS)
	rm -f tests/*.gcda
	rm -f tests/*.gcno
	rm -f *.gcov

.PHONY : check

check: $(TEST_APPS)
	set -e; for e in $(TEST_APPS); do $(RUN) ./$$e $(TEST_RUN_FLAGS); done
	$(GCOV) $(GCOV_FLAGS) $(TEST_OBJECTS)
