/**
 * @author Petr Mičulek
 * @date 27. 4. 2019
 * @title IOS Projekt 2 - Synchronizace procesů 
 * @desc River crossing problem implementation in C,
 * 			using processes, semaphores
 *
 * */


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
#include "proj2.h"




int main(int argc, char **argv)
{
    // ######## ARGUMENTS CHECKING ########

	int return_value = 0;
	int arguments[ARGS_COUNT] = {0};

    if(argc != (ARGS_COUNT + 1))
    {
        print_help();
        return 1;
    }

    for(unsigned int i = 1; i < (unsigned int)argc; i++)
    {
        arguments[i - 1] = parse_int(argv[i]);
    }

    DEBUG_print_args(argc, arguments);

    // @TODO argument test







    // ######## INITIALIZING ########
    FILE* fp;
#ifdef NDEBUG
    if ((fp = fopen("rivercrossing.out", "w+")) == NULL){
    warning_msg("%s: opening output file","main");
    return 1;
    }

    setbuf(fp, NULL);
#else
    fp = stdout;
#endif

    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    barrier_t barrier1;
    if(barrier_init(&barrier1) == -1)
    {
        warning_msg("barrier_init\n");

    }

    sync_t sync1;
    if(sync_init(&sync1)){
        warning_msg("sync_init\n");
    }
    sync1.p_barrier = &barrier1;






    // ######## FORKING ########


    pid_t pid_hack_gen = fork();
    if (pid_hack_gen == 0)
    {
        if (generate_hackers(&sync1, arguments, fp) == -1)
        {
            warning_msg("%s: generating hackers\n", "main");
            return_value = -1;
        }
    }
    else if (pid_hack_gen < 0)
    {
        warning_msg("%s: forking hackers\n", "main");
        return_value = -1;
    }



    pid_t pid_serf_gen = fork();
    if (pid_serf_gen == 0)
    {
        if (generate_serfs(&sync1, arguments, fp) == -1)
        {
            warning_msg("%s: generating serfs\n", "main");
            return_value = -1;
        }
    }
    else if (pid_serf_gen < 0)
    {
        warning_msg("%s: forking serfs\n", "main");
        return_value = -1;
    }



    // ######## CLEANING UP ########

    for (int j = 0; j < (2 + (2 * arguments[OF_EACH_TYPE])); ++j) {
        wait(NULL);
    }

    if(pid_hack_gen && pid_serf_gen)
    {
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

	return return_value;
}




int generate_hackers(sync_t *p_shared, const int arguments[ARGS_COUNT], FILE *fp) {

    pid_t pid_hacker;

    for (int i = 0; i < arguments[OF_EACH_TYPE]; ++i) {

        sleep_up_to(arguments[TIME_HACK_GEN]);

        pid_hacker = fork();

        if (pid_hacker == 0)
        {

            hacker_routine(p_shared, arguments, fp);
        }

        else if (pid_hacker < 0)
        {
            warning_msg("%s: fork fail","hacker_gen");
            return -1;
        }

    }

    waitpid(-1, NULL, 0);
    // exit(0);
    return 0;
}

int generate_serfs(sync_t *p_shared, const int arguments[ARGS_COUNT], FILE* fp) {
    pid_t pid_serf;

    for (int i = 0; i < arguments[OF_EACH_TYPE]; ++i) {

        sleep_up_to(arguments[TIME_SERF_GEN]);

        pid_serf = fork();

        if (pid_serf == 0) {

            serf_routine(p_shared, arguments, fp);
        } else if (pid_serf < 0) {
            warning_msg("%s: fork fail", "serf_gen");
            return -1;
        }

    }

    waitpid(-1, NULL, 0);
    // exit(0);
    return 0;
}


void hacker_routine(sync_t *p_shared, const int arguments[ARGS_COUNT], FILE *fp)
{
	// @TODO safely increment [HACK_TOTAL]

	sem_wait(p_shared->mutex);


	sem_wait(p_shared->mem_lock);
    p_shared->shared_mem[HACK_TOTAL]++;
	int intra_hacker_order = p_shared->shared_mem[HACK_TOTAL];
	sem_post(p_shared->mem_lock);

	bool is_captain = 0;

	p_shared->shared_mem[HACK]++;

	if(p_shared->shared_mem[HACK] == 4)
	{
		sem_post(p_shared->hacker_queue);
		sem_post(p_shared->hacker_queue);
		sem_post(p_shared->hacker_queue);
		sem_post(p_shared->hacker_queue);
		p_shared->shared_mem[HACK] = 0; // hackers 4 -> 0

		is_captain = 1;
	}
	else if (p_shared->shared_mem[HACK] == 2 && p_shared->shared_mem[SERF] >= 2)
	{
		sem_post(p_shared->hacker_queue);
		sem_post(p_shared->hacker_queue);

		sem_post(p_shared->serf_queue);
		sem_post(p_shared->serf_queue);

		p_shared->shared_mem[SERF] -= 2;
		p_shared->shared_mem[HACK] = 0;
		
		is_captain = 1;
	}
	else
    {
		sem_post(p_shared->mutex); // not enough passengers, release mutex
	}

	sem_wait(p_shared->hacker_queue);

	//board(HACK, p_shared); // not needed?


    if(is_captain)
    {
        row_boat(p_shared, arguments, fp, HACK, intra_hacker_order); // "boards"
    }

    reusable_barrier(p_shared->p_barrier); // everyone meets up at the barrier

    if(!is_captain)
    {
        print_action(fp, p_shared, HACK, "member exits", 0);
    }


    reusable_barrier(p_shared->p_barrier);

    if(is_captain)
	{
        print_action(fp, p_shared, HACK, "captain exits", 0);
		sem_post(p_shared->mutex);
	}

    // processes exit
}

void serf_routine(sync_t *p_shared, const int arguments[ARGS_COUNT], FILE *fp)
{
    (void) p_shared;
    (void) arguments;
    (void) fp;

    // symmetrical, swap vars
    // bool is_captain = 0;


    //sem_wait(p_shared->mutex);

}


void reusable_barrier(const barrier_t *p_barrier) {
    // indentation shows "scope" of mutex lock
    sem_wait(p_barrier->barrier_mutex);

        *(p_barrier->barrier_shm) += 1;
        if (*(p_barrier->barrier_shm) == BOAT_CAPACITY) {
            sem_wait(p_barrier->turnstile2);
            sem_post(p_barrier->turnstile1);
        }

    sem_post(p_barrier->barrier_mutex);

    sem_wait(p_barrier->turnstile1);
    sem_post(p_barrier->turnstile1);

    sem_wait(p_barrier->barrier_mutex);

        *(p_barrier->barrier_shm) -= 1;
        if (*(p_barrier->barrier_shm) == 0) {
            sem_wait(p_barrier->turnstile1);
            sem_post(p_barrier->turnstile2);
        }

    sem_post(p_barrier->barrier_mutex);

    sem_wait(p_barrier->turnstile2);
    sem_post(p_barrier->turnstile2);
}


void row_boat(sync_t *p_shared, const int arguments[6], FILE *fp, int role, int intra_role_order)
{
    print_action(fp, p_shared, role, "boards", intra_role_order);
    sleep_up_to(arguments[TIME_BOAT]);
}

void sleep_up_to(int maximum_sleep_time)
{
    srand(time(NULL) * getpid());
    usleep((random() % (maximum_sleep_time + 1)) * 1000);
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
	if (errno == ERANGE || strlen(str) == 0) {
	    // @TODO test end_ptr
		fprintf(stderr, "parsing\n");
		errno = 0;
		return -1;
	}

	if (val < 0) {
		warning_msg("parsed value %s < 0\n", str);
	}



	return (int)val;
}

int barrier_init(barrier_t *p_barrier)
{

	if((p_barrier->barrier_shm_fd = shm_open(barrier_shm_name, O_CREAT | O_EXCL | O_RDWR, 0644 )) == -1) {
		warning_msg("%s: initializing shared mem\n", "p_barrier");
		return -1;
	}

	if((ftruncate(p_barrier->barrier_shm_fd, BARRIER_shm_SIZE)) == -1) {
		warning_msg("%s: truncating shared mem", "p_barrier");
		return -1;
	}

	if((p_barrier->barrier_shm = (int*)mmap(NULL, BARRIER_shm_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, p_barrier->barrier_shm_fd, 0)) == MAP_FAILED) {
		warning_msg("%s: mapping shared mem\n", "p_barrier");
		return -1;
	}

	if((p_barrier->barrier_mutex = sem_open(barrier_mutex_name, O_CREAT, 0644, 1)) == SEM_FAILED) {
		warning_msg("%s,%s: initializing semaphore\n", "p_barrier", "1");
		return -1;
	}

	if((p_barrier->turnstile1 = sem_open(barrier_turnstile1_name, O_CREAT, 0644, LOCKED)) == SEM_FAILED) {
		warning_msg("%s,%s: initializing semaphore\n", "p_barrier", "2");
		return -1;
	}

    if((p_barrier->turnstile2 = sem_open(barrier_turnstile2_name, O_CREAT, 0644, LOCKED)) == SEM_FAILED) {
        warning_msg("%s,%s: initializing semaphore\n", "p_barrier", "3");
        return -1;
    }

    *(p_barrier->barrier_shm) = 0;

	return 0;
}

int sync_init(sync_t *p_shared)
{
    //int sync_shmID;

	if(!(p_shared->shared_mem_fd = shm_open(sync_shm_name, O_CREAT | O_EXCL | O_RDWR, S_IRUSR))){
		warning_msg("%s: initializing shared mem\n", "sync");
		return -1;
	}

	
	if(ftruncate(p_shared->shared_mem_fd , SYNC_shm_SIZE) == -1) {
		warning_msg("%s: truncating shared mem\n", "sync");
		return -1;
	}

	if((p_shared->shared_mem = (int*)mmap(NULL, SYNC_shm_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, p_shared->shared_mem_fd, 0))
	    == MAP_FAILED) {
		warning_msg("%s: mapping shared mem\n", "sync");
		return -1;
	}
	

	if((close(p_shared->shared_mem_fd )) == -1) {
		warning_msg("%s: closing shared mem\n", "sync");
		return -1;
	}



	if((p_shared->mutex = sem_open(sync_mutex_name, O_CREAT, 0644, 1)) == SEM_FAILED) {
		warning_msg("%s,%s: initializing semaphore\n", "sync", "1");
		return -1;
	}


    if((p_shared->mem_lock = sem_open(sync_mem_lock_name, O_CREAT, 0644, 1)) == SEM_FAILED) {
        warning_msg("%s,%s: initializing semaphore\n", "sync", "2");
        return -1;
    }

	if((p_shared->hacker_queue = sem_open(sync_hacker_queue_name, O_CREAT, 0644, 0)) == SEM_FAILED) {
		warning_msg("%s,%s: initializing semaphore\n", "sync", "3");
		return -1;
	}


	if((p_shared->serf_queue = sem_open(sync_serf_queue_name, O_CREAT, 0644, 0)) == SEM_FAILED) {
		warning_msg("%s,%s: initializing semaphore\n", "sync", "4");
		return -1;
	}


	p_shared->shared_mem[ACTION] = 1;
    p_shared->shared_mem[HACK] = 0;
    p_shared->shared_mem[SERF] = 0;
    p_shared->shared_mem[HACK_TOTAL] = 0;
    p_shared->shared_mem[SERF_TOTAL] = 0;

	return 0;
}

int barrier_destroy(barrier_t *p_barrier)  {

	int return_value = 0;
	
	// sem*
	if((sem_close(p_barrier->barrier_mutex) |
		sem_close(p_barrier->turnstile1)|
		sem_close(p_barrier->turnstile2))
		== -1)
	{
		warning_msg("%s: closing semaphores\n", "p_barrier");
		return_value = -1;
	}

	// name
	if((sem_unlink(barrier_mutex_name) |
		sem_unlink(barrier_turnstile1_name)|
		sem_unlink(barrier_turnstile2_name))
		== -1)
	{
		warning_msg("%s: unlinking semaphores\n", "p_barrier");
		return_value = -1;
	}

	// address, length
	if(munmap(p_barrier->barrier_shm, BARRIER_shm_SIZE) == -1)
	{
		warning_msg("%s: unmapping memory\n", "p_barrier");
		return_value = -1;
	}
	
	// name
	if(shm_unlink(barrier_shm_name) == -1){
		warning_msg("%s: unlinking memory\n", "p_barrier");
		return_value = -1;
	}
    p_barrier->barrier_shm_fd = -1;
    p_barrier->barrier_shm_size = 0;

	return return_value;
}



int sync_destroy(sync_t *p_shared)  {
   
	int return_value = 0;
	if((sem_close(p_shared->hacker_queue) |
		sem_close(p_shared->serf_queue) |
		sem_close(p_shared->mem_lock) |
		sem_close(p_shared->mutex))
		== -1)
	{
		warning_msg("%s: closing semaphores\n", "sync");
		return_value = -1;
	}


	if((sem_unlink(sync_hacker_queue_name) |
		sem_unlink(sync_serf_queue_name) |
		sem_unlink(sync_mem_lock_name) |
		sem_unlink(sync_mutex_name) )
		== -1)
	{
		warning_msg("%s: unlinking semaphores\n", "sync");
		return_value = -1;
	}

	if(munmap(p_shared->shared_mem, SYNC_shm_SIZE) == -1)
	{
		warning_msg("%s: unmapping memory\n", "sync");
		return_value = -1;
	}

	if(shm_unlink(sync_shm_name) == -1){
		warning_msg("%s: unlinking memory\n", "sync");
		return_value = -1;
	}

	p_shared->shared_mem_fd = -1;
	p_shared->shared_mem_size = 0;


	return return_value;
}


void print_action(FILE *fp, sync_t *p_shared, int role, char *action_string, int intra_role_order) {
    // value update and read should be atomic
    sem_wait(p_shared->mem_lock);
        p_shared->shared_mem[ACTION]++;

    char* role_string = ((role == HACK) ? "HACK" : "SERF");

    fprintf(fp, "%-8d: %s %-10d: %-20s : %-8d : %d",
            p_shared->shared_mem[ACTION],
            role_string,
            intra_role_order,
            action_string,
            p_shared->shared_mem[HACK],
            p_shared->shared_mem[SERF]
    );

    sem_post(p_shared->mem_lock);

}

void print_help()
{
	printf("Run like:\n"
		   " ./proj2 P H S R W C\n"
		   "	P - Hackers count == Serfs count\n"
		   "	H - Hackers generation duration\n"
		   "	S - Serfs generation duration\n"
		   "	R - Rowing duration\n"
		   "	W - Return time\n"
		   "	C - Waiting queue (pier) capacity\n");
}
