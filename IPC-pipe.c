#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
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

    int pipes[CHILD_PROCESSES][2];

    for (int i = 0; i < CHILD_PROCESSES; i++)
    {
        if (pipe(pipes[i]) == -1)
        {
            perror("Error creating pipe");
            exit(1);
        }
    }

    // The total number of rows are equally divided between the worker processes
    int rows_per_process = MAX_SIZE / CHILD_PROCESSES;
    int remainder = MAX_SIZE % CHILD_PROCESSES;
    int curr_row[MAX_SIZE + 1];

    pid_t pid;

    gettimeofday(&begin, NULL);

    for (int i = 0; i < CHILD_PROCESSES; i++)
    {
        pid = fork();
        if (pid == -1)
        {
            perror("Error creating child process");
            exit(1);
        }
        else if (pid == 0)
        {
            close(pipes[i][0]);
            int start_row = i * rows_per_process;
            int end_row = start_row + rows_per_process + 1 / (CHILD_PROCESSES - i) * remainder;
            for (int l = start_row; l < end_row; l++)
            {
                for (int k = 0; k < MAX_SIZE; k++)
                {
                    register int sum = 0;
                    for (int j = 0; j < MAX_SIZE; j++)
                    {
                        sum += M[l * MAX_SIZE + j] * N[k * MAX_SIZE + j];
                    }
                    curr_row[k + 1] = sum;
                }
                curr_row[0] = l;
                // The write granularity is one row
                write(pipes[i][1], curr_row, (MAX_SIZE + 1) * sizeof(int));
            }
            close(pipes[i][1]);
            exit(0);
        }
        else
        {
            close(pipes[i][1]);
        }
    }

    int *Q = allocate((MAX_SIZE * MAX_SIZE) * sizeof(int));
    int rows_read = MAX_SIZE;

    // The reader process continuously polls the pipes to see if data can be read until all the rows are read
    while (rows_read)
        for (int i = 0; i < CHILD_PROCESSES; i++)
            if (read(pipes[i][0], curr_row, (MAX_SIZE + 1) * sizeof(int)))
            {
                rows_read--;
                for (int a = 0; a < MAX_SIZE; a++)
                    Q[curr_row[0] * MAX_SIZE + a] = curr_row[a + 1];
            }
            else
            {
                sleep(0);
            }

    gettimeofday(&finish, NULL);

    for (int i = 0; i < CHILD_PROCESSES; i++)
        close(pipes[i][0]);

    if (check(Q, M, N))
        printf("Matrix multiplication completed successfully.\n");
    else
        printf("Wrong output\n");

    double elapsedTime = getdetlatimeofday(&begin, &finish);
    printf("Time taken to perform multiplication: %f seconds\n", elapsedTime);

    free(pool);
    return 0;
}