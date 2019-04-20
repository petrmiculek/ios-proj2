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


void barrier_1(barrier_t *p_barrier);

void barrier_2(barrier_t *p_barrier);

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

    srand(time(NULL) * getpid());

    /// PLAYING ----- START
/*
    sleep_up_to(arguments[TIME_HACK_GEN]);
    sleep_up_to(arguments[TIME_SERF_GEN]);
    sleep_up_to(arguments[TIME_BOAT]);
    sleep_up_to(arguments[TIME_REQUEUE]);

    barrier_destroy(&barrier1);
    sync_destroy(&sync1);
    if(sync1.p_barrier == &barrier1)
        exit(12);
*/
    /// PLAYING ----- END




    // ######## FORKING ########

    pid_t pid_hack_gen = fork();
    if (pid_hack_gen == 0)
    {
        if (generate_passengers(&sync1, arguments, fp, HACK) == -1)
        {
            warning_msg("generating hackers\n");
            return_value = -1;
        }

        exit(0);
    }
    else if (pid_hack_gen < 0)
    {
        warning_msg("forking hack-gen\n");
        return_value = -1;
    }


    pid_t pid_serf_gen = fork();
    if (pid_serf_gen == 0)
    {
        if (generate_passengers(&sync1, arguments, fp, SERF) == -1)
        {
            warning_msg("generating serfs\n");
            return_value = -1;
        }

        exit(0);
    }
    else if (pid_serf_gen < 0)
    {
        warning_msg("forking serf-gen\n");
        return_value = -1;
    }




    // ######## CLEANING UP ########

    for (int j = 0; j < (2 + (2 * arguments[OF_EACH_TYPE])); ++j) {
        printf(":waiting for death:\n");
        wait(NULL);
    }

    if (pid_hack_gen > 0 && pid_serf_gen > 0)
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
            warning_msg("closing file\n");
            return_value = 1;

        }
    }

	return return_value;
}


int generate_passengers(sync_t *p_shared, const int arguments[ARGS_COUNT], FILE *fp, int role) {

    pid_t pid;
    srand(time(NULL) * getpid());
    int time_role_gen;

    if (role == HACK) {
        time_role_gen = TIME_HACK_GEN;
    } else {
        time_role_gen = TIME_SERF_GEN;

    }

    for (int i = 0; i < arguments[OF_EACH_TYPE]; ++i) {

        sleep_up_to(arguments[time_role_gen]);

        pid = fork();

        if (pid == 0)
        {
            // printf(":%d:child:hack:\n", getpid());// DEBUG
            passenger_routine(p_shared, arguments, fp, role);
        } else if (pid < 0)
        {
            warning_msg("fork fail for role: %d\n", role);
            return -1;
        }

    }

    waitpid(-1, NULL, 0); // @TODO Whose death does this wait for, /How many?/
    return 0;
}


void passenger_routine(sync_t *p_shared, const int arguments[6], FILE *fp, int role) {
    int intra_role_order;
    int role_total;
    sem_t *role_queue;

    int other_role;

    if (role == HACK) {
        other_role = SERF;
        role_total = HACK_TOTAL;
        role_queue = p_shared->hacker_queue;
    } else {
        other_role = HACK;
        role_total = SERF_TOTAL;
        role_queue = p_shared->serf_queue;
    }

    srand(time(NULL) * getpid());

    sem_wait(p_shared->mutex);
    // printf("%d got the mutex\n", getpid()); // DEBUG

	sem_wait(p_shared->mem_lock);
    //printf("\t%d got the mem-mutex\n", getpid()); // DEBUG

    p_shared->shared_mem[role]++;
    p_shared->shared_mem[role_total]++;

    intra_role_order = p_shared->shared_mem[role_total];

    print_action_plus_plus(fp, p_shared, role, intra_role_order, "starts");

	sem_post(p_shared->mem_lock);

    //printf("\t%d released the mem-mutex\n", getpid()); // DEBUG

	bool is_captain = 0;


    if (p_shared->shared_mem[role] == BOAT_CAPACITY) {
        sem_post(role_queue);
        sem_post(role_queue);
        sem_post(role_queue);
        sem_post(role_queue);
        p_shared->shared_mem[role] = 0; // role_count 4 -> 0

		is_captain = 1;
    } else if ((2 * p_shared->shared_mem[role] == BOAT_CAPACITY) &&
               (2 * p_shared->shared_mem[other_role] >= BOAT_CAPACITY)) {

        sem_post(p_shared->hacker_queue);
        sem_post(p_shared->hacker_queue);

        sem_post(p_shared->serf_queue);
        sem_post(p_shared->serf_queue);

        p_shared->shared_mem[other_role] -= (BOAT_CAPACITY / 2);
        p_shared->shared_mem[role] = 0;

        is_captain = 1;
    } else {
        sem_post(p_shared->mutex); // not enough passengers, release mutex
        //   printf("%d released the mutex\n", getpid()); // DEBUG

    }

    sem_wait(role_queue);


    if(is_captain)
    {
        //sem_post(p_shared->mutex);

        // captain needs to let go of the main mutex, so that other processes can queue at the pier
        // so row_boat uses mem_lock-mutex, instead
        row_boat(p_shared, arguments, fp, role, intra_role_order); // "boards"
        //sem_wait(p_shared->mutex);
    }

    barrier_1(p_shared->p_barrier);

    if(!is_captain)
    {

        //printf("\t%d waiting for memlock\n", getpid()); // DEBUG
        sem_wait(p_shared->mem_lock);
        //printf("\t%d taken memlock\n", getpid()); // DEBUG
        print_action_plus_plus(fp, p_shared, role, intra_role_order, "member exits");
        sem_post(p_shared->mem_lock);
        //printf("\t%d released memlock\n", getpid()); // DEBUG
    }

    barrier_2(p_shared->p_barrier);


    if(is_captain)
	{
        print_action_plus_plus(fp, p_shared, role, intra_role_order, "captain exits");
		sem_post(p_shared->mutex);
        // printf("%d released the mutex(cpt)\n", getpid()); // DEBUG

    }

    // processes exit
    exit(0);
}

void barrier_1(barrier_t *p_barrier) {
    //printf("\t%d waiting for barrier mutex1\n", getpid()); // DEBUG
    sem_wait(p_barrier->barrier_mutex);
    //printf("\t%d got the barrier mutex1\n", getpid()); // DEBUG

    *(p_barrier->barrier_shm) += 1;
    if (*(p_barrier->barrier_shm) == BOAT_CAPACITY) {
        sem_wait(p_barrier->turnstile2);
        sem_post(p_barrier->turnstile1);
    }

    //printf("\t%d releasing the barrier mutex1(%d)\n", getpid(), *(p_barrier->barrier_shm)); // DEBUG
    sem_post(p_barrier->barrier_mutex);


    //printf("\t%d waiting at t1\n", getpid()); // DEBUG
    sem_wait(p_barrier->turnstile1);
    sem_post(p_barrier->turnstile1);
    //printf("\t%d passed t1\n", getpid()); // DEBUG
}

void barrier_2(barrier_t *p_barrier) {

    //printf("\t%d waiting at b2-mutex-entry\n", getpid()); // DEBUG
    sem_wait(p_barrier->barrier_mutex);
    //printf("\t%d got the barrier mutex2\n", getpid()); // DEBUG

    *(p_barrier->barrier_shm) -= 1;
    if (*(p_barrier->barrier_shm) == 0) {
        sem_wait(p_barrier->turnstile1);
        sem_post(p_barrier->turnstile2);
    }

    //printf("\t%d releasing the barrier mutex2(%d)\n", getpid(), *(p_barrier->barrier_shm)); // DEBUG
    sem_post(p_barrier->barrier_mutex);

    //printf("\t%d waiting at t2\n", getpid()); // DEBUG
    sem_wait(p_barrier->turnstile2);
    sem_post(p_barrier->turnstile2);
    //printf("\t%d passed t2\n", getpid()); // DEBUG
}

/*
void serf_routine(sync_t *p_shared, const int arguments[ARGS_COUNT], FILE *fp)
{
    srand(time(NULL) * getpid());

    // ###################################### CTRL-V ###############################
    sem_wait(p_shared->mutex);
    // printf("serf %d got the mutex\n", getpid()); // DEBUG


    sem_wait(p_shared->mem_lock);

    p_shared->shared_mem[SERF]++;
    p_shared->shared_mem[SERF_TOTAL]++;

    int intra_serf_order = p_shared->shared_mem[SERF_TOTAL];
    print_action_plus_plus(fp, p_shared, SERF, intra_serf_order, "starts");
    sem_post(p_shared->mem_lock);

    bool is_captain = 0;


    if (p_shared->shared_mem[SERF] == 4) {
        sem_post(p_shared->serf_queue);
        sem_post(p_shared->serf_queue);
        sem_post(p_shared->serf_queue);
        sem_post(p_shared->serf_queue);
        p_shared->shared_mem[SERF] = 0; // hackers 4 -> 0

        is_captain = 1;
    } else if (p_shared->shared_mem[SERF] == 2 && p_shared->shared_mem[HACK] >= 2) {
        sem_post(p_shared->serf_queue);
        sem_post(p_shared->serf_queue);

        sem_post(p_shared->hacker_queue);
        sem_post(p_shared->hacker_queue);

        p_shared->shared_mem[HACK] -= 2;
        p_shared->shared_mem[SERF] = 0;

        is_captain = 1;
    } else {
        sem_post(p_shared->mutex); // not enough passengers, release mutex
        // printf("serf %d released the mutex\n", getpid()); // DEBUG
    }

    sem_wait(p_shared->serf_queue);


    if (is_captain) {
        // cpt should give up the mutex
        row_boat(p_shared, arguments, fp, SERF, intra_serf_order); // cpt "boards"
    }

    barrier_1(p_shared->p_barrier);

    if (!is_captain) {
        sem_wait(p_shared->mem_lock);

        print_action_plus_plus(fp, p_shared, SERF, intra_serf_order, "member exits");
        sem_wait(p_shared->mem_lock);

    }

    barrier_2(p_shared->p_barrier);

    if (is_captain) {
        print_action_plus_plus(fp, p_shared, SERF, intra_serf_order, "captain exits"); // inside mutex
        sem_post(p_shared->mutex);
        // printf("serf %d released the mutex(Cpt)\n", getpid());
    }
}
*/

void row_boat(sync_t *p_shared, const int arguments[6], FILE *fp, int role, int intra_role_order)
{
    //printf("\t%d waiting for memlock\n", getpid()); // DEBUG
    sem_wait(p_shared->mem_lock);
    //printf("\t%d taken memlock\n", getpid()); // DEBUG
    print_action_plus_plus(fp, p_shared, role, intra_role_order, "boards");
    //printf("\t%d releasing memlock\n", getpid()); // DEBUG
    sem_post(p_shared->mem_lock);

    sleep_up_to(arguments[TIME_BOAT]);
}

void sleep_up_to(int maximum_sleep_time) {

    useconds_t sleep_time = (random() % (maximum_sleep_time + 1)) * 1000;
    usleep(sleep_time);
    // printf(":%d slept for %f:\n", getpid(), sleep_time / 1000.0);
}

int parse_int(const char *str)
{
	char *end_ptr = NULL;
    long numerical_value = strtol(str, &end_ptr, 10);

	errno = 0;
	if (errno == ERANGE || strlen(str) == 0) {
	    // @TODO test end_ptr
        warning_msg("parsing\n");
		errno = 0;
		return -1;
	}

    if (numerical_value < 0) {
		warning_msg("parsed value %s < 0\n", str);
	}

    return (int) numerical_value;
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

    if ((p_barrier->turnstile2 = sem_open(barrier_turnstile2_name, O_CREAT, 0644, 1)) == SEM_FAILED) {
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


    p_shared->shared_mem[ACTION] = 0;
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

	return return_value;
}


void print_action_plus_plus(FILE *fp, sync_t *p_shared, int role, int intra_role_order, const char *action_string) {
    p_shared->shared_mem[ACTION]++;

    char* role_string = ((role == HACK) ? "HACK" : "SERF");

    fprintf(fp, "%-4d (%d)  : %s %-10d: %-20s : %-8d : %d\n",
            p_shared->shared_mem[ACTION],
            getpid(), // @TODO REMOVE THIS
            role_string,
            intra_role_order,
            action_string,
            p_shared->shared_mem[HACK],
            p_shared->shared_mem[SERF]
    );
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
