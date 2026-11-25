CC ?= cc

.PHONY: clean

libknight.a: knight.o
	ar rcs libknight.a knight.o

knight.o: knight.c knight.h
	$(CC) -c knight.c -o knight.o -Wall -Wextra -pedantic -O2

clean:
	rm -f libknight.a knight.o
