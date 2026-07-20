# MAC-C - host-side MAC assembler for the NORD TSS sources
# Build:  make          -> mac-as (driver) + test_mac (unit tests)
#         make test     -> build and run the unit test suite
#         make clean

CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -O2
OBJS    = mac.o mac_permsym.o

all: mac-as test_mac

mac.o: mac.c mac.h
	$(CC) $(CFLAGS) -c mac.c -o mac.o

mac_permsym.o: mac_permsym.c mac.h
	$(CC) $(CFLAGS) -c mac_permsym.c -o mac_permsym.o

mac-as: $(OBJS) main.o
	$(CC) $(CFLAGS) -o mac-as $(OBJS) main.o

main.o: main.c mac.h
	$(CC) $(CFLAGS) -c main.c -o main.o

test_mac: $(OBJS) test_mac.o
	$(CC) $(CFLAGS) -o test_mac $(OBJS) test_mac.o

test_mac.o: test_mac.c mac.h
	$(CC) $(CFLAGS) -c test_mac.c -o test_mac.o

test: test_mac
	./test_mac

clean:
	rm -f *.o mac-as test_mac mac_test_*.bin mac_test_*.bpun mac_test_*.symb

.PHONY: all test clean
