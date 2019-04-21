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
#define sync_mem_action_lock_name "/xplagiat00b-sync_mem_action_lock_name"
#define sync_boat_seat_name "/xplagiat00b-sync_boat_seat_name"


#define BARRIER_BUFSIZE 1 // count
#define SYNC_BUFSIZE 5 // action_count, hacker_count, serf_count, hacker_total, serf_total
#define BARRIER_shm_SIZE (sizeof(int)*BARRIER_BUFSIZE)
#define SYNC_shm_SIZE (sizeof(int)*SYNC_BUFSIZE)

#define BOAT_CAPACITY 4
#define SEM_ACCESS_RIGHTS 0644
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
 * @see barrier_1
 * @see barrier_2
 *
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
 *  @brief Process synchronization structure
 *
 *  @note semaphore boat_seat is initialized to an unexpected value
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
    sem_t *mem_action_lock; // init 1
    sem_t* hacker_queue; // init 0
    sem_t* serf_queue; // init 0
    int *shared_mem; // .action = 0, .hacker_count = 0,
                    // .serf_count = 0, .hack_total = 0, .serf_total = 0
    int shared_mem_fd;
} ;
typedef struct Sync_t sync_t;


/**
 * @brief String to Int conversion with safety checks
 * @param str string to parse from
 * @return valid integer value from string if the format is valid
 *		 -1 on invalid format
 */
int parse_int(const char *str);


/**
 *  @brief Initializes a barrier structure
 * @param p_barrier barrier object to be initialized
 * @return 0 on success,
 *         non-zero value on failure
 */
int barrier_init(barrier_t *p_barrier);

/**
 * @brief Initializes single structure for process synchronization  (all semaphores, shared memory)
 * @param p_shared synchronization object to be initialized
 * @return zero on success,
 *         non-zero value on failure
 */
int sync_init(sync_t *p_shared); //, int pier_capacity

/**
 * @brief Destroys a barrier structure, close&unlink semaphores, unmap&unlink shared mem
 * @param p_barrier barrier object to be destroyed
 * @return zero on success,
 *         non-zero value on failure
 */
int barrier_destroy(barrier_t *p_barrier);

/**
 * @brief Destroys a barrier structure, close&unlink semaphores, unmap&unlink shared mem
 * @param p_barrier barrier object to be destroyed
 * @return zero on success,
 *         non-zero value on failure
 */
int sync_destroy(sync_t *p_shared);

/**
 * @brief Generator of passenger processes
 * @param p_shared synchronization object
 * @param arguments program's arguments array
 * @param fp output stream
 * @param role passenger's role: HACK or SERF
 * @return zero on successful forking
 *         non-zero on failure
 */
int generate_passengers(sync_t *p_shared, const int arguments[ARGS_COUNT], FILE *fp, int role);

/**
 * @brief Runs the passenger's routine
 * @param p_shared synchronization object
 * @param arguments program's arguments array
 * @param fp output stream
 * @param role passenger's role: HACK or SERF
 *
 * @desc passenger is started up by one of the two generate_passengers processes
 *       he announces his startup, then tries to join the queue at the pier, where
 *       passengers wait for a boat. If the pier if full, he leaves the queue for some
 *       time and then tries to come back. Both leaving the queue and the return are
 *       announced. Once he can join the queue, he will wait (while announcing it).
 *
 *       Once a suitable passengers group (4H or 4S or 2H+2S) has gathered, the last one
 *       becomes the captain, who calls the rest of the group to board. The crew spends
 *       some time crossing the river (row_boat) and then calls their exit. Captain is the last
 *       passenger to do so.
 *
 *       Each of the passengers runs this code, but in a separate process.
 * @see Barrier_t, Sync_t Semaphores & Shared-memory-objects used and their meaning
 */
void passenger_routine(sync_t *p_shared, const int arguments[ARGS_COUNT], FILE *fp, int role);


/**
 * @brief Part of captain's passenger_routine,
 * @desc announces the action and sleeps for some time, while others (in passenger_routine) wait for him
 * @param p_shared synchronization object
 * @param arguments program's arguments array
 * @param fp output stream
 * @param role HACK or SERF (also, macros expanding to 1 and 2, respectively)
 * @param intra_role_order passenger's number per role (HACK 1, SERF 1, HACK 2, HACK 3, SERF 2)
 */
void row_boat(sync_t *p_shared, const int arguments[ARGS_COUNT], FILE *fp, int role, int intra_role_order);

/**
 * @brief Calling process sleeps for a random time up to a given amount
 * @param maximum_sleep_time maximum sleep time in microseconds
 */
void sleep_up_to(int maximum_sleep_time);

/**
 * @brief Runs phase 1/2 of an reusable barrier (synchronization mechanism)
 * @param p_barrier barrier object
 */
void barrier_1(barrier_t *p_barrier);

/**
 * @brief Runs phase 2/2 of an reusable barrier (synchronization mechanism)
 * @param p_barrier barrier object
 */
void barrier_2(barrier_t *p_barrier);

/**
 * @brief checks arguments' validity
 * @param arguments program's arguments
 * @return zero on valid arguments
 *         non-zero on invalid arguments
 */
int test_arguments_validity(const int arguments[ARGS_COUNT]);

/**
 * @brief Prints out action in given format and increments the action counter
 * @warning callee needs to lock the Sync_t.mem_action_lock
 * @param fp output stream
 * @param p_shared synchronization object
 * @param role HACK or SERF (also, macros expanding to 1 and 2, respectively)
 * @param intra_role_order passenger's order per role
 * @param action_string action that the callee is announcing
 * @param print_pier_state print current waiting queue state
 */
void print_action_plus_plus(FILE *fp, sync_t *p_shared, int role, int intra_role_order, const char *action_string,
                            bool print_pier_state);

/**
 * @brief prints out help text, telling the user how to run the program
 */
void print_help();

#endif //IOS_PROJ2_PROJ2_H
