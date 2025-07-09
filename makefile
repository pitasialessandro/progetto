# Variabili
JAVAC = javac
SRC = *.java

CC = gcc
CFLAGS = -Wall -g -O3 -std=c11 -pthread

all: compilejava compilec

compilec: cammini.out

compilejava:
	$(JAVAC) $(SRC)

cammini.out: cammini.o
	$(CC) $(CFLAGS) -o cammini.out cammini.o

cammini.o: cammini.c
	$(CC) $(CFLAGS) -c cammini.c -o cammini.o

clean:
	rm -f *.class *.o cammini.out

run:
	cammini.out nomi.txt grafo.txt 4

valgrind:
	valgrind --leak-check=full cammini.out nomi.txt grafo.txt 4