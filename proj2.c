/**
 * @author Petr Mičulek
 * @date 27. 4. 2019
 * @title IOS Projekt 2 - Synchronizace procesů 
 * @desc River crossing problem implementation in C,
 * 			using processes, semaphores
 * */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>

#define barrier_sem_name "/barrier_sem_name"
#define barrier_mutex_name "/barrier_mutex_name"

#define sync_sem_name "/sync_sem_name"
#define sync_mutex_name "/sync_mutex_name"
#define LOCKED 0

typedef struct {
    sem_t *b_sem; // init 0
    sem_t *b_mutex; // init 1
    int *b_shm; // how big?
    int b_size;
} barrier_t;

typedef struct {
    barrier_t* s_barrier;
    sem_t* s_sync_mutex; // init ?
    sem_t* s_hacker_queue; // init ?
    sem_t* s_serf_queue; // init ?
    bool is_captain;
    int *s_shm; // hacker_count, serf_count, isCaptain per process
} sync_t;

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

void barrier_init(barrier_t *barrier);
void sync_init(sync_t *barrier);

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

        int pid;
        setbuf(stdout,NULL);
        setbuf(stderr,NULL);

        // vytvorime a inicializujeme barieru
        barrier_t barrier1;
        barrier_init(&barrier1);

        sync_t sync1;
	    sync_init(&sync1);


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

void hacker_routine()
{

}

void serf_routine()
{

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

	if (errno == ERANGE || strlen(str) == 0)
	{
		fprintf(stderr, "parsing\n");
		errno = 0;
		return -1;
	}

	if (val < 0)
	{
		fprintf(stderr, "val %s < 0\n", str);
	}



	return (int)val;
}

void barrier_init(barrier_t *barrier)
{
    int shmID;
    shmID = shm_open(barrier_sem_name, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
    ftruncate(shmID, sizeof(int));
    barrier->b_shm = (int*)mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shmID, 0);
    close(shmID);
    *(barrier->b_shm) = 0;
    munmap(barrier->b_shm, sizeof(int));

    barrier->b_mutex = sem_open(barrier_mutex_name, O_CREAT, 0666, 1);
    barrier->b_sem = sem_open(barrier_sem_name, O_CREAT, 0666, 0);
    sem_close(barrier->b_mutex);
    sem_close(barrier->b_sem);
}

void sync_init(sync_t *sync1)
{
    (void)sync1;

}


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
