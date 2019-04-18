/**
 * @author Petr Mičulek
 * @date 27. 4. 2019
 * @title IOS Projekt 2 - Synchronizace procesů 
 * @desc River crossing problem implementation in C,
 * 			using processes, semaphores
 * */

/// @TODO Check results of system calls

// ./proj2 2 2 2 200 200 5
// ./proj2 6 0 0 200 200 5

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#include <semaphore.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#include "error_msg.h"

#define barrier_shm_name "/xplagiat00b-barrier_shm_name" // notice the s HHHH m
#define barrier_sem_name "/xplagiat00b-barrier_sem_name" // notice the s EEEE m
#define barrier_mutex_name "/xplagiat00b-barrier_mutex_name"

#define sync_shm_name "/xplagiat00b-sync_shm_name"
#define sync_hacker_queue_name "/xplagiat00b-sync_hacker_queue_name"
#define sync_serf_queue_name "/xplagiat00b-sync_serf_queue_name"
#define sync_mutex_name "/xplagiat00b-sync_mutex_name"

// @TODO Implement or Discard macros
#define LOCKED 0
#define BARRIER_BUFSIZE 1
#define SYNC_BUFSIZE 3 // action_count, hacker_count, serf_count
#define BARRIER_shm_SIZE sizeof(int)*BARRIER_BUFSIZE
#define SYNC_shm_SIZE sizeof(int)*SYNC_BUFSIZE

#define ACTION 0
#define HACK 1
#define SERF 2

#define BOAT_CAPACITY 4
#define SEM_ACCESS_RIGHTS 0644

#define PRINT_ERRNO_IF_SET() do { if(errno != 0) { warning_msg("errno = %d\n", errno); } else { printf("errno OK\n");} } while(0)

struct Barrier_t {
	sem_t *barrier_sem; // init 0
	sem_t *barrier_mutex; // init 1
	int *barrier_shm; // count; How to init?
	int barrier_shm_size;
	int barrier_shm_fd;
} ;
typedef struct Barrier_t barrier_t;
	
struct Sync_t {
	barrier_t* sync_barrier;
	sem_t* sync_mutex; // init 1
	sem_t* sync_hacker_queue; // init 0
	sem_t* sync_serf_queue; // init 0
	int *sync_shm; // hacker_count, serf_count
	//int *action_count_shm;
	int sync_shm_size;
	int sync_shm_fd;
} ;
typedef struct Sync_t sync_t;

/**
 *
 * @param str string to parse from
 * @return valid integer value from string if the format is valid
 *		 -1 on invalid format
 */
int parse_int(char *str);

// @TODO f-doc
void print_help();

void DEBUG_print_args(int argc, const int *arguments);

int barrier_init(barrier_t *barrier);
int sync_init(sync_t *sync1);

int barrier_destroy(barrier_t *barrier);
int sync_destroy(sync_t *sync1);

void hacker_routine(sync_t* sync);
void serf_routine(sync_t* sync);


int main(int argc, char **argv)
{
	int return_value = 0;
	int arguments[6] = {0};
	if(argc == 7)
	{
		for(unsigned int i = 1; i < (unsigned int)argc; i++)
		{
			arguments[i - 1] = parse_int(argv[i]);
		}

		DEBUG_print_args(argc, arguments);

		// init
		FILE* fp;
		if ((fp = fopen("rivercrossing.out", "w+")) == NULL){
        warning_msg("%s: opening output file","main");
        return 1;
    	}

		setbuf(stdout, NULL);
		setbuf(stderr, NULL);
		setbuf(fp, NULL);

		barrier_t barrier1;
		if(barrier_init(&barrier1) == -1)
		{
			warning_msg("barrier_init\n");

		}

		sync_t sync1;
		if(sync_init(&sync1)){
			warning_msg("sync_init\n");
		}

/*
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
					fprintf(stdout, "process: %d\n", getpid());
					usleep(4000);
					exit(0);
				}
			}
			usleep(1000);
		}
*/
		// @TODO wait for processes to end


        if(barrier_destroy(&barrier1))
        {
            warning_msg("barrier_destroy\n");
            return_value = 1;
        }

		if(sync_destroy(&sync1))
		{
            warning_msg("sync_destroy\n");
            return_value = 1;
		}

		if (fclose(fp) == -1)
    	{
			warning_msg("%s: closing file", "main");
			return_value = 1;
			
    	}

	}
	else
	{
		print_help();
		return 1;
	}
	return return_value;
}

void hacker_routine(sync_t* sync)
{
	bool is_captain = 0;
	sem_wait(sync->sync_mutex);
	sync->sync_shm[HACK]++;
	if(sync->sync_shm[HACK] == 4)
	{
		sem_post(sync->sync_hacker_queue);
		sem_post(sync->sync_hacker_queue);
		sem_post(sync->sync_hacker_queue);
		sem_post(sync->sync_hacker_queue);
		sync->sync_shm[HACK] = 0;
		// declare current process captain @TODO
		is_captain = 1;
	}
	else if (sync->sync_shm[HACK] == 2 && sync->sync_shm[SERF] >= 2) {
		sem_post(sync->sync_hacker_queue);
		sem_post(sync->sync_hacker_queue);

		sem_post(sync->sync_serf_queue);
		sem_post(sync->sync_serf_queue);

		sync->sync_shm[SERF] -= 2;
		sync->sync_shm[HACK] = 0;
		
		is_captain = 1;
	}
	else{
		sem_post(sync->sync_mutex);
	}

	if(is_captain)
	{
		(void)is_captain; // @TODO remove
	}

}

void serf_routine(sync_t* sync)
{
	// symmetrical, swap vars
	if(sync)
		fprintf(stdout, "SERF\n");
}

void DEBUG_print_args(int argc, const int *arguments) {
	for (unsigned int j = 1; j < (unsigned int) argc; j++)
	{
		printf("%d, ", arguments[j - 1]);
	}
	puts("\n");
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

	errno = 0;

	if((barrier->barrier_shm_fd = shm_open(barrier_shm_name, O_CREAT | O_EXCL | O_RDWR, 0644 )) == -1)
	{
		// shm_open returns -1 on error
		warning_msg("%s: Error initializing shared mem\n", "barrier");
	    PRINT_ERRNO_IF_SET();
		return -1;
	}

	// printf("fd: %d;\n", barrier->barrier_shm_fd);


	// printf("%d, %d, %d,%d, %d, %d, %d \n", EACCES, EEXIST, EINVAL, EMFILE, ENAMETOOLONG, ENFILE, ENOENT);


	errno = 0;
	if((ftruncate(barrier->barrier_shm_fd, BARRIER_shm_SIZE)) == -1)
	{
		warning_msg("%s: Error truncating shared mem", "barrier");
		PRINT_ERRNO_IF_SET();
		return -1;

	}

    errno = 0;
	if((barrier->barrier_shm = (int*)mmap(NULL, BARRIER_shm_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, barrier->barrier_shm_fd, 0)) == MAP_FAILED)
	{
		PRINT_ERRNO_IF_SET();
		warning_msg("%s: Error mapping shared mem\n", "barrier");
		return -1;
	}
	
	// *shm = 0
	
	*(barrier->barrier_shm) = 0; // @TODO Do I really init(barrier, 0)

	if((barrier->barrier_mutex = sem_open(barrier_mutex_name, O_CREAT, 0644, 1)) == SEM_FAILED)
	{
		warning_msg("%s,%s: Error initializing semaphore\n", "barrier", "1");
		return -1;
	}

	if((barrier->barrier_sem = sem_open(barrier_sem_name, O_CREAT, 0644, LOCKED)) == SEM_FAILED)
	{
		warning_msg("%s,%s: Error initializing semaphore\n", "barrier", "2");
		return -1;
	}



	return 0;
}

int sync_init(sync_t *sync1)
{
    //int sync_shmID;

	if(!(sync1->sync_shm_fd = shm_open(sync_shm_name, O_CREAT | O_EXCL | O_RDWR, S_IRUSR))){
		// shm_open returns -1 on error
		warning_msg("%s: Error initializing shared mem\n", "sync");
		return -1;
	}

	
	if(ftruncate(sync1->sync_shm_fd , SYNC_shm_SIZE) == -1) {
		warning_msg("%s: Error truncating shared mem\n", "sync");
		return -1;
	}

	if((sync1->sync_shm = (int*)mmap(NULL, SYNC_shm_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, sync1->sync_shm_fd, 0)) == MAP_FAILED) {
		warning_msg("%s: Error mapping shared mem\n", "sync");
		return -1;
	}
	

	if((close(sync1->sync_shm_fd )) == -1) {
		warning_msg("%s: Error closing shared mem\n", "sync");
		return -1;
	}

	sync1->sync_shm[0] = 0; // hacker_count
	sync1->sync_shm[1] = 0; // serf_count
	sync1->sync_shm[2] = 0; // action_count

	if((sync1->sync_mutex = sem_open(sync_mutex_name, O_CREAT, 0644, 1)) == SEM_FAILED)
	{
		warning_msg("%s,%s: Error initializing semaphore\n", "sync", "1");
		return -1;
	}
	//sem_close(sync1->sync_mutex);


	if((sync1->sync_hacker_queue = sem_open(sync_hacker_queue_name, O_CREAT, 0644, 0)) == SEM_FAILED)
	{
		warning_msg("%s,%s: Error initializing semaphore\n", "sync", "2");
		return -1;
	}
	//sem_close(sync1->sync_hacker_queue);

	if((sync1->sync_serf_queue = sem_open(sync_serf_queue_name, O_CREAT, 0644, 0)) == SEM_FAILED)
	{
		warning_msg("%s,%s: Error initializing semaphore\n", "sync", "3");
		return -1;
	}
	//sem_close(sync1->sync_serf_queue);


	return 0;
}

int barrier_destroy(barrier_t *barrier)  {
	// @TODO shared mem clear



	int return_value = 0;
	
	// sem*
	if((sem_close(barrier->barrier_mutex) |
		sem_close(barrier->barrier_sem))
		== -1)
	{
		warning_msg("%s: closing semaphores\n", "barrier");
		return_value = -1;
	}

	// name
	if((sem_unlink(barrier_mutex_name) |
		sem_unlink(barrier_sem_name))
		== -1)
	{
		warning_msg("%s: unlinking semaphores\n", "barrier");
		return_value = -1;
	}

	// address, length
	if(munmap(barrier->barrier_shm, BARRIER_shm_SIZE) == -1)
	{
		warning_msg("%s: unmapping memory\n", "barrier");
		return_value = -1;
	}
	
	// name
	if(shm_unlink(barrier_shm_name) == -1){
		warning_msg("%s: unlinking memory\n", "barrier");
		return_value = -1;
	}
    barrier->barrier_shm_fd = -1;
    barrier->barrier_shm_size = 0;

	return return_value;
}



int sync_destroy(sync_t *sync1)  {
   
	int return_value = 0;
	if((sem_close(sync1->sync_hacker_queue) |
		sem_close(sync1->sync_serf_queue) |
		sem_close(sync1->sync_mutex))
		== -1)
	{
		warning_msg("%s: closing semaphores\n", "sync");
		return_value = -1;
	}


	if((sem_unlink(sync_hacker_queue_name) |
		sem_unlink(sync_serf_queue_name) |
		sem_unlink(sync_mutex_name) )
		== -1)
	{
		warning_msg("%s: unlinking semaphores\n", "sync");
		return_value = -1;
	}

	if(munmap(sync1->sync_shm, SYNC_shm_SIZE) == -1)
	{
		warning_msg("%s: unmapping memory\n", "sync");
		return_value = -1;
	}

	if(shm_unlink(sync_shm_name) == -1){
		warning_msg("%s: unlinking memory\n", "sync");
		return_value = -1;
	}

	sync1->sync_shm_fd = -1;
	sync1->sync_shm_size = 0;


	return return_value;
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
