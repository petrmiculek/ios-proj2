#ifndef IOS_PROJ2_PROJ2_H
#define IOS_PROJ2_PROJ2_H

/**
 * @author Petr Mičulek
 * @date 27. 4. 2019
 * @title IOS Projekt 2 - Synchronizace procesů
 * @desc River crossing problem implementation in C,
 * 			using processes, semaphores
 * */


#define barrier_shm_name "/xplagiat00b-barrier_shm_name"
#define barrier_turnstile1_name "/xplagiat00b-barrier_turnstile1_name"
#define barrier_turnstile2_name "/xplagiat00b-barrier_turnstile2_name"
#define barrier_mutex_name "/xplagiat00b-barrier_mutex_name"

#define sync_shm_name "/xplagiat00b-sync_shm_name"
#define sync_hacker_queue_name "/xplagiat00b-sync_hacker_queue_name"
#define sync_serf_queue_name "/xplagiat00b-sync_serf_queue_name"
#define sync_mutex_name "/xplagiat00b-sync_mutex_name"


#define ARGS_COUNT 6

#define LOCKED 0 // Semaphore state
#define BARRIER_BUFSIZE 1 // count
#define SYNC_BUFSIZE 5 // action_count, hacker_count, serf_count, hacker_total, serf_total
#define BARRIER_shm_SIZE sizeof(int)*BARRIER_BUFSIZE
#define SYNC_shm_SIZE sizeof(int)*SYNC_BUFSIZE

// INDEXING macros for shm[...]
#define ACTION 0
#define HACK 1
#define SERF 2
#define HACK_TOTAL 3
#define SERF_TOTAL 4


#define BOAT_CAPACITY 4
#define SEM_ACCESS_RIGHTS 0644 //@TODO Implement or throw away

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
    int *barrier_shm; // count; init 0
    int barrier_shm_size;
    int barrier_shm_fd;
} ;
typedef struct Barrier_t barrier_t;

/**
 *  Process synchronization structure
 */
struct Sync_t {
    barrier_t* p_barrier;
    sem_t* mutex; // init 1
    sem_t* hacker_queue; // init 0
    sem_t* serf_queue; // init 0
    int *shared_mem; // hacker_count, serf_count
    //int *action_count_shm;
    int shared_mem_size;
    int shared_mem_fd;
} ;
typedef struct Sync_t sync_t;


/**
 * @brief String to Int with safety checks
 * @param str string to parse from
 * @return valid integer value from string if the format is valid
 *		 -1 on invalid format
 */
int parse_int(char *str);

// @TODO doxygen comments
void print_help();

void DEBUG_print_args(int argc, const int *arguments);

int barrier_init(barrier_t *p_barrier);
int sync_init(sync_t *p_shared);

int barrier_destroy(barrier_t *p_barrier);
int sync_destroy(sync_t *p_shared);

void hacker_routine(sync_t *p_shared, const int arguments[ARGS_COUNT], FILE *fp);
void serf_routine(sync_t *p_shared, const int arguments[ARGS_COUNT], FILE *fp);

int generate_hackers(sync_t *p_shared, const int arguments[ARGS_COUNT], FILE *fp);
int generate_serfs(sync_t *p_shared, const int arguments[ARGS_COUNT], FILE* fp);


void row_boat(sync_t *pSync, const int arguments[ARGS_COUNT], FILE *fp, int role);

void reusable_barrier(const barrier_t *p_barrier);



/**
 * @brief Prints out action in given format
 * @param fp
 * @param p_sync
 * @param role
 * @param action_string
 */
void print_action(FILE *fp, sync_t *p_sync, int role, char *action_string);
#endif //IOS_PROJ2_PROJ2_H
