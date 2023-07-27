#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>

#define A1NAME "/warnsdorff"
#define A2NAME "/matrix"
#define B1NAME "/justget"
#define B2NAME "/overman"
#define MAT2TRANSPOSE "matrix2-transposed.txt"

typedef struct {
	int index_start, index_end, index;
} calc_argument;

int mi, mj, mk;
int* shmPtr;
int *fposn1, *fposn2;
int *matrix1, *matrix2;
int** matrix3;
int *switchingStatus, *semADirxn, *semBDirxn;
int** cellsDone;
char *in1, *in2, *out;
int calc_n;
sem_t *sem_a1, *sem_a2, *sem_b1, *sem_b2;
int thread_count = 0;
int doneCount = 0;
pthread_mutex_t lock;
pthread_mutex_t exitLock;


void* calc_result(void* func_args) {
	calc_argument* arg = (calc_argument*) func_args;
	int total_index = arg->index_end - arg->index_start;
	sem_wait(sem_b2);
	sem_post(sem_b2);
	int count = 0;
	int prev_count = 0;
	int flag = 0;
	int flag2 = 0;
	int flag3 = 0;

	while(doneCount != calc_n) {
		for(int i = arg->index_start; i < arg->index_end && *shmPtr == 1 && count < total_index; ++i) {
			if(cellsDone[i/mk][i%mk] || fposn1[i/mk] != -1 || fposn2[i%mk] != -1)	
				continue;
			matrix3[i/mk][i%mk] = 0;
			for(int j = 0; j < mj; ++j) {
				matrix3[i/mk][i%mk] += (matrix1[(i/mk)*mj+j]*matrix2[(i%mk)*mj+j]);		// matrix3[i/mk][i%mk] += matrix1[i/mk][j]*matrix2[i%mk][j]
			}
			cellsDone[i/mk][i%mk] = 1;
			count++;
		}
		if(*shmPtr == 0 && shmPtr[1] == 1 && doneCount != calc_n) {
			if(*semBDirxn == 1) {
				pthread_mutex_lock(&lock);
				thread_count++;
				if(thread_count == calc_n) {
					sem_wait(sem_a2);
					*switchingStatus = 0;
					sem_post(sem_a1);
				}
				pthread_mutex_unlock(&lock);
				sem_wait(sem_b1);
				flag3 = 0;
				sem_post(sem_b1);
			} else if(*semBDirxn == 0) {
				pthread_mutex_lock(&lock);
				thread_count--;
				if(thread_count == 0) {
					sem_wait(sem_a1);
					*switchingStatus = 0;
					sem_post(sem_a2);
				}
				pthread_mutex_unlock(&lock);
				sem_wait(sem_b2);
				flag3 = 0;
				sem_post(sem_b2);
			}
		}
		if(count == total_index) {
			count++;
			pthread_mutex_lock(&exitLock);
			doneCount++;
			pthread_mutex_unlock(&exitLock);
		}
	}

	sem_post(sem_a1);
	sem_post(sem_a2);
	return NULL;
}

void roundRobin() {
	cellsDone = (int**) malloc(mi*sizeof(int*));
	matrix3 = (int**) malloc(mi*sizeof(int*));
	for(int i = 0; i < mi; ++i) {
		matrix3[i] = (int*) malloc(mk*sizeof(int));
		if(matrix3[i] == NULL)	printf("Failed at malloc: %d of matrix3.\n", i);
		cellsDone[i] = (int*) malloc(mk*sizeof(int));
		if(matrix3[i] == NULL)	printf("Failed at malloc: %d of cellsDone.\n", i);
		for(int j = 0; j < mk; ++j)	cellsDone[i][j] = 0;
	}


	pthread_t calc_tid[calc_n];
	calc_argument calc_args[calc_n];
	for(int i = 0; i < calc_n; ++i) {
		calc_args[i].index_start = (i*(mi*mk))/calc_n;
		calc_args[i].index_end = ((i+1)*(mi*mk))/calc_n;
		calc_args[i].index = i;
		pthread_create(&calc_tid[i], NULL, calc_result, (void*) &calc_args[i]);
	}
	for(int i = 0; i < calc_n; ++i) {
		pthread_join(calc_tid[i], NULL);
	}
	shmPtr[1] = 0;
	free(cellsDone);
}


int main(int argc, char* argv[]) {
	mi = atoi(argv[1]);
	mj = atoi(argv[2]);
	mk = atoi(argv[3]);
	in1 = argv[4];
	in2 = argv[5];
	out = argv[6];
	calc_n = atoi(argv[7]);

	/* Opening the Named Semaphores */
	sem_a1 = sem_open(A1NAME, 0);
    sem_a2 = sem_open(A2NAME, 0);
	sem_b1 = sem_open(B1NAME, 0);
	sem_b2 = sem_open(B2NAME, 0);
	if(sem_a1 == SEM_FAILED) {
		perror("[C1] ERROR! sem_open(A1NAME) failed");
		exit(-1);
	}
	if(sem_a2 == SEM_FAILED) {
		perror("[C1] ERROR! sem_open(A2NAME) failed");
		exit(-1);
	}
	if(sem_b1 == SEM_FAILED) {
		perror("[C1] ERROR! sem_open(B1NAME) failed");
		exit(-1);
	}
	if(sem_b2 == SEM_FAILED) {
		perror("[C1] ERROR! sem_open(B2NAME) failed");
		exit(-1);
	}

	/* Opening the Shared Memory */
	key_t key = ftok(argv[4], 43);
	if(key == -1) {
		perror("[C2] ERROR! ftok()");
		exit(-1);
	}
	int shmid = shmget(key, 1, 0);
	if(shmid == -1) {
		perror("[C2] ERROR! shmget()");
		if(errno == ENOENT) {
			perror("[C2] ENOENT in shmget()");
		} else if(errno == EINVAL) {
			perror("[C2] EINVAL in shmget()");
		}
		exit(-2);
	}
	shmPtr = shmat(shmid, NULL, 0);
	if(shmPtr == (void*) -1) {
		perror("[C2] ERROR! shmat()");
		exit(-2);
	}
	fposn1 = shmPtr+(2+(mi+mk)*mj);
	fposn2 = shmPtr+(2+(mi+mk)*mj+mi);
	matrix1 = shmPtr+2;
	matrix2 = shmPtr+2+mi*mj;
	switchingStatus = shmPtr+(2+(mi+mk)*mj+(mi+mk));
	semADirxn = shmPtr+(2+(mi+mk)*mj+(mi+mk)+1);
	semBDirxn = shmPtr+(2+(mi+mk)*mj+(mi+mk)+2);
	pthread_mutex_init(&lock, NULL);
	pthread_mutex_init(&exitLock, NULL);


	/* Main Work */
	roundRobin();
	FILE* fptr = fopen(argv[6], "w");
	for(int i = 0; i < mi; ++i) {
		for(int j = 0; j < mk; ++j) {
			// if(j == mk-1)	fprintf(fptr, "%d", matrix3[i][j]);
			// else	fprintf(fptr, "%d ", matrix3[i][j]);
			fprintf(fptr, "%d ", matrix3[i][j]);
		}
		fprintf(fptr, "\n");
	}
	fclose(fptr);

	return 0;
}
