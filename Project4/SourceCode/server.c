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

// Request logging
pthread_mutex_t log_access = PTHREAD_MUTEX_INITIALIZER;
int worker_id = 0;

// Buffer Information
request_queue_t queue[MAX_QUEUE_SIZE];
int queue_length;
int queue_in = 0;
int queue_out = 0;
int count = 0;
pthread_cond_t queue_open = PTHREAD_COND_INITIALIZER;
pthread_cond_t queue_content = PTHREAD_COND_INITIALIZER;
pthread_mutex_t lock_access = PTHREAD_MUTEX_INITIALIZER;

//Dispatcher Info *No longer used*
//int num_dispatchers = 0;

/*
 * Finds the last period in a string and returns the portion afterward
 */
const char *get_filename_ext(const char *filename) {
	const char *dot_index = strrchr(filename, '.');
	if (!dot_index || dot_index == filename)
		return "";
	return dot_index + 1;
}

/*
 * Waits for a connection to come. Grabs the lock once it gets one, receives the request and sticks it into
 * the request queue, the unlocks and loops around.
 */
void * dispatch(void * arg)
{
	char request[MAX_REQUEST_LENGTH];
	int socket;
	
	while (1) {
		socket = accept_connection();
		if (socket < 0) {
			continue; // according to TA Nishad Trivedi, do nothing here
		}
		
		// access the bounded buffer and place a request
		pthread_mutex_lock(&lock_access);
		if (get_request(socket, request) != 0) {
			pthread_mutex_unlock(&lock_access);
			continue; // according to TA Nishad Trivedi, do nothing here
		}
		while (count >= queue_length) {
			pthread_cond_wait(&queue_open, &lock_access);
		}
		
		strcpy(queue[queue_in].m_szRequest, request);
		queue[queue_in].m_socket = socket;
		queue_in = (queue_in + 1) % queue_length;
		count++;
		pthread_cond_signal(&queue_content);
		pthread_mutex_unlock(&lock_access);
	}
	
	return NULL;
}

/*
 * Waits until the request queue has content. Once it does, it grabs the lock, gets the request, unlocks, then
 * processes the request by opening the file, reading it into a buffer, and returning it using return_result().
 * Writes to a log file for each request.
 */
void * worker(void * arg)
{
	// Initialize request variables
	char request[MAX_REQUEST_LENGTH];
	int socket;
	
	// Initialize processing variables
	char *buf;
	struct stat sb;
	int fd;
	char content_type[12];
	char ext[5];

	// Initialize logging variables
	int requests_handled;
	pthread_mutex_lock(&log_access);
	int wid= worker_id;
	worker_id++;
	pthread_mutex_unlock(&log_access);
	FILE * request_log;
	char tempchar[MAX_REQUEST_LENGTH*2];
	char lbuf[MAX_REQUEST_LENGTH*2];

	while (1) {
		// prepare log entry
		requests_handled++;
		sprintf(lbuf, "[%d][%d]", wid, requests_handled);
		
		// access the bounded buffer and grab a request
		pthread_mutex_lock(&lock_access);
		while (count == 0) {
			pthread_cond_wait(&queue_content, &lock_access);
		}
		sprintf(request, queue[queue_out].m_szRequest);
		socket = queue[queue_out].m_socket;
		queue_out = (queue_out + 1) % queue_length;
		count--;
		pthread_cond_signal(&queue_open);
		pthread_mutex_unlock(&lock_access);
		
		// write to log
		sprintf(tempchar, "[%d][%s]", socket, request);
		strcat(lbuf, tempchar);
		
		if (strcmp(request, "/favicon.ico") == 0) {
			requests_handled--;
			continue; // skips favicon.ico request from Google Chrome browser
		}
		
		// stat() into the requested file for its size
		if (stat(request+1, &sb) < 0) {
			buf = (char *) malloc(1060);
			sprintf(buf, "BAD REQUEST: Could not find \"%s\"", request);
			return_error(socket, buf);
			free(buf);

			// write to log
			strcat(lbuf, "[Bad request.]\n");
			pthread_mutex_lock(&log_access);
			request_log = fopen("web_server_log.txt", "a");
			if (request_log == NULL) printf("Error opening log\n");
			fprintf(request_log, lbuf);
			fclose(request_log);
			pthread_mutex_unlock(&log_access);
			continue;
		}
		
		// if exists, open requested file for reading
		fd = open(request+1, 0);
		if (fd < 0) {
			buf = (char *) malloc(1060);
			sprintf(buf, "BAD REQUEST: Could not open \"%s\"", request);
			return_error(socket, buf);
			free(buf);

			// write to log
			strcat(lbuf, "[Bad request.]\n");
			pthread_mutex_lock(&log_access);
			request_log = fopen("web_server_log.txt", "a");
			if (request_log == NULL) printf("Error opening log\n");
			fprintf(request_log, lbuf);
			fclose(request_log);
			pthread_mutex_unlock(&log_access);
			continue;
		}
		
		// read requested file into a buffer
		buf = (char *) malloc(sb.st_size);
		if (read(fd, buf, sb.st_size) < 0) {
			perror("read failure");
			return_error(socket, buf);
			close(fd);
			free(buf);

			// write to log
			strcat(lbuf, "[Read failure.]\n");
			pthread_mutex_lock(&log_access);
			request_log = fopen("web_server_log.txt", "a");
			if (request_log == NULL) printf("Error opening log\n");
			fprintf(request_log, lbuf);
			fclose(request_log);
			pthread_mutex_unlock(&log_access);
			continue;
		}
		
		// get the file type by checking the requested file's extension
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
		
		// return the result
		if (return_result(socket, content_type, buf, sb.st_size) != 0) {
			perror("return_result failure");
			return_error(socket, buf);
			close(fd);
			free(buf);

			// write to log
			strcat(lbuf, "[Failure returning result.]\n");
			pthread_mutex_lock(&log_access);
			request_log = fopen("web_server_log.txt", "a");
			if (request_log == NULL) printf("Error opening log\n");
			fprintf(request_log, lbuf);
			fclose(request_log);
			pthread_mutex_unlock(&log_access);
			continue;
		}
		
		// clean up and write to log
		sprintf(tempchar, "[%d]\n", sb.st_size);
		strcat(lbuf, tempchar);
		pthread_mutex_lock(&log_access);
		request_log = fopen("web_server_log.txt", "ab");
		if (request_log == NULL) printf("Error opening log\n");
		fprintf(request_log, lbuf);
		fclose(request_log);
		pthread_mutex_unlock(&log_access);
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
	FILE *request_log;

	//Error check and initialize all globals
	if(argc != 6 && argc != 7)
	{
		printf("usage: %s port path num_dispatcher num_workers queue_length [cache_size]\n", argv[0]);
		return -1;
	}

	port = atoi(argv[1]);
	init(port);
	if (chdir(argv[2]) == -1) {
		printf("Improper directory\n");
		return -1;
	}
	// Delete old log.
	remove("web_server_log.txt");
	
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
