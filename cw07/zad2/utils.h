#include <stdlib.h>
#include <semaphore.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <sys/mman.h>

#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>

#define keyForPathname 120321

#define PID_PROJ_ID 110
#define QUEUE_PROJ_ID 111
#define SHMVAR_PROJ_ID 120

#define SEM_FAS 0   // 0
#define SEM_D 1     // 1
#define SEM_Q 2     // 1
#define SEM_C 3     // 0

#define SEM_NMB 4

#define SHMNMB 6

#define THE_ONLY_ONE 0
#define FROM_QUEUE 1

#define PID_SHM     "pid_shm"
#define VARTAB_SHM  "vartab_shm"

#define GRN   "\x1B[32m"
#define RES   "\x1B[0m"
#define RED   "\x1B[31m"
#define BLU    "\e[44m"


union semun {
    int              val;    /* Value for SETVAL */
    struct semid_ds *buf;    /* Buffer for IPC_STAT, IPC_SET */
    unsigned short  *array;  /* Array for GETALL, SETALL */
    struct seminfo  *__buf;  /* Buffer for IPC_INFO (Linux-specific) */
};

struct msgp
{
    long mtype;
    pid_t pid;
};
