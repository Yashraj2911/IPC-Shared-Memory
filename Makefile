all: IPC-pipe IPC-shmem
CC=gcc
CFLAGS=-O3 -march=native -falign-functions -w

IPC-pipe: IPC-pipe.o
	$(CC) -o IPC-pipe IPC-pipe.o

IPC-shmem: IPC-shmem.o
	$(CC) -o IPC-shmem IPC-shmem.o

.PHONY: clean
clean:
	rm -f *.o IPC-pipe IPC-shmem
