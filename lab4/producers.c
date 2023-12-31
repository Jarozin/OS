#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>

#define BUFSIZE 128
#define PERMS S_IRWXU | S_IRWXG | S_IRWXO
#define NP 3
#define NC 3
#define SE 0
#define SF 1
#define SB 2

struct sembuf start_produce[2] = {{SE, -1, 0}, {SB, -1, 0}};
struct sembuf stop_produce[2] = {{SB, 1, 0}, {SF, 1, 0}};
struct sembuf start_consume[2] = {{SF, -1, 0}, {SB, -1, 0}};
struct sembuf stop_consume[2] = {{SB, 1, 0}, {SE, 1, 0}};

int semid;
char *addr;
char **ptr_prod;
char **ptr_cons;
char *conv;
char *ch;

int flag = 1;
void sig_handler(int sig_num)
{
    flag = 0;
    printf("catch PID = %d, signal = %d\n", getpid(), sig_num);
}

void producer(const int semid)
{
    while (flag)
    {
        sleep(rand() % 3);
        int p = semop(semid, start_produce, 2);
        if (p == -1)
        {
            perror("p semop error p\n");
            exit(1);
        }
        **ptr_prod = *ch;
        printf("Producer %d >>> %c (%p)\n", getpid(), **ptr_prod, *ptr_prod);
        if (*ch == 'z') {
            *ch = 'a';
        } else {
            (*ch)++;
        }
        (*ptr_prod)++;
        int v = semop(semid, stop_produce, 2);
        if (v == -1)
        {
            perror("p semop error v\n");
            exit(1);
        }
    }
    exit(0);
}

void consumer(const int semid)
{
    while (flag)
    {
        sleep(rand() % 3);
        int p = semop(semid, start_consume, 2);
        if (p == -1)
        {
            perror("c semop error p\n");
            exit(1);
        }
        printf("Consumer %d <<< %c (%p)\n", getpid(), **ptr_cons, *ptr_cons);
        (*ptr_cons)++;
        int v = semop(semid, stop_consume, 2);
        if (v == -1)
        {
            perror("c semop error v\n");
            exit(1);
        }
    }
    exit(0);
}

int main()
{
    signal(SIGINT, sig_handler);

    srand(((int)time(NULL)));
    int memkey = 0;
    int status;
    pid_t child_pid;
    int fd = shmget(memkey, BUFSIZE, IPC_CREAT | PERMS);
    if (fd == -1)
    {
        perror("shmget\n");
        exit(1);
    }

    addr = (char *)shmat(fd, 0, 0);
    if (addr == (char *)-1)
    {
        perror("shmat\n");
        exit(1);
    }
    ptr_prod = (char **)addr;
    ptr_cons = (char **)((char *)ptr_prod + sizeof(char *));
    ch = (char *)ptr_cons + sizeof(char *);
    *ch = 'a';
    conv = ch + sizeof(char);
    *ptr_prod = conv;
    *ptr_cons = conv;

    int semkey = ftok("key_file", 0);
    if ((semid = semget(semkey, 3, IPC_CREAT | PERMS)) == -1)
    {
        perror("semget\n");
        exit(1);
    }
    int cse = semctl(semid, SE, SETVAL, BUFSIZE);
    int csf = semctl(semid, SF, SETVAL, 0);
    int csb = semctl(semid, SB, SETVAL, 1);
    if (cse == -1 || csf == -1 || csb == -1)
    {
        perror("semctl\n");
        exit(1);
    }

    pid_t pid = -1;
    for (int i = 0; i < NP; i++)
    {
        if ((pid = fork()) == -1)
        {
            perror("p can't fork\n");
            exit(1);
        }
        if (pid == 0)
        {
            producer(semid);
        }
    }
    for (int i = 0; i < NC; i++)
    {
        if ((pid = fork()) == -1)
        {
            perror("c can't fork\n");
            exit(1);
        }
        if (pid == 0)
        {
            consumer(semid);
        }
    }

    for (int i = 0; i < (NP + NC); i++)
    {
        child_pid = wait(&status);
        if (WIFEXITED(status))
            printf("Child PID = %d exit with code %d\n", child_pid, WEXITSTATUS(status));
        else if (WIFSIGNALED(status))
            printf("Child PID = %d terminate, recieved signal %d\n", child_pid, WTERMSIG(status));
        else if (WIFSTOPPED(status))
            printf("Child PID = %d\n stop, recieved signal %d\n", child_pid, WSTOPSIG(status));
    }

    if (shmdt(addr) == -1)
    {
        perror("shmdt\n");
        exit(1);
    }
}
