/* csci4061 S2016 Assignment 4 
 * Name: Caleb Biasco, Dennis Ehrhardt, Meghan Jonas
 * X500: biasc007, ehrh0023, jonas050 */


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
#define MAX_REQUEST_LENGTH 1024

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
request_queue_t queue[MAX_QUEUE_SIZE];
int queue_length;
int queue_in = 0;
int queue_out = 0;
int count = 0;
pthread_cond_t queue_open = PTHREAD_COND_INITIALIZER;
pthread_cond_t queue_content = PTHREAD_COND_INITIALIZER;
pthread_mutex_t lock_access = PTHREAD_MUTEX_INITIALIZER;

//Dispacther Info *No longer used*
//int num_dispatchers = 0;

const char *get_filename_ext(const char *filename) {
	const char *dot_index = strrchr(filename, '.');
	if (!dot_index || dot_index == filename)
		return "";
	return dot_index + 1;
}

void * dispatch(void * arg)
{
	int fd;
	char filename[MAX_REQUEST_LENGTH];
	
	while (1) {
		fd = accept_connection();
		if (fd < 0) {
			continue; // according to TA Nishad Trivedi, do nothing here
		}
		
		pthread_mutex_lock(&lock_access);
		if (get_request(fd, filename) != 0) {
			pthread_mutex_unlock(&lock_access);
			continue; // according to TA Nishad Trivedi, do nothing here
		}
		while (count >= queue_length) {
			pthread_cond_wait(&queue_open, &lock_access);
		}
		
		queue[queue_in].m_socket = fd;
		strcpy(queue[queue_in].m_szRequest, filename);
		queue_in = (queue_in + 1) % queue_length;
		count++;
		pthread_cond_signal(&queue_content);
		pthread_mutex_unlock(&lock_access);
	}
	
	return NULL;
}

void * worker(void * arg)
{
	char *buf;
	struct stat sb;
	int fd;
	char request[MAX_REQUEST_LENGTH];
	int socket;
	char content_type[12];
	char ext[5];
	
	while (1) {
		pthread_mutex_lock(&lock_access);
		if (count == 0) {
			pthread_cond_wait(&queue_content, &lock_access);
		}
		strcpy(request, queue[queue_out].m_szRequest);
		socket = queue[queue_out].m_socket;
		queue_out = (queue_out + 1) % queue_length;
		count--;
		pthread_cond_signal(&queue_open);
		pthread_mutex_unlock(&lock_access);
		
		if (strcmp(request, "/favicon.ico") == 0) {
			continue; // skips favicon.ico request from Google Chrome browser
		}
		
		if (stat(request+1, &sb) < 0) {
			buf = (char *) malloc(1060);
			sprintf(buf, "BAD REQUEST: Could not find \"%s\"", request);
			return_error(socket, buf);
			free(buf);
			continue;
		}
		fd = open(request+1, 0);
		
		buf = (char *) malloc(sb.st_size);
		if (read(fd, buf, sb.st_size) < 0) {
			perror("read failure");
			close(fd);
			return_error(socket, buf);
			free(buf);
			continue;
		}
		
		strcpy(ext, get_filename_ext(request));
		if (strcmp(ext, "html") == 0 || strcmp(ext, "htm") == 0) {
			strcpy(content_type, "text/html");
		}
		else if (strcmp(ext, "jpg") == 0 || strcmp(ext, "jpeg") == 0) {
			strcpy(content_type, "image/jpeg");
		}
		else if (strcmp(ext, "gif") == 0) {
			strcpy(content_type, "image/gif");
		}
		else {
			strcpy(content_type, "text/plain");
		}
		
		if (return_result(socket, content_type, buf, sb.st_size) != 0) {
			perror("return_result failure");
			close(fd);
			return_error(socket, buf);
			free(buf);
			continue;
		}
		close(fd);
		free(buf);
	}
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

	port = atoi(argv[1]);
	init(port);
	chdir(argv[2]);
	
	if (atoi(argv[3]) > MAX_THREADS) {
		printf("Max # of dispatcher threads is 100\n");
		return -1;
	}
	num_dispatcher = atoi(argv[3]);
	if (atoi(argv[4]) > MAX_THREADS) {
		printf("Max # of worker threads is 100\n");
		return -1;
	}
	num_workers = atoi(argv[4]);
	
	if (atoi(argv[5]) > MAX_QUEUE_SIZE) {
		printf("Max queue size is 100\n");
		return -1;
	}
	queue_length = atoi(argv[5]);
	if (argv[6] != 0)
		cache_size = atoi(argv[6]);
	else
		cache_size = 0;
	
	printf("Creating dispatcher and worker threads...\n");
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
	
	printf("Threads created!\n");
	
	// Join Dispatchers
	for(i = 0; i < num_dispatcher; i++)
	{
		if (pthread_join(dispatchers[i],NULL) != 0)
			perror("couldn't join thread\n");
	}
	// Join Workers
	for(i = 0; i < num_workers; i++)
	{
		if (pthread_join(workers[i],NULL) != 1)
			perror("couldn't join thread\n"); 
	}
	
	return 0;
}
