/* CSci4061 S2016 Assignment 2
 * section: 7
 * section:
 * section: 
 * date: 03/11/16
 * name: Caleb Biasco, Meghan Jonas, Dennis Ehrhardt
 * id: biasc007, jonas050, ehrh0023  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "util.h"

/*
 * Read a line from stdin.
 */
char *sh_read_line(void)
{
	char *line = NULL;
	ssize_t bufsize = 0;

	getline(&line, &bufsize, stdin);
	return line;
}

/*
 * Do stuff with the shell input here.
 */
int sh_handle_input(char *line, int fd_toserver)
{
	char* n = NULL;
 	/* Check for \seg command and create segfault */
	if(starts_with(line, CMD_SEG))
	{
		printf("Generating segfault...");
		*n = 1;
	}
	/* Write message to server for processing */
	else
	{
		write(fd_toserver, line, MSG_SIZE);
	}
	return 0;
}

/*
 * Start the main shell loop:
 * Print prompt, read user input, handle it.
 */
void sh_start(char *name, int fd_toserver)
{
	char msg[MSG_SIZE];
	int i;
	/***** Insert YOUR code *******/
	while(1)
	{
		printf("%s >> ", name);
		strcpy(msg, sh_read_line());
		if(!is_empty(msg))
		{	
			sh_handle_input(msg, fd_toserver);
		}
	}
}

int main(int argc, char **argv)
{
	int pid;
	int ends[2];
	char msg[MSG_SIZE];
	char* name;
	
	/* Extract pipe descriptors and name from argv */
	if(argc <= 3)
	{
		return EXIT_FAILURE;
	}
	ends[0] = atoi(argv[1]);
	ends[1] = atoi(argv[2]);
	name = argv[3];
	
	/* Fork a child to read from the pipe continuously */
	pid = fork();
	if(pid == -1)
	{
		return EXIT_FAILURE;
	}
	/*
	 * Once inside the child
	 * look for new data from server every 1000 usecs and print it
	 */ 
	if(pid == 0)
	{
		while(1)
		{
			if(read(ends[0], msg, MSG_SIZE) > 0)
			{
				printf("%s\n", msg);
			}
			usleep(1000);
		}
	}
	/* Inside the parent
	 * Send the child's pid to the server for later cleanup
	 * Start the main shell loop
	 */
	 else
	 {
		sprintf(msg, "%s %d\n", CMD_CHILD_PID, pid);
		write(ends[1], msg, MSG_SIZE);
		sh_start(name, ends[1]);
	 }
	 return EXIT_SUCCESS;
}
