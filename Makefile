# autor: Petr Miculek xmicul08@stud.fit.vutbr.cz
# nazev: IOS - Projekt 2
# datum: odevzdani do 28. 4. 2019

# @TODO delete this
#	If you are getting SIGSEGV, try:
#		rm /dev/shm/sem.semaphore_synchronization1 /dev/shm/sem.semaphore_synchronization2



CC=gcc
CFLAGS=-std=gnu99 -Wall -Wextra -Werror -pedantic
LFLAGS=-lrt -lpthread # @TODO needed?
BIN=proj2
SOURCES= proj2.c error_msg.c
TESTS=check_syntax.sh
RM=rm
ARGS=6 0 0 200 200 5

.PHONY: clean

all:
	$(CC) $(CFLAGS) $(SOURCES) -o $(BIN) $(LFLAGS)

clean:
	$(RM) -f *.o core *.out

cleanall: clean
	$(RM) $(BIN)

zip: *.c *.h Makefile
	zip xmicul08.zip *.c *.h Makefile

leaks: $(BIN)
	valgrind --track-origins=yes --leak-check=full --leak-resolution=med --track-origins=yes --vgdb=no --show-reachable=yes --trace-children=yes ./$(BIN) $(ARGS) $(CMDLINE)


test1: all
	./$(BIN) 2 2 2 200 200 5

test2: all
	./$(BIN) $(ARGS)

tests: all
	./$(TESTS) $(BIN)

# delete shared memory objects that were not cleaned during runtime
manual-cleanup:
	./cleanup.sh $(CMDLINE)