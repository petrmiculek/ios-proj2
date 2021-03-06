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

#define barrier_shm_name "/xmicul08-barrier_shm_name"
#define barrier_turnstile1_name "/xmicul08-barrier_turnstile1_name"
#define barrier_turnstile2_name "/xmicul08-barrier_turnstile2_name"
#define barrier_mutex_name "/xmicul08-barrier_mutex_name"

#define sync_shm_name "/xmicul08-sync_shm_name"
#define sync_hacker_queue_name "/xmicul08-sync_hacker_queue_name"
#define sync_serf_queue_name "/xmicul08-sync_serf_queue_name"
#define sync_mutex_name "/xmicul08-sync_mutex_name"
#define sync_entry_mutex_name "/xmicul08-sync_entry_mutex_name"
#define sync_mem_lock_name "/xmicul08-sync_mem_lock_name"
#define sync_boat_mutex_name "/xmicul08-sync_boat_mutex_name"


#define BARRIER_BUFSIZE 1 // count
#define SYNC_BUFSIZE 5 // action_count, hacker_count, serf_count, hacker_total, serf_total
#define BARRIER_shm_SIZE (sizeof(int)*BARRIER_BUFSIZE)
#define SYNC_shm_SIZE (sizeof(int)*SYNC_BUFSIZE)

#define BOAT_CAPACITY 4
#define SEM_ACCESS_RIGHTS 0644
#define PRINT_PIER_STATE 1
#define DONT_PRINT_PIER_STATE 0
#define TIME_REQUEUE_MIN 20 // [microseconds]


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
#define TIME_REQUEUE_MAX 4
#define PIER_CAPACITY 5
#define ARGS_COUNT 6
/// #endwarning

#ifdef DEBUG
#define PRINT_ERRNO_IF_SET() do { if(errno != 0) { warning_msg("errno = %d\n", errno); } else { printf("errno OK\n");} } while(0)
#else
#define PRINT_ERRNO_IF_SET()
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
 *  @var mutex Semaphore which protects the wait-test_full_crew block for regular passengers
 *             and the whole wait-test_full_crew-boards-captain_exits block for the captain
 *             Calling wait (at the pier) and testing for full crew is a critical section.
 *
 *
 * @var boat_mutex Semaphore that makes sure nobody boards the boat before the previous
 *                 passengers have exited it.
 *
 * @var hacker_queue Semaphore for HACk passengers to wait for the rest of the crew
 * @var serf_queue Semaphore for SERF passengers to wait for the rest of the crew
 *
 * @var shared_mem int[SYNC_BUFSIZE] ( --> int[5] ), passengers' shared memory variables
 *
*                  index    content                macro for accessing (index)
 *                 -----------------------------------------------------------
 *                 [0]      action_count           [ACTION]
 *                 [1]      current_hacker_count   [HACK]
 *                 [2]      current_serf_count     [SERF]
 *                 [3]      total_hacker_count     [HACK_TOTAL]
 *                 [4]      total_serf_count       [SERF_TOTAL]
 *
 *
 * @var mem_lock Mutex for guarding shared memory access
 */
struct Sync_t {
    barrier_t* p_barrier;
    sem_t* mutex; // init 1
	sem_t *entry_mutex; // init 1
    sem_t *boat_mutex; // init 1
    sem_t* mem_lock; // init 1
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
 * @param p_shared synchronization object (contains the barrier object)
 * @param arguments program's arguments array
 * @param fp output stream
 * @param role passenger's role: HACK or SERF
 *
 * @desc A passenger is started up by one of the two generate_passengers processes,
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
 * @see Barrier_t
 * @see Sync_t
 */
void passenger_routine(sync_t *p_shared, const int arguments[ARGS_COUNT], FILE *fp, int role);


/**
 * @brief Calling process sleeps for a random time in range <min, max>
 * @param minimum_sleep_time minimum sleep time in miliseconds
 * @param maximum_sleep_time maximum sleep time in miliseconds
 */
void sleep_in_range(int minimum_sleep_time, int maximum_sleep_time);

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
