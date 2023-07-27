#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>

typedef struct
{
	int x, y;
} pair;

typedef struct
{
	int x, y, degree;
} argument;

int N;
int x_modifier[8] = {-2, -1, 1, 2, 2, 1, -1, -2};
int y_modifier[8] = {1, 2, 2, 1, -1, -2, -2, -1};
int degree_of_rotation[4] = {0, 90, 180, 270};
int total_squares;
int solved_status = 0, print_status = 0;
int whoSolved = -1;
int throughput[4] = {0};
int thread_count = 0;
int debugCheck = 0;

sem_t turnstile;			  // Using turnstile synchronization design pattern to create a thread barrier. This ensures at least every thread is doing some work.
pthread_mutex_t barrier;	  // We could have used binary semaphore to implement this. But mutex_locks have the an additional advantage that only the thread which locked it can unlock it back.
pthread_mutex_t solutionLock; // This mutex lock is for making sure that if multiple threads find the solution at the same time, then only one thread sets the solved_status as 1 and outputs the path.

int validate(int x, int y, int visited[N][N])
{
	return (x >= 0 && x < N && y >= 0 && y < N && visited[x][y] == -1);
}

int find_number(int x, int y, int visited[N][N])
{
	int neighbour_count = 0;
	for (int i = 0; i < 8; i++)
	{
		neighbour_count += validate(x + x_modifier[i], y + y_modifier[i], visited);
	}
	return neighbour_count;
}

pair rotate_anticlockwise(int x, int y, int degree)
{
	pair rotated_coords;

	if (degree == 0 || degree == 360)
	{
		rotated_coords.x = x;
		rotated_coords.y = y;
	}
	else if (degree == 90)
	{
		rotated_coords.x = N - y - 1;
		rotated_coords.y = x;
	}
	else if (degree == 180)
	{
		rotated_coords.x = N - x - 1;
		rotated_coords.y = N - y - 1;
	}
	else
	{
		rotated_coords.x = y;
		rotated_coords.y = N - x - 1;
	}

	return rotated_coords;
}

void print_rotated_path(pair path[], int n, int degree)
{
	pair rotated_coords;
	for (int i = 0; i < n; i++)
	{
		rotated_coords = rotate_anticlockwise(path[i].x, path[i].y, 360 - degree);
		printf("%d,%d|", rotated_coords.x, rotated_coords.y);
	}
}

int Solve(int x, int y, int count, int degree, int visited[N][N], pair path[])
{
	if (solved_status == 1)
		return 1;

	if (N >= 15 && throughput[degree / 90] == total_squares / 2)
	{
		pthread_mutex_lock(&barrier);
		thread_count++;
		if (thread_count == 4)
		{
			sem_post(&turnstile);
			if (debugCheck)
				printf("Last reached thread #%d\n", degree / 90);
		}
		pthread_mutex_unlock(&barrier);

		if (debugCheck && thread_count != 4)
			printf("Thread #%d waiting\n", degree / 90);

		sem_wait(&turnstile);
		sem_post(&turnstile);
	}

	int mi = 8, length = 0;
	int *xcoordinate = (int *)malloc(sizeof(int));
	int *ycoordinate = (int *)malloc(sizeof(int));

	visited[x][y] = 0;
	throughput[(degree / 90)]++;
	count++;

	if (count == total_squares)
	{
		pthread_mutex_lock(&solutionLock);
		if (solved_status != 1)
		{
			solved_status = 1;
			print_status = 1;
			print_rotated_path(path, total_squares, degree);
			whoSolved = degree / 90;
		}
		pthread_mutex_unlock(&solutionLock);
		return 1;
	}

	for (int k = 0; k < 8; k++)
	{
		int new_x = x + x_modifier[k];
		int new_y = y + y_modifier[k];
		if (validate(new_x, new_y, visited))
		{
			int a = find_number(new_x, new_y, visited);
			if (a == 0 && count != total_squares - 1)
				continue;
			if (a < mi)
			{
				mi = a;
				length = 0;
				free(xcoordinate);
				free(ycoordinate);
				xcoordinate = (int *)malloc(sizeof(int));
				ycoordinate = (int *)malloc(sizeof(int));
				xcoordinate[length] = new_x;
				ycoordinate[length] = new_y;
				length++;
			}
			else if (a == mi)
			{
				xcoordinate = (int *)realloc(xcoordinate, (length + 1) * sizeof(int));
				ycoordinate = (int *)realloc(ycoordinate, (length + 1) * sizeof(int));
				xcoordinate[length] = new_x;
				ycoordinate[length] = new_y;
				length++;
			}
		}
	}

	if (length == 0)
	{
		visited[x][y] = -1;
		return 0;
	}

	for (int k = 0; k < length; k++)
	{
		path[count].x = xcoordinate[k];
		path[count].y = ycoordinate[k];
		if (Solve(xcoordinate[k], ycoordinate[k], count, degree, visited, path) == 1)
		{
			free(xcoordinate);
			free(ycoordinate);
			return 1;
		}
	}

	free(xcoordinate);
	free(ycoordinate);

	visited[x][y] = -1;
	return 0;
}

void *begin_tour(void *args)
{
	int visited[N][N];
	for (int i = 0; i < N; i++)
	{
		for (int j = 0; j < N; j++)
		{
			visited[i][j] = -1;
		}
	}

	int temp_degree = ((argument *)args)->degree;
	pair rotated_coords = rotate_anticlockwise(((argument *)args)->x, ((argument *)args)->y, temp_degree);

	pair path[total_squares];
	path[0].x = rotated_coords.x;
	path[0].y = rotated_coords.y;

	int *ret_value = (int *)malloc(sizeof(int));

	if (Solve(rotated_coords.x, rotated_coords.y, 0, temp_degree, visited, path) == 0)
		ret_value[0] = 0;
	else
		ret_value[0] = 1;
	return (void *)ret_value;
}

void *snorlax()
{
	struct timespec ts;
	ts.tv_sec = 0;
	ts.tv_nsec = 100000000;

	nanosleep(&ts, NULL);
	solved_status = 1;
}

int main(int argc, char *argv[])
{

	if (argc != 4)
	{
		printf("Usage: ./Knight.out grid_size StartX StartY");
		exit(-1);
	}

	N = atoi(argv[1]);
	int StartX = atoi(argv[2]);
	int StartY = atoi(argv[3]);

	total_squares = N * N;
	int i;

	argument args[4];
	pthread_t tid[4];
	pthread_t tkiller;
	sem_init(&turnstile, 0, 0);
	pthread_mutex_init(&barrier, NULL);
	pthread_mutex_init(&solutionLock, NULL);

	pthread_create(&tkiller, NULL, snorlax, NULL);

	for (i = 0; i < 4; i++)
	{
		args[i].x = StartX;
		args[i].y = StartY;
		args[i].degree = degree_of_rotation[i];
		pthread_create(&tid[i], NULL, begin_tour, (void *)&args[i]);
	}

	int flag = 0;

	for (i = 0; i < 4; i++)
	{
		void *t;
		pthread_join(tid[i], &t);
		flag += *((int *)t);
		free(t);
	}

	if (flag == 0 || print_status == 0)
		printf("No Possible Tour");
	else if (debugCheck)
		printf("\nSolved by thread #%d. Throughput: (%d, %d, %d, %d).\n", whoSolved, throughput[0], throughput[1], throughput[2], throughput[3]);

	return 0;
}
