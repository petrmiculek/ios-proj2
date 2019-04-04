# autor: Petr Miculek xmicul08@stud.fit.vutbr.cz
# nazev: IOS - Projekt 2
# datum: odevzdani do 28. 4. 2019

# @TODO delete this
#	If you are getting SIGSEGV, try:
#		rm /dev/shm/sem.semaphore_synchronization1 /dev/shm/sem.semaphore_synchronization2



CC=gcc
CFLAGS=-std=gnu99 -Wall -Wextra -Werror -pedantic
LFLAGS=-lpthread # @TODO needed?
BIN=proj2
SOURCES=proj2.c

.PHONY: clean

all:
	$(CC) $(CFLAGS) $(SOURCES) -o $(BIN) $(LFLAGS)

run: all
	./$(BIN)

clean:
	rm -f $(BIN) *.o

zip: *.c *.h Makefile
	zip xmicul08.zip *.c *.h Makefile

leaks: primes
	valgrind --track-origins=yes --leak-check=full --show-reachable=yes ./$(BIN) $(CMDLINE)
