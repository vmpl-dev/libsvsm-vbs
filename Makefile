CC = gcc
CFLAGS = -fPIC -Wall -Wextra -I../vmpl-dev
LDFLAGS = -shared -ldl

TARGET = libxom.so libxomy.so test
SRCS = libxom.c libxomy.c test.c
OBJS = $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)

libxom.so: libxom.o
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

libxomy.so: libxomy.o
	$(CC) $(CFLAGS) -o $@ $< -shared -ldl -lxom -L. -Wl,-rpath,.

test: test.o libxom.o
	$(CC) $(CFLAGS) -o $@ $^ -lxom -L. -Wl,-rpath,.

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)