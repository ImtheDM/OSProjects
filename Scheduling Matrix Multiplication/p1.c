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

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define A1NAME "/warnsdorff"
#define A2NAME "/matrix"
#define B1NAME "/justget"
#define B2NAME "/overman"
#define MAT2TRANSPOSE "matrix2-transposed.txt"

typedef struct {
	int line_start, line_end, index;
} read_argument;

int mi, mj, mk;
int* shmPtr;
int *fposn1, *fposn2;
int *matrix1, *matrix2;
char *in1, *in2, *out;
int read_n;
sem_t *sem_a1, *sem_a2, *sem_b1, *sem_b2;
int *switchingStatus, *semADirxn, *semBDirxn;
int thread_count = 0;
int doneCount = 0;
pthread_mutex_t lock;
pthread_mutex_t exitLock;

void* reading_file(void* func_args) {
	read_argument* arg = (read_argument*) func_args;
	int lend = arg->line_end;
	int lposn = arg->line_start;
	int count = 0, total_lines = lend-lposn;
	int prev_doneCount = 0;
	
	while(prev_doneCount != read_n && shmPtr[1] == 1) {
		if(lposn < mi && count < total_lines && *shmPtr == 0) {
			FILE* fread = fopen(in1, "r");
			fseek(fread, fposn1[lposn], SEEK_SET);
			
			for(; lposn < MIN(lend, mi) && *shmPtr == 0; ++lposn) {
				for(int j = 0; j < mj; ++j) {
					fscanf(fread, "%d", &matrix1[lposn*mj+j]);	// matrix1[lposn][j]
				}
				fposn1[lposn] = -1;
				count++;
			}
			fclose(fread);
		}
		if(lposn >= mi && lposn < lend && count < total_lines && *shmPtr == 0) {
			FILE* fread = fopen(MAT2TRANSPOSE, "r");
			fseek(fread, fposn2[lposn-mi], SEEK_SET);
			
			for(; lposn < lend && *shmPtr == 0; ++lposn) {
				for(int j = 0; j < mj; ++j) {
					fscanf(fread, "%d", &matrix2[(lposn-mi)*mj+j]);	// matrix2[lposn-mi][j]
				}
				fposn2[lposn-mi] = -1;
				count++;
			}
			fclose(fread);
		}	
		if(*shmPtr == 1 && prev_doneCount < read_n) {
			if(*semADirxn == 1) {
				pthread_mutex_lock(&lock);
				thread_count++;
				if(thread_count == read_n) {
					sem_wait(sem_b1);
					*switchingStatus = 0;
					sem_post(sem_b2);
				}
				pthread_mutex_unlock(&lock);
				sem_wait(sem_a1);
				sem_post(sem_a1);
				prev_doneCount = doneCount;
			} else if(*semADirxn == 0 && prev_doneCount != read_n) {
				pthread_mutex_lock(&lock);
				thread_count--;
				if(thread_count == 0) {
					sem_wait(sem_b2);
					*switchingStatus = 0;
					sem_post(sem_b1);
				}
				pthread_mutex_unlock(&lock);
				sem_wait(sem_a2);
				sem_post(sem_a2);
				prev_doneCount = doneCount;
			}
		}
		if(count == total_lines) {
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

int main(int argc, char* argv[]) {
	mi = atoi(argv[1]);
	mj = atoi(argv[2]);
	mk = atoi(argv[3]);
	in1 = argv[4];
	in2 = argv[5];
	out = argv[6];
	read_n = atoi(argv[7]);

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
		perror("[C1] ERROR! ftok()");
		exit(-1);
	}
	int shmid = shmget(key, 1, 0);
	if(shmid == -1) {
		perror("[C1] ERROR! shmget()");
		if(errno == ENOENT) {
			perror("[C1] ENOENT in shmget()");
		} else if(errno == EINVAL) {
			perror("[C1] EINVAL in shmget()");
		}
		exit(-2);
	}
	shmPtr = shmat(shmid, NULL, 0);
	if(shmPtr == (void*) -1) {
		perror("[C1] ERROR! shmat()");
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
	pthread_t* read_tid = malloc(read_n*sizeof(pthread_t));
	read_argument* read_args = malloc(read_n*sizeof(read_argument));

	for(int i = 0; i < read_n; ++i) {
		read_args[i].line_start = (i*(mi+mk))/read_n;
		read_args[i].line_end = ((i+1)*(mi+mk))/read_n;	
		read_args[i].index = i;
		pthread_create(&read_tid[i], NULL, reading_file, (void*) &read_args[i]);
	}

	for(int i = 0; i < read_n; ++i) {
		pthread_join(read_tid[i], NULL);
	}
	shmPtr[1] = 0;
	switchingStatus[0] = 0;
	return 0;
}
