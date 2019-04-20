#ifndef IOS_PROJ2_PROJ2_H
#define IOS_PROJ2_PROJ2_H

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

#define barrier_shm_name "/xplagiat00b-barrier_shm_name"
#define barrier_turnstile1_name "/xplagiat00b-barrier_turnstile1_name"
#define barrier_turnstile2_name "/xplagiat00b-barrier_turnstile2_name"
#define barrier_mutex_name "/xplagiat00b-barrier_mutex_name"

#define sync_shm_name "/xplagiat00b-sync_shm_name"
#define sync_hacker_queue_name "/xplagiat00b-sync_hacker_queue_name"
#define sync_serf_queue_name "/xplagiat00b-sync_serf_queue_name"
#define sync_mutex_name "/xplagiat00b-sync_mutex_name"
#define sync_mem_lock_name "/xplagiat00b-sync_mem_lock_name"
#define sync_boat_seat_name "/xplagiat00b-sync_boat_seat_name"


#define BARRIER_BUFSIZE 1 // count
#define SYNC_BUFSIZE 5 // action_count, hacker_count, serf_count, hacker_total, serf_total
#define BARRIER_shm_SIZE (sizeof(int)*BARRIER_BUFSIZE)
#define SYNC_shm_SIZE (sizeof(int)*SYNC_BUFSIZE)

#define BOAT_CAPACITY 4
#define SEM_ACCESS_RIGHTS 0644 //@TODO Implement or throw away
#define PRINT_PIER_STATE 1
#define DONT_PRINT_PIER_STATE 0

/// #warning Macros below ARE NOT ACTUAL VALUES of given variable
// INDEXING macros for shm[...]
#define ACTION 0
#define HACK 1
#define SERF 2
#define HACK_TOTAL 3
#define SERF_TOTAL 4

// INDEXING macros for arguments[6]
#define OF_EACH_TYPE 0
#define TIME_HACK_GEN 1
#define TIME_SERF_GEN 2
#define TIME_BOAT 3
#define TIME_REQUEUE 4
#define PIER_CAPACITY 5
#define ARGS_COUNT 6
/// #endwarning

#ifdef NDEBUG
#define PRINT_ERRNO_IF_SET()
#else
#define PRINT_ERRNO_IF_SET() do { if(errno != 0) { warning_msg("errno = %d\n", errno); } else { printf("errno OK\n");} } while(0)
#endif // DEBUG

/**
 * Reusable barrier
 */
struct Barrier_t {
    sem_t *turnstile1; // init 0
    sem_t *turnstile2; // init 0
    sem_t *barrier_mutex; // init 1
    int *barrier_shm; // .count = 0
    int barrier_shm_fd;
} ;
typedef struct Barrier_t barrier_t;

/**
 *  Process synchronization structure
 *  Note: semaphore boat_seat is initialized to an unexpected value
 *  (BOAT_CAPACITY + 1) --> 5
 *  This is a constraint for the section where a passenger tries to
 *  get on the boat. Since 5 passengers always make for a valid
 *  passenger group, it should never cause a deadlock.
 *
 *  It also makes sure that when one group (of 4 passengers) is
 *  crossing the river, no other complete group will try and board
 *  the boat.
 *
 *
 */
struct Sync_t {
    barrier_t* p_barrier;
    sem_t* mutex; // init 1
    sem_t *boat_seat; // init BOAT_CAPACITY + 1
    sem_t* mem_lock; // init 1
    sem_t* hacker_queue; // init 0
    sem_t* serf_queue; // init 0
    int *shared_mem; // .action = 0, .hacker_count = 0,
                    // .serf_count = 0, .hack_total = 0, .serf_total = 0
    int shared_mem_fd;
} ;
typedef struct Sync_t sync_t;


/**
 * @brief String to Int with safety checks
 * @param str string to parse from
 * @return valid integer value from string if the format is valid
 *		 -1 on invalid format
 */
int parse_int(const char *str);

// @TODO doxygen comments

int barrier_init(barrier_t *p_barrier);

int sync_init(sync_t *p_shared); //, int pier_capacity
// @TODO remove all pier_capacity vars

int barrier_destroy(barrier_t *p_barrier);
int sync_destroy(sync_t *p_shared);

void passenger_routine(sync_t *p_shared, const int arguments[ARGS_COUNT], FILE *fp, int role);

int generate_passengers(sync_t *p_shared, const int arguments[ARGS_COUNT], FILE *fp, int role);


void row_boat(sync_t *p_shared, const int arguments[ARGS_COUNT], FILE *fp, int role, int intra_role_order);

void sleep_up_to(int maximum_sleep_time);

void print_help();

/**
 * @brief Prints out action in given format
 * @param fp
 * @param p_shared
 * @param role
 * @param action_string
 */
void print_action_plus_plus(FILE *fp, sync_t *p_shared, int role, int intra_role_order, const char *action_string,
                            bool print_pier_state);

int test_arguments_validity(const int *arguments);

void barrier_1(barrier_t *p_barrier);

void barrier_2(barrier_t *p_barrier);

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

#endif //IOS_PROJ2_PROJ2_H
