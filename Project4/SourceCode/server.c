/* csci4061 S2016 Assignment 4 
 * Name: Caleb Biasco, Dennis Ehrhardt, Meghan Jonas
 * X500: biasc007, ehrh0023, jonas050 */
*/

#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "util.h"

#define MAX_THREADS 100
#define MAX_QUEUE_SIZE 100
#define MAX_REQUEST_LENGTH 64

//Structure for queue.
typedef struct request_queue
{
	int             m_socket;
	char    m_szRequest[MAX_REQUEST_LENGTH];
} request_queue_t;

// Global Variables
int port;
int cache_size;

// Buffer Information
request_queue_t queue[MAX_REQUEST_LENGTH];
int queue_length;
int queue_index = 0;

void * dispatch(void * arg)
{
	return NULL;
}

void * worker(void * arg)
{
	return NULL;
}

int main(int argc, char **argv)
{
	int num_dispatcher = 0;
	int num_workers = 0;
	int i;
	pthread_t dispatchers[MAX_THREADS];
	pthread_t workers[MAX_THREADS];
	//Error check first.
	if(argc != 6 && argc != 7)
	{
			printf("usage: %s port path num_dispatcher num_workers queue_length [cache_size]\n", argv[0]);
			return -1;
	}

	printf("Call init() first and make a dispather and worker threads\n");
	port = atoi(argv[1]);
	init(port);
	
	num_dispatcher = atoi(argv[3]);
	num_workers = atoi(argv[4]);
	
	queue_length = atoi(argv[5]);
	cache_size = atoi(argv[6]);
	
	// Create Dispatchers
	for(i = 0; i < num_dispatcher; i++)
	{
		if (pthread_create(&dispatchers[i],NULL,dispatch,NULL))
		  perror("couldn't create thread\n");		
	}
	// Create Workers
	for(i = 0; i < num_workers; i++)
	{
		if (pthread_create(&workers[i],NULL,worker,NULL))
		  perror("couldn't create thread\n");		
	}
	
	// Join Dispatchers
	for(i = 0; i < num_dispatcher; i++)
	{
		if (pthread_join(&dispatchers[i],NULL) != 0)
			perror("couldn't join thread\n");
	}
	// Join Workers
	for(i = 0; i < num_workers; i++)
	{
		if (pthread_join(&workers[i],NULL) != 0)
			perror("couldn't join thread\n"); 
	}
	
	return 0;
}
