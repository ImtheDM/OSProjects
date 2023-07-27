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
int read_n = 5, calc_n = 5;
int mi, mj, mk;
int* shmPtr;
int *fposn1, *fposn2;
sem_t *sem_a1, *sem_a2, *sem_b1, *sem_b2;
char *in1, *in2, *out;
int* switchingStatus, *semADirxn, *semBDirxn;
struct timespec begin, end;

/*
	SHMEM LAYOUT:
	whichProcessTurn, 
	runningStatus, 		// 1 - not over yet. 	0 - reading completed.
	mi*mj elements of matrix 1, 
	mk*mj elements of matrix 2,
	mi elements for fposn1,
	mk elements for fposn2,
	switchingStatus,		
	semADirxn,
	semBDirxn

*/

void input_preprocess() {
	/* Opening file-streams */
	FILE *file1, *file2;
	file1 = fopen(in1, "r");
	if(file1 == NULL) {
		perror("[S] Not able to open in1.txt");
		exit(1);
	}
	file2 = fopen(in2, "r");
	if(file2 == NULL) {
		perror("[S] Not able to open in2.txt");
		exit(1);
	}
	
	/* Taking transpose of the second matrix and storing it in a separate file */
	int** mat = (int**) malloc(mj*sizeof(int*));
	for(int i = 0; i < mj; ++i) {
		mat[i] = (int*) malloc(mk*sizeof(int));
		if(mat[i] == NULL)	printf("Error in malloc at index %d, in input_preprocess function.\n", i);
	}
	
	for(int i = 0; i < mj; ++i) {
		for(int j = 0; j < mk; ++j) {
			fscanf(file2, "%d", &mat[i][j]);
		}
	}
	fclose(file2);
	
	file2 = fopen(MAT2TRANSPOSE, "w");
	for(int i = 0; i < mk; ++i) {
		for(int j = 0; j < mj; ++j) {
			if(j == mj-1)	fprintf(file2, "%d", mat[j][i]);
			else	fprintf(file2, "%d ", mat[j][i]);
		}
		fprintf(file2, "\n");
	}
	fclose(file2);
	free(mat);
	
	/* Indexing each line of the two text files */
	file2 = fopen(MAT2TRANSPOSE, "r");
	rewind(file1);
	rewind(file2);
	for(int i = 0; i < mi; ++i) {
		fposn1[i] = ftell(file1);		
		fscanf(file1, "%*[^\n] ");
	}
	for(int i = 0; i < mk; ++i) {
		fposn2[i] = ftell(file2);
		fscanf(file2, "%*[^\n] ");
	}

	fclose(file1);
	fclose(file2);
}

int main(int argc, char* argv[]) {
	if(argc != 7) {
		printf("Usage: ./sched-1.out i j k in1.txt in2.txt out.txt\n");
		exit(-1);
	}

	mi = atoi(argv[1]);
	mj = atoi(argv[2]);
	mk = atoi(argv[3]);
	in1 = argv[4];
	in2 = argv[5];
	out = argv[6];

	/* Named Semaphores Initialization */
	sem_a1 = sem_open(A1NAME, O_CREAT | O_EXCL, 0666, 0);
    sem_a2 = sem_open(A2NAME, O_CREAT | O_EXCL, 0666, 1);
	sem_b1 = sem_open(B1NAME, O_CREAT | O_EXCL, 0666, 1);
	sem_b2 = sem_open(B2NAME, O_CREAT | O_EXCL, 0666, 0);
	if(sem_a1 == SEM_FAILED) {
		perror("[S] sem_a1 was already existing. removiing it.");
		if(sem_unlink(A1NAME) == -1) {
			perror("[S] Error removing the existing sem_a1");
			exit(-1);
		}
		sem_a1 = sem_open(A1NAME, O_CREAT | O_EXCL, 0666, 0);
		if(sem_a1 == SEM_FAILED) {
			perror("[S] ERROR! sem_open(A1NAME) failed");
			exit(-1);
		}
	}
	if(sem_a2 == SEM_FAILED) {
		perror("[S] sem_a2 was already existing. removiing it.");
		if(sem_unlink(A2NAME) == -1) {
			perror("[S] Error removing the existing sem_a2");
			exit(-1);
		}
		sem_a2 = sem_open(A2NAME, O_CREAT | O_EXCL, 0666, 1);
		if(sem_a2 == SEM_FAILED) {
			perror("[S] ERROR! sem_open(A2NAME) failed");
			exit(-1);
		}
	}
	if(sem_b1 == SEM_FAILED) {
		perror("[S] sem_b1 was already existing. removiing it.");
		if(sem_unlink(B1NAME) == -1) {
			perror("[S] Error removing the existing sem_b1");
			exit(-1);
		}
		sem_b1 = sem_open(B1NAME, O_CREAT | O_EXCL, 0666, 1);
		if(sem_b1 == SEM_FAILED) {
			perror("[S] ERROR! sem_open(B1NAME) failed");
			exit(-1);
		}
	}
	if(sem_b2 == SEM_FAILED) {
		perror("[S] sem_b2 was already existing. removiing it.");
		if(sem_unlink(B2NAME) == -1) {
			perror("[S] Error removing the existing sem_b2");
			exit(-1);
		}
		sem_b2 = sem_open(B2NAME, O_CREAT | O_EXCL, 0666, 0);
		if(sem_b2 == SEM_FAILED) {
			perror("[S] ERROR! sem_open(B2NAME) failed");
			exit(-1);
		}
	}

	/* Shared Memory Initialization */
	key_t key = ftok(argv[4], 43);
	if(key == -1) {
		perror("ERROR! ftok()");
		exit(-1);
	}
	int shmid = shmget(key, (2+(mi+mk)*mj+(mi+mk)+3)*sizeof(int), 0666 | IPC_CREAT | IPC_EXCL);
	if(shmid == -1) {
		shmid = shmget(key, 1, 0);
		if(shmctl(shmid, IPC_RMID, NULL) == -1) {
			perror("ERROR removing the existing shared memory segment. shmctl()");
		}
		shmid = shmget(key, (2+(mi+mk)*mj+(mi+mk)+3)*sizeof(int), 0666 | IPC_CREAT | IPC_EXCL);
		if(shmid == -1) {
			perror("ERROR! shmget()");
			if(errno == ENOENT) {
				perror("ENOENT in shmget()");
			} else if(errno == EINVAL) {
				perror("EINVAL in shmget()");
			}
			exit(-2);
		}
	}
	shmPtr = shmat(shmid, NULL, 0);
	if(shmPtr == (void*) -1) {
		perror("ERROR! shmat()");
		exit(-2);
	}
	*shmPtr = 0;
	shmPtr[1] = 1;
	fposn1 = shmPtr+(2+(mi+mk)*mj);
	fposn2 = shmPtr+(2+(mi+mk)*mj+mi);
	switchingStatus = shmPtr+(2+(mi+mk)*mj+(mi+mk));
	switchingStatus[0] = 1;
	semADirxn = shmPtr+(2+(mi+mk)*mj+(mi+mk)+1);
	semBDirxn = shmPtr+(2+(mi+mk)*mj+(mi+mk)+2);
	*semADirxn = 1;
	*semBDirxn = 0;



	/* Input Preprocessing */
	input_preprocess();


	/* Create two child processes and do round-robin */
	pid_t p1, p2;
	if((p1 = fork()) == 0) {	//P1
		char read_n_str[2];
		sprintf(read_n_str, "%d", read_n);
		if(execlp("./p1.out", "rex-read", argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], read_n_str, NULL) == -1) {
			perror("ERROR! pread execlp()");
		}
		exit(1);
	} else {	//P2
		if((p2 = fork()) == 0) {
			char calc_n_str[2];
			sprintf(calc_n_str, "%d", calc_n);
			if(execlp("./p2.out", "rex-write", argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], calc_n_str, NULL) == -1) {
				perror("ERROR! pwrite execlp()");
			}
			exit(2);
		}
	}
	

	struct timespec timing[200];
	if(p1 != 0 && p2 != 0) {	//S

		// Time Quantum
		struct timespec ts;
		ts.tv_sec = 0;
		ts.tv_nsec = 1000000;
		int switchCount = 0;

		while(shmPtr[1]) {
			switchCount++;
			nanosleep(&ts, NULL);
			if(switchCount&1)	*semBDirxn = !*semBDirxn;
			else	*semADirxn = !*semADirxn;
			*shmPtr = !*shmPtr;
			while(switchingStatus[0]);
			switchingStatus[0] = 1;
		}
		*shmPtr = 1;
		sem_post(sem_b1);
		sem_post(sem_b2);

		if(wait(NULL) == -1) {
			perror("[S] Failed Waiting 1");
		}
		if(wait(NULL) == -1) {
			perror("[S] Failed Waiting 2");
		}
	}



	if(shmctl(shmid, IPC_RMID, NULL) == -1) {
		perror("[S] ERROR removing shared memory segment. shmctl()");
	}
	if(sem_unlink(A1NAME) == -1) {
		perror("[S] ERROR removing sem_a1. sem_unlink()");
	}
	if(sem_unlink(A2NAME) == -1) {
		perror("[S] ERROR removing sem_a2. sem_unlink()");
	}
	if(sem_unlink(B1NAME) == -1) {
		perror("[S] ERROR removing sem_b1. sem_unlink()");
	}
	if(sem_unlink(B2NAME) == -1) {
		perror("[S] ERROR removing sem_b2. sem_unlink()");
	}
	return 0;
}
