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
	/***** Insert YOUR code *******/
 	/* Check for \seg command and create segfault */
	if(starts_with(line, "\\seg"))
	{
		*n = 1;
		
	}
	/* Write message to server for processing */
	else
	{
		write(fd_toserver, line, strlen(line));
	}
	return 0;
}

/*
 * Check if the line is empty (no data; just spaces or return)
 */
int is_empty(char *line)
{
	while (*line != '\0') {
		if (!isspace(*line))
			return 0;
		line++;
	}
	return 1;
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
		fgets(msg, MSG_SIZE, stdin);
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
				printf("%s", msg);
			usleep(1000);
		}
	}
	/* Inside the parent
	 * Send the child's pid to the server for later cleanup
	 * Start the main shell loop
	 */
	 else
	 {
		sprintf(msg, "%d", pid);
		//write(ends[1], msg, MSG_SIZE); THIS NEEDS TO BE DONE WITH THE CHILD_PID COMMAND
		sh_start(name, ends[1]);
	 }
	 return EXIT_SUCCESS;
}
