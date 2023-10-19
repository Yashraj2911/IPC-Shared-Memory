#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/time.h>

#define MAX_SIZE 10000
#define CHILD_PROCESSES 12
#define POOL_SIZE (MAX_SIZE * MAX_SIZE * 3) * sizeof(int)

void *pool;
static size_t next_free_offset = 0;

void *allocate(size_t size)
{
    int next = next_free_offset;
    next_free_offset += size / sizeof(int);
    return (void *)((int *)pool + next);
}

double getdetlatimeofday(struct timeval *begin, struct timeval *end)
{
    return (end->tv_sec + end->tv_usec * 1.0 / 1000000) -
           (begin->tv_sec + begin->tv_usec * 1.0 / 1000000);
}

void swap(int *a, int *b)
{
    *a = *a ^ *b;
    *b = *a ^ *b;
    *a = *a ^ *b;
}

void transpose(int *M)
{
    int i, j;
    for (i = 0; i < MAX_SIZE; i++)
        for (j = 0; j < i; j++)
            swap(&M[i * MAX_SIZE + j], &M[j * MAX_SIZE + i]);
}

int check(int *Q, int *M, int *N)
{
    for (int i = 0; i < MAX_SIZE; i++)
    {
        for (int j = 0; j < MAX_SIZE; j++)
        {
            register int sum = 0;
            for (int k = 0; k < MAX_SIZE; k++)
                sum += M[i * MAX_SIZE + k] * N[j * MAX_SIZE + k];
            if (Q[i * MAX_SIZE + j] != sum)
                return 0;
        }
    }
    return 1;
}

int main()
{
    // Allocating a contiguous pool to leverage spatial locality
    pool = malloc(POOL_SIZE);
    if (pool == NULL)
    {
        perror("Error creating memory pool");
        exit(1);
    }
    struct timeval begin, finish;

    // Allocating memory to the arrays from the contiguous pool
    int *M = (int *)allocate((MAX_SIZE * MAX_SIZE) * sizeof(int));
    int *N = (int *)allocate((MAX_SIZE * MAX_SIZE) * sizeof(int));

    for (int i = 0; i < MAX_SIZE * MAX_SIZE; i++)
    {
        M[i] = rand() % 10;
        N[i] = rand() % 10;
    }

    // To ensure row-wise access of elements
    transpose(N);

    int id = shmget(IPC_PRIVATE, (MAX_SIZE * MAX_SIZE * sizeof(int)), IPC_CREAT | 0666);

    if (id < 0)
    {
        perror("shmget");
        exit(1);
    }

    int *Q = (int *)shmat(id, NULL, 0);

    pid_t pid[CHILD_PROCESSES];

    int curr_row[MAX_SIZE];
    int rows_per_process = MAX_SIZE / CHILD_PROCESSES;

    gettimeofday(&begin, NULL);

    for (int i = 0; i < CHILD_PROCESSES; i++)
    {
        pid[i] = fork();
        if (pid[i] < 0)
        {
            perror("fork");
            exit(1);
        }
        else if (pid[i] == 0)
        {

            int start_row = i * rows_per_process;
            int remainder = MAX_SIZE % CHILD_PROCESSES;
            int end_row = start_row + rows_per_process + 1 / (CHILD_PROCESSES - i) * remainder;
            for (int l = start_row; l < end_row; l++)
            {
                for (int k = 0; k < MAX_SIZE; k++)
                {
                    int sum = 0;
                    for (int j = 0; j < MAX_SIZE; j++)
                    {
                        sum += M[l * MAX_SIZE + j] * N[k * MAX_SIZE + j];
                    }
                    Q[l * MAX_SIZE + k] = sum;
                }
            }
            exit(0);
        }
    }

    for (int i = 0; i < MAX_SIZE; i++)
        wait(NULL);

    gettimeofday(&finish, NULL);

    if (check(Q, M, N))
        printf("Matrix multiplication completed successfully.\n");
    else
        printf("Wrong output\n");

    double elapsedTime = getdetlatimeofday(&begin, &finish);
    printf("Time taken to perform multiplication: %f seconds\n", elapsedTime);

    shmdt(Q);
    shmctl(id, IPC_RMID, NULL);
    return 0;
}
