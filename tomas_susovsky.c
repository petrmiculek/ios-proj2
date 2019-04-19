/**


    Notes:          Program arguments:

                    -P: Number of persons generated in each category.
                        P > 0 && P % 2 = 0

                    -H: Maximum time (in milliseconds) to generate a new hacker-process.
                        H >= 0 && H < 5001

                    -S: Maximum time (in milliseconds) to generate a new serf-process.
                        S >= 0 && S < 5001

                    -R: Maximum time (in milliseconds) to cross the river.
                        R >= 0 && R < 5001


                    All argument are integers.


    Limitations: Program is limitated by maximum allowed amout of processes assigned to user by operation system.

    Function:   1)  parse_arguments
                2)  valar_initializ
                3)  valar_morghulis
                3)  killme
                4)  fork_forge
                5)  fork_child
                6)  counter
                7)  set_course_for_intercourse
                8)  get_on_board
                9)  sem_shop


*/

// Libraries:
// general:
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// time.h for handling waitting in random time interval:
#include <time.h>
// Libraries for work with semaphores and shared memory:
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mman.h>


// Constants - for better readability of the code:
// process categories: Hackers & Serfs
#define HACKER 0
#define SERF 1
// process priority for rivercrossing:
#define MEMBER 1
#define CAPTAIN 2

// for checking, if ship is fully loaded:
#define SHIP_CAPACITY 4
// for checking failures of system functions:
#define SYS_FAIL (-1)
// for checking wrong arguments:
#define ARG_FAIL 1
// for returning 2 if system function fails:
#define EXIT_SYS_FAILURE 2
// for return value of argument-parsing function, if program was run with --help:
#define HELP_MESSAGE 3

// Structure, which will be used for holding program's arguments:
typedef struct{

    int p;      // p stores amount of children processes of each category generated.
    int h;      // h stores value of maximum time to generate new hacker child-process.
    int s;      // s stores value of maximum time to generate new serf child-process.
    int r;      // r stores value of maximum time to perform crossing the river.
} param;


/** shared resources **/

// SEMAPHORES:
sem_t

        *sem_counter = NULL,        // changing value of shared variable.
        *sem_log = NULL,            // writing to output file.

        *sem_hacker_wait = NULL,    // getting onboard for hackers.
        *sem_serf_wait = NULL,      // getting onboard for serfs.

        *sem_molo = NULL,           // getting on pier, when previous crossing is finished.
        *sem_board = NULL,          // getting on board, when appropriate group is gathered.
        *sem_ride = NULL,           // starting crossing the river, when all members are onboard.
        *sem_landing = NULL,        // landing after crossing the river is finished.
        *sem_finish = NULL;         // THE END - after all process have crossed the river.

// POINTERS TO SHARED VARIABLES:
int
        // shared variable used as action-counter
        // (after each action is incremented by 1 for log)
        *sharedCounter,
        // Array of two shared variable
        // used as counter of processes of given category currently waitting on pier
        // sharedN[0] = amount of HACKERs waitting
        // sharedN[1] = amount of SERFs waitting
        *sharedN[2],
        // shared variable used for counting processes
        // f.e. is all members are boarded, landed etc.
        *sharedSEAT;

// FILE DESCRIPTORS FOR SHARED VARIABLES:
int

        shm_fd0,                    // sharedCounter
        shm_fd1,                    // sharedN[0]
        shm_fd2,                    // sharedN[1]
        shm_fd3;                    // sharedSEAT

// OUTPUP FILE FOR LOG:
FILE *fp = NULL;


/** functions prototypes **/

// service functions
// handle program's arguments
// + allocation/initialization & freeing/termination shared resources
// and taking care of program, if something goes horribly wrong
int parse_arguments(int argc, char *argv[], param *pm);
int valar_initializ(void);
int valar_morghulis(void);
void killme(int sig);

// forking functions
// handle forking children process
// and children processes activity
int fork_forge(FILE *fp, param *pm, int sleep, int category);
void fork_child(FILE *fp, param *pm, int category, int i, int *y);

// synchronization functions
// handle work with shared resources (shared variables)
// and process synchronization (via locking/unlocking semaphores)
int counter(int *x, int y);
int set_course_for_intercourse(void);
void get_on_board(int *seat, sem_t *sem_target, int free_seat, int num);
void sem_shop(sem_t *sem_target, int free_seat);

/** messages **/

// enum for names of error messages
enum
{
    FORK,
    FOPEN,
    FCLOSE,
    FTRUNCATE,
    MMAP,
    MUNMAP,
    PARAM_MISSING,
    PARAM_INT,
    PARAM_RANGE,
    SEM_OPEN,
    SEM_CLOSE,
    SEM_UNLINK,
    SIGNAL,
    SHM_OPEN,
    SHM_CLOSE,
    SHM_UNLINK,
};


// ERROR MESSAGES
const char *ERR_MSG[] =
{
    [FORK] = "Forking new child process has failed.\n",
    [FOPEN] = "Can't open output file rivercrossing.out.\n",
    [FCLOSE] = "Can't close output file rivercrossing.out.\n",
    [FTRUNCATE] = "Truncating file size has failed.\n",
    [MMAP] = "Mapping file to memory has failed.\n",
    [MUNMAP] = "Unmapping file from memory has failed.\n",
    [PARAM_MISSING] = "Invalid program arguments.\nUsage:   rivercrossing -P -H -S -R\nSee --help for further informations.\n",
    [PARAM_INT] = "Invalid program argument.\nAll arguments must be integers.\nSee --help for further informations.\n",
    [PARAM_RANGE] = "Invalid program argument.\nArgument doesn't fit its range.\nSee --help for further informations.\n",
    [SEM_OPEN] = "Creating semaphore has failed.\n",
    [SEM_CLOSE] = "Closing semaphore has failed.\n",
    [SEM_UNLINK] = "Removing semaphore has failed.\n",
    [SIGNAL] = "Interupted by SIGNAL:",
    [SHM_OPEN] = "Creating shared memory has failed.\n",
    [SHM_CLOSE] = "Closing file of shared memory has failed.\n",
    [SHM_UNLINK] = "Removing shared memory has failed.\n",
};

// names of process categories
// used for identification of process in log
const char *CATEGORY[] =
{
    [HACKER] = "hacker",
    [SERF] = "serf",
};

// HELP MESSAGE
const char *HELPMSG =
    "rivercrossing - iOS project no.2\n"
    "Author: Tomas Susovsky\n"
    "Usage: rivercrossing -P -H -S -R\n\n\n"
    "Arguments:\n\n"
    "   -P          Number of persons generated in each category.\n"
    "               P > 0 && P % 2 = 0\n"
    "   -H          Maximum time (in milliseconds) to generate a new hacker-process.\n"
    "               H >= 0 && H < 5001\n"
    "   -S          Maximum time (in milliseconds) to generate a new serf-process.\n"
    "               S >= 0 && S < 5001\n"
    "   -R          Maximum time (in milliseconds) to cross the river.\n"
    "               R >= 0 && R < 5001\n\n"
    "               All arguments are integers.\n"
    "\nIn case of experiencing a bug, please notify: xsusov01@stud.fit.vutbr.cz\n"
    "\n"
    ;


/** functions **/

/* @main int ******************************************************************
**
** @brief main function forks two children processes (fork_forge for hacker/serf)
**        Each child forge-process continue to fork children of its own category.
**
** @param[in] int argc - amount of program's command line arguments.
** @param[in] char *argv[] - array of strings containing names of program's command line arguments.
** @return 0 when successful.
**         1 when program's command line arguments were invalid.
**         2 when system function failed.
** @@
******************************************************************************/
int main(int argc, char *argv[])
{

    pid_t
          pid_hacker,       // for process of the first fork (hacker fork_forge).
          pid_serf;         // for process of the second fork (serf fork_forge).

    param *pm, ini;         // structure for storing program's arguments values.
    pm = &ini;

    /*  TRAPS:
        prevents killing program without freeing shared resources and killing all children.
    */
    signal(SIGTERM, killme); // termination signal
    signal(SIGINT, killme);  // int signal sent by user by pressing CTRL+C.
    signal(SIGSEGV, killme); // when s*** happens.
                             //(f.e. someone other has created shared memory on adress /xsusov01_sharedMemory0 and hasn't removed it yet.)
                             //((note: this trap should solve this problem and re-run of program should be okay.))


    if (parse_arguments(argc, argv, pm) == ARG_FAIL)
    {                        // Arguments were invalid.
        return EXIT_FAILURE;
    }

    else if (parse_arguments(argc, argv, pm) == HELP_MESSAGE)
    {                        // Program showed HELP and can now end.
        return EXIT_SUCCESS;
    }


    if ((fp = fopen("rivercrossing_test.out", "w+")) == NULL)
    {                        // Opening output log has failed.
        fprintf(stderr, "%s", ERR_MSG[FOPEN]);
        return EXIT_SYS_FAILURE;
    }

    setbuf(fp, NULL);        // For writing logs (by multiple processes) to output file without a problem.

    if (valar_initializ() != 0)
    {                        // Tries to allocate and initializate shared resources, if fails, tries to clean
        valar_morghulis();
        return EXIT_SYS_FAILURE;
    }


    *sharedCounter   = 0;    // Initialize action-counter to 0.
    *sharedN[HACKER] = 0;    // So does with waitting-hackers-counter.
    *sharedN[SERF]   = 0;    // As well as with serfs one.
    *sharedSEAT      = 0;    // And seat-counter.

    pid_hacker = fork();
    if (pid_hacker == 0)
    {
        if (fork_forge(fp, pm, pm -> h, HACKER) == EXIT_SYS_FAILURE)
        {                    // Forking hacker child-process inside of hacker fork_forge failed.
            return EXIT_SYS_FAILURE;
        }
    }
    else if (pid_hacker < 0)
    {                        // Forking hacker fork_forge failed.
        fprintf(stderr, "%s", ERR_MSG[FORK]);
        killme(SIGINT);
        return EXIT_SYS_FAILURE;
    }


    srand(time(NULL) * getpid());
                             // Makes sure that SERF's random numbers are not as same as HACKER's ones.

    pid_serf = fork();
    if(pid_serf == 0)
    {
        if (fork_forge(fp, pm, pm -> s, SERF) == EXIT_SYS_FAILURE)
        {                    // Forking serf child-process inside of serf fork_forge failed.
            return EXIT_SYS_FAILURE;
        }
    }

    else if(pid_serf < 0)
    {                        // Forking serf fork_forge failed.
        fprintf(stderr, "%s", ERR_MSG[FORK]);
        killme(SIGINT);
        return EXIT_SYS_FAILURE;
    }


    int child;

    for (child = 0; child < ((pm -> p) * 2 + 2); child++)
    {                        // Assurance that children ends first.
        wait(NULL);
    }

    waitpid(-1, NULL, 0);    // Level 2 master sayian assurance.

    valar_morghulis();       // Clean shared resources etc.

    return EXIT_SUCCESS;     // IF PROGRAM GETS HERE EVERYTHING WENT BETTER THAN EXPECTING.
}


/* @parse_arguments int *******************************************************
**
** @brief function to parse program's command line arguments.
**
**  Function loads program's command line arguments (strings) and tries
**  to convert them to integers.
**  Then checks if values falls within the range and comply with other
**  conditions( -P ).
**  If anything fails, prints error message and returns 1.
**
** @param[in] int argc - amount of program's command line arguments.
** @param[in] char *argv[] - array of strings containing program's command line arguments.
** @param[out] param *pm - pointer to structure param, which holds values of all four program's arguments.
** @return 0 when successful.
**         1 when program's command line arguments were invalid.
**         3 for --help.
** @@
******************************************************************************/
int parse_arguments(int argc, char *argv[], param *pm)
{

    char *errorXe;           // Error detector.

    if (argc > 1)
    {
        if (strcmp(argv[1],"--help") == 0)
        {                    // Checks for --help argument, if present prints HELP MESSAGE and returns 3 to exit program succesfully.
            printf("%s\n", HELPMSG);
            return HELP_MESSAGE;
        }
    }

    if (argc > 4)
    {                        // Program needs 4 valid integer argument to work properly.
        pm -> p = strtoul(argv[1], &errorXe, 10);
            if (*errorXe != '\0')
            {                // Frist argument wasn't integer.
                fprintf(stderr, "%s\n", ERR_MSG[PARAM_INT]);
                return ARG_FAIL;
            }

        pm -> h = strtoul(argv[2], &errorXe, 10);
            if (*errorXe != '\0')
            {                // Second argument wasn't integer.
                fprintf(stderr, "%s\n", ERR_MSG[PARAM_INT]);
                return ARG_FAIL;
            }

        pm -> s = strtoul(argv[3], &errorXe, 10);
            if (*errorXe != '\0')
            {                // Third argument wasn't integer.
                fprintf(stderr, "%s\n", ERR_MSG[PARAM_INT]);
                return ARG_FAIL;
            }

        pm -> r = strtoul(argv[4], &errorXe, 10);
            if (*errorXe != '\0')
            {                // Fourth argument wasn't integer.
                fprintf(stderr, "%s\n", ERR_MSG[PARAM_INT]);
                return ARG_FAIL;
            }

        if (     (pm -> p <= 0) || (pm -> p % 2)
            ||    (pm -> h < 0) || (pm -> s < 0)
            ||    (pm -> r < 0) || (pm -> h > 5000)
            || (pm -> s > 5000) || (pm -> r > 5000) )
            {                // Arguments dont match their range.
                fprintf(stderr, "%s\n", ERR_MSG[PARAM_RANGE]);
                return ARG_FAIL;
            }
    }

    else
    {                        // Program was run withoud necessary arguments.
        fprintf(stderr, "%s", ERR_MSG[PARAM_MISSING]);
        return ARG_FAIL;
    }

    return 0;                // 0 if everything went right.
}


/* @valar_initializ int ******************************************************
**
** @brief All memory must by initialized.
**
** Creates all shared resources.
** Creates semaphores and shared memory and initializes them.
** Creates space in memory for shared variables and map them.
**
** @see valar_morghulis()
** @return  0 - if successful.
**          number - of system functions fails if any failed.
** @@
******************************************************************************/
int valar_initializ(void)
{

    int fail = 0;            // fail-counter to indicate system function failures.


    /* SEMAPHORES:
       Opens all semaphores in /dev/shm/ on adress starting with "xsusov01", which should prevent conflit with others students semaphores.
       Flags:       O_CREAT - for creating
                    O_EXCL  - for exclusivity

       Rights:      0644    - rw- r-- r--
                            - Owner has Read and Write
                            - Group has Read only
                            - Other has Read only

       Intializes semaphores to correct value.
       Value:      1        - the first process will be allowed to go in (and then he unlocks semaphore for next, if needed)
                   0        - locked by defalut, unlocked after condition is met (fe. needed amount of processes gathered in same phase)

       If opening of any semaphore fails, error message is sent to stderr and fail counter is raised by 1.
    */
    if (     ((sem_counter = sem_open("/xsusov01_semafor_counter", O_CREAT | O_EXCL, 0644, 1)) == SEM_FAILED)
          || ((sem_log = sem_open("/xsusov01_semafor_log", O_CREAT | O_EXCL, 0644, 1)) == SEM_FAILED)
          || ((sem_molo = sem_open("/xsusov01_semafor_molo", O_CREAT | O_EXCL, 0644, 1)) == SEM_FAILED)
          || ((sem_board = sem_open("/xsusov01_semafor_board", O_CREAT | O_EXCL, 0644, 0)) == SEM_FAILED)
          || ((sem_ride = sem_open("/xsusov01_semafor_ride", O_CREAT | O_EXCL, 0644, 0)) == SEM_FAILED)
          || ((sem_hacker_wait = sem_open("/xsusov01_semafor_hacker_wait", O_CREAT | O_EXCL, 0644, 0)) == SEM_FAILED)
          || ((sem_serf_wait = sem_open("/xsusov01_semafor_serf_wait", O_CREAT | O_EXCL, 0644, 0)) == SEM_FAILED)
          || ((sem_landing = sem_open("/xsusov01_semafor_landing", O_CREAT | O_EXCL, 0644, 0)) == SEM_FAILED)
          || ((sem_finish = sem_open("/xsusov01_semafor_finish", O_CREAT | O_EXCL, 0644, 0)) == SEM_FAILED)   )
    {
        fprintf(stderr, "%s", ERR_MSG[SEM_OPEN]);
        fail++;
    }


    /* CREATING SHARED MEMORY:
    Opens adress in shared memory for all shared variables in /dev/shm/ on adress starting with "xsusov01", which should prevent conflit with others students shared memory.
    Flags:       O_CREAT - for creating
                 O_EXCL  - for exclusivity
                 O_RDWR  - for read-write rights

    Rights:      0644    - rw- r-- r--
                         - Owner has Read and Write
                         - Group has Read only
                         - Other has Read only

    Result (adress) saves to file descriptor of given shared variable.

    If opening of any fails, error message is sent to stderr and fail counter is raised by 1.
    */
    if (     ((shm_fd0 = shm_open("/xsusov01_sharedMemory0", O_CREAT | O_EXCL | O_RDWR, 0644)) == SYS_FAIL)
          || ((shm_fd1 = shm_open("/xsusov01_sharedMemory1", O_CREAT | O_EXCL | O_RDWR, 0644)) == SYS_FAIL)
          || ((shm_fd2 = shm_open("/xsusov01_sharedMemory2", O_CREAT | O_EXCL | O_RDWR, 0644)) == SYS_FAIL)
          || ((shm_fd3 = shm_open("/xsusov01_sharedMemory3", O_CREAT | O_EXCL | O_RDWR, 0644)) == SYS_FAIL)  )
    {
        fprintf(stderr, "%s", ERR_MSG[SHM_OPEN]);
        fail++;
    }


    /* TRUNCATING FILEDESCRIPTORS:
    Truncate (chaneg) size of each file descriptor to size of one integer.

    If truncating of any fails, error message is sent to stderr and fail counter is raised by 1.
    */
    if (    (ftruncate(shm_fd0, sizeof(int)) == SYS_FAIL)
         || (ftruncate(shm_fd1, sizeof(int)) == SYS_FAIL)
         || (ftruncate(shm_fd2, sizeof(int)) == SYS_FAIL)
         || (ftruncate(shm_fd3, sizeof(int)) == SYS_FAIL) )
    {
        fprintf(stderr, "%s", ERR_MSG[FTRUNCATE]);
        fail++;
    }


    /* MAPPING SHARED MEMORY:
    Maps all shared variables to their shared memory adress (provided by file descriptor) and size.
    Flags:      O_CREAT - for creating
                O_EXCL  - for exclusivity
                O_RDWR  - for read-write rights

    Sets pointers on shared variables to correct adress mapped in shared memory.

    If mapping of any fails, error message is sent to stderr and fail counter is raised by 1.
    */
    if (     ((sharedCounter = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd0, 0)) == MAP_FAILED)
          || ((sharedN[0] = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd1, 0)) == MAP_FAILED)
          || ((sharedN[1] = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd2, 0)) == MAP_FAILED)
          || ((sharedSEAT = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd3, 0)) == MAP_FAILED) )
    {
        fprintf(stderr, "%s", ERR_MSG[MMAP]);
        fail++;
    }

    return fail;             // 0 if everything went right.
}


/* @valar_morghulis int ******************************************************
**
** @brief All memory must die.
**
** Removes all shared resources.
** Closes all semaphores and unlinks their adresses.
** Unmaps space in memory of shared variables, unlinks their adresses and closes them.
** In the end closes file, which was used as output (rivercrossing.out).
**
** @see valar_initializ()
** @return  0 - if successful.
**          number - of system functions fails if any failed.
** @@
******************************************************************************/
int valar_morghulis(void)
{

    int fail = 0;            // fail-counter to indicate system function failures.


    /* CLOSING:
    Closes all semaphores.

    If closing of any fails, error message is sent to stderr and fail counter is raised by 1.
    */
    if (    (sem_close(sem_counter) == SYS_FAIL)
         || (sem_close(sem_log) == SYS_FAIL)
         || (sem_close(sem_ride) == SYS_FAIL)
         || (sem_close(sem_hacker_wait) == SYS_FAIL)
         || (sem_close(sem_serf_wait) == SYS_FAIL)
         || (sem_close(sem_molo) == SYS_FAIL)
         || (sem_close(sem_board) == SYS_FAIL)
         || (sem_close(sem_landing) == SYS_FAIL)
         || (sem_close(sem_finish) == SYS_FAIL)     )
    {
        fprintf(stderr, "%s", ERR_MSG[SEM_CLOSE]);
        fail++;
    }

    /* UNLINKING SEMAPHORES FROM MEMORY:
    Removes semaphores from memory.

    If removing of any fails, error message is sent to stderr and fail counter is raised by 1.
    */
    if (    (sem_unlink("/xsusov01_semafor_counter") == SYS_FAIL)
         || (sem_unlink("/xsusov01_semafor_log") == SYS_FAIL)
         || (sem_unlink("/xsusov01_semafor_ride") == SYS_FAIL)
         || (sem_unlink("/xsusov01_semafor_hacker_wait") == SYS_FAIL)
         || (sem_unlink("/xsusov01_semafor_serf_wait") == SYS_FAIL)
         || (sem_unlink("/xsusov01_semafor_molo") == SYS_FAIL)
         || (sem_unlink("/xsusov01_semafor_board") == SYS_FAIL)
         || (sem_unlink("/xsusov01_semafor_landing") == SYS_FAIL)
         || (sem_unlink("/xsusov01_semafor_finish") == SYS_FAIL)   )
    {
        fprintf(stderr, "%s", ERR_MSG[SEM_UNLINK]);
        fail++;
    }

    /* UNMAPING POINTERS TO SHARED MEMORY:
    Unmaps shared variables (of given pointer and size) from memory.

    If unmapping of any fails, error message is sent to stderr and fail counter is raised by 1.
    */
    if (    ((munmap(sharedCounter, sizeof(int))) != 0)
         || ((munmap(sharedN[0], sizeof(int))) != 0)
         || ((munmap(sharedN[1], sizeof(int))) != 0)
         || ((munmap(sharedSEAT, sizeof(int))) != 0) )
    {
        fprintf(stderr, "%s", ERR_MSG[MUNMAP]);
        fail++;
    }


    /* UNLINKING SHARED MEMORY:
    Unlinks all shared memory.

    If unlinking of any fails, error message is sent to stderr and fail counter is raised by 1.
    */
    if (    (shm_unlink("/xsusov01_sharedMemory0") == SYS_FAIL)
         || (shm_unlink("/xsusov01_sharedMemory1") == SYS_FAIL)
         || (shm_unlink("/xsusov01_sharedMemory2") == SYS_FAIL)
         || (shm_unlink("/xsusov01_sharedMemory3") == SYS_FAIL) )
    {
        fprintf(stderr, "%s", ERR_MSG[SHM_UNLINK]);
        fail++;
    }

    /*  CLOSING FILE DESCRIPTORS OF SHARED MEMORY:
    Close all file descriptors of shared memory.

    If closing of any fails, error message is sent to stderr and fail counter is raised by 1.
    */
    if (    (close(shm_fd0) == SYS_FAIL)
         || (close(shm_fd1) == SYS_FAIL)
         || (close(shm_fd2) == SYS_FAIL)
         || (close(shm_fd3) == SYS_FAIL) )
    {
        fprintf(stderr, "%s", ERR_MSG[SHM_CLOSE]);
        fail++;
    }

    /*  CLOSING OUTPUT FILE:
    Close output file (rivercrossing.out).

    If closing fails, error message is sent to stderr and fail counter is raised by 1.
    */
    if (fclose(fp) == EOF)
    {
        fprintf(stderr, "%s", ERR_MSG[FCLOSE]);
        fail++;
    }

    return fail;             // 0 if everything went right.
}


/* @killme void ***************************************************************
**
** @brief Safety function which clean mess in case of interupt.
**
**  Function is called after SIGNAL is trapped.
**  Function kills process and all of its children (as it is inherited by all forks).
**  Also calls function, which free resources (valar_morghulis()).
**
** @param[in] int sig - signal which sett off trap.
** @see     valar_morghulis()
** @return signal
** @@
******************************************************************************/
void killme(int sig)
{
    fprintf(stderr, "%s %d.\n", ERR_MSG[SIGNAL], sig);
    valar_morghulis();
    kill(getpid(), SIGINT);
    exit(sig);
}


/* @fork_forge int *******************************************************
**
** @brief Forking function forks children processes of choosen category.
**
**        Function forks new children fork in random time from interval
**  from 0 seconds to time stated by program's argument (depends on process
**  family: hacker/serf).
**
** @param[in] FILE *fp - output file rivercrossing.out, which is passed to children.
** @param[in] param *pm - pointer to structure param, which holds values of all four program's arguments.
** @param[in] int sleep - maximum time of pause between two forks.
** @param[in] int category - process faimly - HACKER or SERF - of which children processes are generated.
** @return 2 when forking process failed.
** @@
******************************************************************************/
int fork_forge(FILE *fp, param *pm, int sleep, int category)
{

    pid_t pid_fork;
    int i = 0;

    sleep++;                 // Prevents (random() % 0) to occur.

    while (i < pm -> p)
    {
        i++;

        usleep((random() % (sleep)) * 1000);
                             // Forking will happen in random time from 0
                             // ms to maximum time to generate child-process
                             // of given category.
        pid_fork = fork();

        if (pid_fork == 0)
        {                    // Forked child-process of given category.
            fork_child(fp, pm, category, i, sharedCounter);
        }

        else if (pid_fork < 0)
        {                   // Forking child-process failed.
            fprintf(stderr, "%s", ERR_MSG[FORK]);
            return EXIT_SYS_FAILURE;
        }

    }

    waitpid(-1, NULL, 0);
    exit(0);
}


/* @fork_child void *******************************************************
**
** @brief Function to handle child process output.
**
**        Function is common for both hacker and serf child processes.
**  Depending on system of process managment (semaphores for exclusive access
**  to shared resources and process synchronization) allows child process
**  writing to output file and changing value of shared variables.
**
**  Each process has six steps:
**            1)start - right after process is created
**            2)waiting for boarding - process waits till crossing is finished
**              and appropriate group of process is gathered.
**            3)boarding - when selected group of processes boards ship -
**             last one to board becomes a captain.
**            4)member/captain - after captain is determined, its put to sleep
**              for time equivalent of rivercrossing time, afterwards signals to
**              all members that its safe to land.
**            5)landing - processes leave ship and pier is ready for gathering
**              of another processes, which would like to cross the river.
**            6)finish - after all processes have safely crossed, each sends its
**              finish message and ends.
**
**
** @param[out] FILE *fp - output file rivercrossing.out where are process reports logged.
** @param[in] param *pm - pointer to structure param, which holds values of all four program's arguments.
** @param[in] int category - process faimly - HACKER or SERF - of current child process.
** @param[in] int i - Process ID within its family (first process of its family has ID == 1, and so on)
** @param[in/out] int *y - Pointer on shared memory varieble which counts actions
**             of all processes for numbering reports written to log.
** @
** @see fork_forge()
** @@
******************************************************************************/
void fork_child(FILE *fp, param *pm, int category, int i, int *y)
{
    sem_wait(sem_log);       // Child-process was created and reports it to the log.
        fprintf(fp, "%-3d: %-15s: %d: started\n", counter(y, 1), CATEGORY[category], i);
    sem_post(sem_log);


    sem_wait(sem_molo);     // To eneter the pier, processes must wait till current rivercrossing is over.

    sem_wait(sem_log);      // After entering the pier, process raise counter of waiting processes of its category and reports to the log.
        counter(sharedN[category], MEMBER);
        fprintf(fp, "%-3d: %-15s: %d: waiting for boarding: %d: %d\n", counter(y, 1), CATEGORY[category], i, *sharedN[0], *sharedN[1]);
    sem_post(sem_log);

    int prio = set_course_for_intercourse();
                             // Priority Member/Captain is given to process by function set_course_for_intercourse.

    if (category == HACKER)
    {                        // Each processes category has quee of its own to protect from waittting-zombie-process to infiltrate their group.
        sem_wait(sem_hacker_wait);
    }
    else
    {
        sem_wait(sem_serf_wait);
    }


    sem_wait(sem_log);       // After valid group is allowed to enter the ship, processes start boarding and report to the log.
        fprintf(fp, "%-3d: %-15s: %d: boarding: %d: %d\n", counter(y, 1), CATEGORY[category], i, *sharedN[0], *sharedN[1]);
    sem_post(sem_log);


    get_on_board(sharedSEAT, sem_ride, SHIP_CAPACITY, SHIP_CAPACITY);
                             // After all members are on board rivercrossing is initializated.

    sem_wait(sem_ride);

    if (prio == MEMBER)
    {
        sem_wait(sem_log);   // First three processes to board ship becomes members and report it to the log.
            fprintf(fp, "%-3d: %-15s: %d: member\n", counter(y, 1), CATEGORY[category], i);
        sem_post(sem_log);
    }

    else if (prio == CAPTAIN)
    {
        sem_wait(sem_log);   // Last one to board ship becomes the ship's captain and reports it to the log.
            fprintf(fp, "%-3d: %-15s: %d: captain\n", counter(y, 1), CATEGORY[category], i);
        sem_post(sem_log);

        srand(time(NULL)*getpid());
                             // Randomize random number generator.

        usleep((random() % (pm -> r + 1)) * 1000);
                             // Captain is put to sleep for time needed to perform crossing the river.
    }

    get_on_board(sharedSEAT, sem_landing, SHIP_CAPACITY, SHIP_CAPACITY);
                             // Ship members waits for its captain to land.

    sem_wait(sem_landing);

    sem_wait(sem_log);       // After crossing the river is finished, processes land and report it to the log.
        fprintf(fp, "%-3d: %-15s: %d: landing: %d: %d\n", counter(y, 1), CATEGORY[category], i, *sharedN[0], *sharedN[1]);
    sem_post(sem_log);

    get_on_board(sharedSEAT, sem_molo, MEMBER, SHIP_CAPACITY);
    // After all landed successfully, next process is allowed to enter the pier.

    sem_wait(sem_counter);
        if (*sharedCounter == pm -> p * 10)
        {                        // Checks if all processes crossed the river, if so, unlocks finish semaphore for them.
                                 // If not, waits for last process to cross the river and do it.
            sem_shop(sem_finish, pm -> p * 2);
        }
    sem_post(sem_counter);

    sem_wait(sem_finish);    // FINISH LINE.

    sem_wait(sem_log);       // If all processes crossed the river, process can report to the log, that its finished.
        fprintf(fp, "%-3d: %-15s: %d: finished\n", counter(y, 1), CATEGORY[category], i);
    sem_post(sem_log);

                             // At long last. No process crosses the river forever, my child-process.
                             // NOW GO. LEAVE THIS PLACE – AND NEVER RETURN.
    exit(0);
}


/* @counter int****************************************************************
**
** @brief Function to handle changing value of shared variable.
**
**        Function is closes semaphore for access to shared memory, change value
**  of choosen variable, stores new value in its local variable and open semaphore.
**  New value (stored in local variable) is returned.
**
**
** @param[in/out] int *x - variable (shared) targeted for changing.
** @param[in] int y - number (integer) which is added to variable.
** @return new value
** @@
******************************************************************************/
int counter(int *x, int y)
{
    int cache = 0;

    sem_wait(sem_counter);
    (*x) = (*x) + y;
    cache = *x;              // New value is stored in local variable to return.
    sem_post(sem_counter);

    return cache;
}


/* @set_course_for_intercourse int********************************************
**
** @brief Function to check, if appropriate group has gathered.
**
**        Function is checks amount of processes of each family waiting on pier.
**  For appropriate group there must be: 4 HACKERS
**                                       4 SERFS
**  or                                   2 HACKERS and 2 SERFS
**
**  When group is created semaphores of necessary amount and category are opened.
**  If group cannot be created semaphore is opened to let another process to pier.
**  It's used to determine last boarding one process which becomes
**  captain.
**
** @see sem_shop()
** @return 2 - if last of approprite group for crossing (Captain).
** @return 1 - if not.
** @@
******************************************************************************/
int set_course_for_intercourse(void)
{

    if (*sharedN[HACKER] >= 4)
    {                        // Group of 4 HACKERS - valid for crossing the river.
        counter(sharedN[HACKER], -4);

        sem_shop(sem_hacker_wait, 4);
        return CAPTAIN;
    }

    else if (*sharedN[SERF] >= 4)
    {                        // Group of 4 SERFS - valid for crossing the river.
        counter(sharedN[SERF], -4);

        sem_shop(sem_serf_wait, 4);
        return CAPTAIN;
    }


    else if ((*sharedN[HACKER] >= 2) && (*sharedN[SERF] >= 2))
    {                        // Group of 2 HACKERS & 2 SERFS - valid for crossing the river.
        counter(sharedN[HACKER], -2);
        counter(sharedN[SERF], -2);

        sem_shop(sem_hacker_wait, 2);
        sem_shop(sem_serf_wait, 2);
        return CAPTAIN;
    }

    else
    {                        // No valid group for crossing the river could be assembled.
        sem_post(sem_molo);  // Unlocks semaphore that allows another process to enter the pier and try assemble valid group again.
        return MEMBER;
    }
}


/* @get_on_board void********************************************
**
** @brief Function to check, if all members are ready.
**
**        Loads shared variable (amount of processes), increments it
**  and if its equal to needed value, opens appropriate amount of
**  choosen kind of semaphores and sets shared variable to zero.
**
** @param[in] int *seat - pointer to shared variable which is tested
** @param[out] sem_t *sem_target - semaphores which will be opened,
**             if test succedes
** @param[in] in free_seat - number of how many semaphores will be opened
** @param[in] int num - value which is shared variable tested on
** @see sem_shop()
** @@
******************************************************************************/
void get_on_board(int *seat, sem_t *sem_target, int free_seat, int num)
{

    counter(seat, 1);        // Raise (safely) shared value (seat counter) by 1.
    sem_wait(sem_counter);

    if (*seat == num)
    {
        (*seat) = 0;
        sem_shop(sem_target, free_seat);
                             // Will unlock needed amount of given semaphores in loop.
    }

    sem_post(sem_counter);
}


/* @sem_shop void**************************************************************
**
** @brief Function to open multiple semaphores (of given kind) in loop.
**
**        Opens choosen amount of semaphores of choosen kind in for-loop.
**  Used in other function handling semaphores and fe. finishing phase.
**
** @param[in] sem_t *sem_target - semaphores which will be opened,
** @param[in] int free_seat - number of how many semaphores will be opened
** @see sem_shop()
** @return 2 - if needed value is reached. (Captain).
** @return 1 - if not.
** @@
******************************************************************************/
void sem_shop(sem_t *sem_target, int free_seat)
{

    int pass;

    for(pass = 0; pass < free_seat; pass++)
    {
        sem_post(sem_target);
    }
}

/** Tomas Susovsky 2014/05/04 **/