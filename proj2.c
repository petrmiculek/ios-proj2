/**
 * @author Petr Mičulek
 * @date 27. 4. 2019
 * @title IOS Projekt 2 - Synchronizace procesů 
 * @desc River crossing problem implementation in C,
 * 			using processes, semaphores
 *
 * */

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

    if (test_arguments_validity(arguments) == -1) {
        return 1;
    }

    // ######## INITIALIZING ########

    FILE* fp;

    if ((fp = fopen("rivercrossing.out", "w+")) == NULL) {
        warning_msg("%s: opening output file", "main");
        return 1;
    }

    setbuf(fp, NULL);
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    barrier_t barrier1;
    if(barrier_init(&barrier1) == -1)
    {
        warning_msg("barrier_init\n");

    }

    sync_t sync1;
    if (sync_init(&sync1)) {
        warning_msg("sync_init\n");
    }
    sync1.p_barrier = &barrier1;

    srand(time(NULL) * getpid());

    // ######## FORKING ########

    pid_t pid_hack_gen = fork();
    if (pid_hack_gen == 0)
    {
        if (generate_passengers(&sync1, arguments, fp, HACK) == -1)
        {
            warning_msg("generating hackers\n");
            return_value = -1;
        }

        exit(return_value);
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

        exit(return_value);
    }
    else if (pid_serf_gen < 0)
    {
        warning_msg("forking serf-gen\n");
        return_value = -1;
    }

    // ######## CLEANING UP ########

    for (int j = 0; j < (2 + (2 * arguments[OF_EACH_TYPE])); ++j) {
        wait(NULL);
    }

    if (barrier_destroy(&barrier1)) {
        warning_msg("barrier_destroy\n");
        return_value = 1;
    }

    if (sync_destroy(&sync1)) {
        warning_msg("sync_destroy\n");
        return_value = 1;
    }

    if (fclose(fp) == -1) {
        warning_msg("closing file\n");
        return_value = 1;
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
            passenger_routine(p_shared, arguments, fp, role);
        } else if (pid < 0)
        {
            warning_msg("fork fail for role: %d\n", role);
            return -1;
        }
    }

    for (int j = 0; j < arguments[OF_EACH_TYPE]; ++j) {
        wait(NULL);
    }

    return 0;
}

void passenger_routine(sync_t *p_shared, const int arguments[ARGS_COUNT], FILE *fp, int role) {
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

    sem_wait(p_shared->mem_lock);

    p_shared->shared_mem[role_total]++;
    intra_role_order = p_shared->shared_mem[role_total];

    sem_wait(p_shared->mem_action_lock);
    print_action_plus_plus(fp, p_shared, role, intra_role_order, "starts", DONT_PRINT_PIER_STATE);
    sem_post(p_shared->mem_action_lock);


    while (arguments[PIER_CAPACITY] <= (p_shared->shared_mem[HACK] + p_shared->shared_mem[SERF])) {
        print_action_plus_plus(fp, p_shared, role, intra_role_order, "leaves queue", PRINT_PIER_STATE);
        sem_post(p_shared->mem_lock);

        sleep_up_to(arguments[TIME_REQUEUE]);

        sem_wait(p_shared->mem_lock);
        print_action_plus_plus(fp, p_shared, role, intra_role_order, "is back", DONT_PRINT_PIER_STATE);
        //sem_post(p_shared->mem_lock);
    }
    sem_post(p_shared->mem_lock);


    sem_wait(p_shared->mem_action_lock);

    sem_wait(p_shared->mem_lock);
    p_shared->shared_mem[role]++;
    sem_post(p_shared->mem_lock);

    print_action_plus_plus(fp, p_shared, role, intra_role_order, "waits", PRINT_PIER_STATE);
    sem_post(p_shared->mem_action_lock);

    sem_wait(p_shared->boat_seat);

    // DEBUG moved main mutex here
    sem_wait(p_shared->mutex);


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

    }

    sem_wait(role_queue);



    if(is_captain)
    {
        row_boat(p_shared, arguments, fp, role, intra_role_order); // "boards"
    }

    barrier_1(p_shared->p_barrier);

    if(!is_captain)
    {

        sem_wait(p_shared->mem_action_lock);
        print_action_plus_plus(fp, p_shared, role, intra_role_order, "member exits", PRINT_PIER_STATE);
        sem_post(p_shared->mem_action_lock);
    }

    barrier_2(p_shared->p_barrier);


    if(is_captain)
	{
        sem_wait(p_shared->mem_action_lock);
        print_action_plus_plus(fp, p_shared, role, intra_role_order, "captain exits", PRINT_PIER_STATE);
        sem_post(p_shared->mem_action_lock);

        sem_post(p_shared->boat_seat);
        sem_post(p_shared->boat_seat);
        sem_post(p_shared->boat_seat);
        sem_post(p_shared->boat_seat);
    }

    exit(0);
}


void row_boat(sync_t *p_shared, const int arguments[ARGS_COUNT], FILE *fp, int role, int intra_role_order)
{
    sem_wait(p_shared->mem_action_lock);
    print_action_plus_plus(fp, p_shared, role, intra_role_order, "boards", PRINT_PIER_STATE);
    sem_post(p_shared->mem_action_lock);


    sem_post(p_shared->mutex);
    sleep_up_to(arguments[TIME_BOAT]);
}

void sleep_up_to(int maximum_sleep_time) {
    useconds_t sleep_time = (random() % (maximum_sleep_time + 1)) * 1000;
    usleep(sleep_time);
}

int parse_int(const char *str)
{
	char *end_ptr = NULL;
    long numerical_value = strtol(str, &end_ptr, 10);

	errno = 0;
    if (errno == ERANGE || strlen(str) == 0 || strcmp(end_ptr, "") != 0) {
        warning_msg("parsing - invalid format\n");
		errno = 0;
		return -1;
	}

    if (numerical_value < 0) {
        warning_msg("parsing - negative value (%s)\n", str);
	}

    return (int) numerical_value;
}

int barrier_init(barrier_t *p_barrier)
{
    if ((p_barrier->barrier_shm_fd = shm_open(barrier_shm_name, O_CREAT | O_EXCL | O_RDWR, SEM_ACCESS_RIGHTS)) == -1) {
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

    if ((p_barrier->barrier_mutex = sem_open(barrier_mutex_name, O_CREAT, SEM_ACCESS_RIGHTS, 1)) == SEM_FAILED) {
		warning_msg("%s,%s: initializing semaphore\n", "p_barrier", "1");
		return -1;
	}

    if ((p_barrier->turnstile1 = sem_open(barrier_turnstile1_name, O_CREAT, SEM_ACCESS_RIGHTS, 0)) == SEM_FAILED) {
		warning_msg("%s,%s: initializing semaphore\n", "p_barrier", "2");
		return -1;
	}

    if ((p_barrier->turnstile2 = sem_open(barrier_turnstile2_name, O_CREAT, SEM_ACCESS_RIGHTS, 1)) == SEM_FAILED) {
        warning_msg("%s,%s: initializing semaphore\n", "p_barrier", "3");
        return -1;
    }

    *(p_barrier->barrier_shm) = 0;

	return 0;
}

int sync_init(sync_t *p_shared) // ,int pier_capacity)
{
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


    if ((p_shared->mutex = sem_open(sync_mutex_name, O_CREAT, SEM_ACCESS_RIGHTS, 1)) == SEM_FAILED) {
		warning_msg("%s,%s: initializing semaphore\n", "sync", "1");
		return -1;
	}


    if ((p_shared->mem_lock = sem_open(sync_mem_lock_name, O_CREAT, SEM_ACCESS_RIGHTS, 1)) == SEM_FAILED) {
        warning_msg("%s,%s: initializing semaphore\n", "sync", "2");
        return -1;
    }

    if ((p_shared->mem_action_lock = sem_open(sync_mem_action_lock_name, O_CREAT, SEM_ACCESS_RIGHTS, 1)) ==
        SEM_FAILED) {
        warning_msg("%s,%s: initializing semaphore\n", "sync", "3");
        return -1;
    }


    if ((p_shared->hacker_queue = sem_open(sync_hacker_queue_name, O_CREAT, SEM_ACCESS_RIGHTS, 0)) == SEM_FAILED) {
        warning_msg("%s,%s: initializing semaphore\n", "sync", "4");
		return -1;
	}


    if ((p_shared->serf_queue = sem_open(sync_serf_queue_name, O_CREAT, SEM_ACCESS_RIGHTS, 0)) == SEM_FAILED) {
        warning_msg("%s,%s: initializing semaphore\n", "sync", "5");
		return -1;
	}

    if ((p_shared->boat_seat = sem_open(sync_boat_seat_name, O_CREAT, SEM_ACCESS_RIGHTS, BOAT_CAPACITY + 1)) ==
        SEM_FAILED) {
        warning_msg("%s,%s: initializing semaphore\n", "sync", "6");
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

    if((sem_close(p_barrier->barrier_mutex) |
		sem_close(p_barrier->turnstile1)|
		sem_close(p_barrier->turnstile2))
		== -1)
	{
		warning_msg("%s: closing semaphores\n", "p_barrier");
		return_value = -1;
	}

	if((sem_unlink(barrier_mutex_name) |
		sem_unlink(barrier_turnstile1_name)|
		sem_unlink(barrier_turnstile2_name))
		== -1)
	{
		warning_msg("%s: unlinking semaphores\n", "p_barrier");
		return_value = -1;
	}

	if(munmap(p_barrier->barrier_shm, BARRIER_shm_SIZE) == -1)
	{
		warning_msg("%s: unmapping memory\n", "p_barrier");
		return_value = -1;
	}
	
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
        sem_close(p_shared->mem_action_lock) |
        sem_close(p_shared->boat_seat) |
        sem_close(p_shared->mutex))
		== -1)
	{
		warning_msg("%s: closing semaphores\n", "sync");
		return_value = -1;
	}


	if((sem_unlink(sync_hacker_queue_name) |
        sem_unlink(sync_serf_queue_name) |
        sem_unlink(sync_mem_lock_name) |
        sem_unlink(sync_mem_action_lock_name) |
        sem_unlink(sync_boat_seat_name) |
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


void print_action_plus_plus(FILE *fp, sync_t *p_shared, int role, int intra_role_order, const char *action_string,
                            bool print_pier_state) {
    p_shared->shared_mem[ACTION]++;

    char* role_string = ((role == HACK) ? "HACK" : "SERF");

    if (print_pier_state) {
        fprintf(fp, "%-4d : %s %-10d: %-20s : %-8d : %d\n",
                p_shared->shared_mem[ACTION],
                role_string,
                intra_role_order,
                action_string,
                p_shared->shared_mem[HACK],
                p_shared->shared_mem[SERF]
        );
    } else {
        fprintf(fp, "%-4d : %s %-10d: %-20s\n",
                p_shared->shared_mem[ACTION],
                role_string,
                intra_role_order,
                action_string
        );
    }
}

void print_help()
{
    fprintf(stdout, "Run like:\n"
		   " ./proj2 P H S R W C\n"
		   "	P - Hackers count == Serfs count\n"
                    "	H - Hackers generation duration (maximum)\n"
                    "	S - Serfs generation duration(max)\n"
                    "	R - Rowing duration (max)\n"
                    "	W - Time it takes to return to pier after leaving (max)\n"
		   "	C - Waiting queue (pier) capacity\n");
}

int test_arguments_validity(const int arguments[ARGS_COUNT]) {
    int return_value = 0;

    if ((2 * arguments[OF_EACH_TYPE] < BOAT_CAPACITY) || (arguments[OF_EACH_TYPE] % 2 != 0)) {
        return_value = -1;
        warning_msg("invalid argument: passenger count\n");
    }

    if ((arguments[TIME_HACK_GEN] < 0) || (arguments[TIME_HACK_GEN] > 2000)) {
        return_value = -1;
        warning_msg("invalid argument: hack generation time\n");
    }

    if ((arguments[TIME_SERF_GEN] < 0) || (arguments[TIME_SERF_GEN] > 2000)) {
        return_value = -1;
        warning_msg("invalid argument: serf generation time\n");
    }

    if ((arguments[TIME_BOAT] < 0) || (arguments[TIME_BOAT] > 2000)) {
        return_value = -1;
        warning_msg("invalid argument: boat time\n");
    }
    if ((arguments[TIME_REQUEUE] < 0) || (arguments[TIME_REQUEUE] > 2000)) {
        return_value = -1;
        warning_msg("invalid argument: requeue time\n");
    }
    if (arguments[PIER_CAPACITY] < 5) {
        return_value = -1;
        warning_msg("invalid argument: pier capacity\n");
    }

    return return_value;
}

void barrier_1(barrier_t *p_barrier) {
    sem_wait(p_barrier->barrier_mutex);

    *(p_barrier->barrier_shm) += 1;
    if (*(p_barrier->barrier_shm) == BOAT_CAPACITY) {
        sem_wait(p_barrier->turnstile2);
        sem_post(p_barrier->turnstile1);
    }

    sem_post(p_barrier->barrier_mutex);


    sem_wait(p_barrier->turnstile1);
    sem_post(p_barrier->turnstile1);
}

void barrier_2(barrier_t *p_barrier) {

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
