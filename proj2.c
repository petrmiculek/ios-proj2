/**
 * @author Petr Mičulek
 * @date 27. 4. 2019
 * @title IOS Projekt 2 - Synchronizace procesů 
 * @desc River crossing problem implementation in C,
 * 			using processes, semaphores
 * */

/// @TODO Check results of system calls


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>

#include "error_msg.h"

#define barrier_shm_name "/xplagiat00b-barrier_shm_name"
#define barrier_sem_name "/xplagiat00b-barrier_sem_name"
#define barrier_mutex_name "/xplagiat00b-barrier_mutex_name"

#define sync_sem_name "/xplagiat00b-sync_sem_name"
#define sync_mutex_name "/xplagiat00b-sync_mutex_name"
#define LOCKED 0


#define BOAT_CAPACITY 4
#define SEM_ACCESS_RIGHTS 0644

struct Barrier_t {
    sem_t *barrier_sem; // init 0
    sem_t *barrier_mutex; // init 1
    int *barrier_shm; // how big? ---  could be count
    int barrier_size;
} ;
typedef struct Barrier_t barrier_t;

struct Sync_t {
    barrier_t* sync_barrier;
    sem_t* sync_mutex; // init ?
    sem_t* sync_hacker_queue; // init ?
    sem_t* sync_serf_queue; // init ?
    int *sync_shm; // hacker_count, serf_count, isCaptain per process
    int sync_shm_size;
    int sync_hacker_count;
    int sync_serfs_count;
    
    //bool is_captain; // @TODO per process
} ;
typedef struct Sync_t sync_t;

/**
 *
 * @param str string to parse from
 * @return valid integer value from string if the format is valid
 *         -1 on invalid format
 */
int parse_int(char *str);

// @TODO f-doc
void print_help();

void DEBUG_print_args(int argc, const int *arguments);

int barrier_init(barrier_t *barrier);
int sync_init(sync_t *sync1);

void barrier_unlink();
void sync_unlink();



int main(int argc, char **argv)
{
	int arguments[6] = {0};
	if(argc == 7)
	{
		for(unsigned int i = 1; i < (unsigned int)argc; i++)
		{
			arguments[i - 1] = parse_int(argv[i]);
		}

        DEBUG_print_args(argc, arguments);

        setbuf(stdout,NULL);
        setbuf(stderr,NULL);

        // vytvorime a inicializujeme barieru
        barrier_t barrier1;
        barrier_init(&barrier1);

        sync_t sync1;
	    sync_init(&sync1);

        int pid;

        for (int j = 0; j < 2; j++) {
            for (int i = 1; i <= 5; i++) {
                if ((pid = fork()) < 0) {
                    perror("fork");
                    //b_unlink();
                    exit(2);
                }
                if (pid == 0) {
                    //proc2(j*10+i);
                    exit(0);
                }
            }
            usleep(1000);
        }

        // "cekani na skonceni vsech procesu"

        // barrier_unlink();

    }
	else
	{
		print_help();
		return 1;
	}
	return 0;
}

void hacker_routine(sync_t* sync)
{
    sem_wait(sync->sync_mutex);
    sync->sync_hacker_count++;
    if(sync->sync_hacker_count == 4)
    {
        sem_post(sync->sync_hacker_queue);
        sem_post(sync->sync_hacker_queue);
        sem_post(sync->sync_hacker_queue);
        sem_post(sync->sync_hacker_queue);
        sync->sync_hacker_count = 0;
        // declare current process captain @TODO test
        (sync->sync_shm)[ getpid() ] = 1;

    }
    else if (sync->sync_hacker_count == 2 && sync->sync_serfs_count == 2) {
        sem_post(sync->sync_hacker_queue);
        sem_post(sync->sync_hacker_queue);

        sem_post(sync->sync_serf_queue);
        sem_post(sync->sync_serf_queue);

        sync->sync_serfs_count -= 2;
        sync->sync_hacker_count = 0;

        (sync->sync_shm)[ getpid() ] = 1;
    }
    else{
        sem_post(sync->sync_mutex);
    }

}

void serf_routine()
{
    // symmetrical, swap vars
}

void DEBUG_print_args(int argc, const int *arguments) {
    for (unsigned int j = 1; j < (unsigned int) argc; j++)
    {
        printf("%d, ", arguments[j - 1]);
    }
}


int parse_int(char *str)
{
	char *end_ptr = NULL;
	long val = strtol(str, &end_ptr, 10);

	errno = 0;
	if (errno == ERANGE || strlen(str) == 0)
	{
		fprintf(stderr, "parsing\n");
		errno = 0;
		return -1;
	}

	if (val < 0)
	{
		warning_msg("parsed value %s < 0\n", str);
	}



	return (int)val;
}

int barrier_init(barrier_t *barrier)
{
    int shmID;
    errno = 0;
    if(!(shmID = shm_open(barrier_shm_name, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR)))
    {
        // shm_open returns -1 on error
        warning_msg("%s: Error initializing shared mem", "barrier");
        return -1;
    }

    ftruncate(shmID, sizeof(int));
    errno = 0;
    if((barrier->barrier_shm = (int*)mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shmID, 0)) == MAP_FAILED)
    {
        warning_msg("%s: Error mapping shared mem", "barrier");
        return -1;
    }

    close(shmID);
    *(barrier->barrier_shm) = 0; // @TODO why?
    munmap(barrier->barrier_shm, sizeof(int));

    errno = 0;
    if((barrier->barrier_mutex = sem_open(barrier_mutex_name, O_CREAT, 0644, 1)) == SEM_FAILED)
    {
        warning_msg("%s,%s: Error initializing semaphore", "barrier", "1");
        return -1;
    }

    errno = 0;
    if((barrier->barrier_sem = sem_open(barrier_sem_name, O_CREAT, 0644, 0)) == SEM_FAILED)
    {
        warning_msg("%s,%s: Error initializing semaphore", "barrier", "2");
        return -1;
    }


    sem_close(barrier->barrier_mutex);
    sem_close(barrier->barrier_sem);
    return 0;
}

int sync_init(sync_t *sync1)
{
    int sync_shmID;
    if(!(sync_shmID = shm_open(sync_sem_name, O_CREAT | O_EXCL | O_RDWR, S_IRUSR))){
        // shm_open returns -1 on error
        warning_msg("%s: Error initializing shared m em", "sync");
        return -1;
    }

    ftruncate(sync_shmID, sizeof(int));

    errno = 0;
    if((sync1->sync_shm = (int*)mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, sync_shmID, 0)) == MAP_FAILED) {
        warning_msg("%s: Error mapping shared mem", "sync");
        return -1;
    }

    close(sync_shmID);

    if(!(munmap(sync1->sync_shm, sizeof(int)))) {
        warning_msg("%s: Error un-mapping shared mem", "sync");
        return -1;
    }


    return 0;
}
/*
int synchronization_init(barrier_t* barrier, sync_t* sync)
{
    int barrier_shm_ID;
    int sync_shm_ID;

    errno = 0;
    if((barrier_shm_ID = shm_open(barrier_shm_name, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR))
        ||(sync_shm_ID = shm_open(sync_sem_name, O_CREAT | O_EXCL | O_RDWR, S_IRUSR))
                       == -1)
    {
        // shm_open returns -1 on error
        warning_msg("%s: Error initializing shared mem", "1");
        return -1;
    }


}
*/

void barrier_unlink()  {
    // @TODO shared mem unlink
    // shm_unlink(shmNAME);
    sem_unlink(barrier_mutex_name);
    sem_unlink(barrier_sem_name);
}



void sync_unlink()  {
   /*
    shm_unlink(shmNAME);
    sem_unlink(semMUTEXNAME);
    sem_unlink(semNAME);
    */
}

void print_help()
{
	printf("run like\n"
		   " ./proj2 P H S R W C\n"
		   "	P - Persons\n"
		   "	H - Hackers generation duration\n"
		   "	S - Serfs generation duration\n"
		   "	R - Rowing duration\n"
		   "	W - Return time\n"
		   "	C - Waiting queue (pier) capacity\n");
}
