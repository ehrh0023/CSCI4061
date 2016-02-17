#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h> 
#include <unistd.h>

#include "util.h"

#define DEBUG 0

target_t targetList[MAX_NODES]; // Array of target structs
int nTargetCount = 0; // Counter for placing targets in array
int boolCheckModTimes = 1; // Boolean for -B command

// This function will parse makefile input from user or default makeFile. 
int parse(char * lpszFileName)
{
	int i, j, k;
	int nLine = 0;
	char szLine[1024];
	char * lpszLine;

	FILE * fp = file_open(lpszFileName);
	
	int nDependencyCount = 0; // Counter for placing dependencies in array inside target
	
	int boolHasCommand = 0; // Boolean for checking if we already have a command

	if(fp == NULL)
	{
		return -1;
	}

	while(file_getline(szLine, fp) != NULL) 
	{
		nLine++;

		// Remove newline character at end if there is one
		lpszLine = strtok(szLine, "\n");

		// Checks if "blank line"
		if (lpszLine != NULL)
		{
			// If there is no leading tab
			if (strncmp(lpszLine, "\t", 1) != 0)
			{
				// If the line is not a comment
				if (strncmp(lpszLine, "#", 1) != 0)
				{
					// Check for any illegal characters in line
					int boolHasColon = 0;
					while (strlen(lpszLine) > 0)
					{
						if (isalnum(*lpszLine) || isblank(*lpszLine) || *lpszLine == '.')
						{
							lpszLine++;
						}
						else if (*lpszLine == ':' && !boolHasColon)
						{
							boolHasColon = 1;
							lpszLine++;
						}
						else
						{
							printf("ERROR: Illegal character on Line %d\n", nLine);
							return -1;
						}
					}
					
					lpszLine = szLine;
					// Skip all leading spaces
					while (*lpszLine == ' ') {lpszLine++;}
					// If there is no colon in line
					if (strchr(lpszLine, ':') == NULL)
					{
						printf("ERROR: No target on Line %d\n", nLine);
						return -1;
					}
					
					nTargetCount++;
					nDependencyCount = 0;
					boolHasCommand = 0;

					lpszLine = strtok(lpszLine, ":");
					// If there is a space in the target name
					if (strchr(lpszLine, ' '))
					{
						printf("ERROR: Illegal target name on Line %d\n", nLine);
						return -1;
					}
					// Check for duplicate target names
					for (i = 0; i < nTargetCount-1; i++)
					{
						if (strcmp(lpszLine, targetList[i].szTarget) == 0)
						{
							printf("ERROR: Duplicate target on Line %d\n", nLine);
							return -1;
						}
					}
					strcpy(targetList[nTargetCount-1].szTarget, lpszLine);
					
					lpszLine = strtok(NULL, " \t");
					while (lpszLine != NULL)
					{
						strcpy(targetList[nTargetCount-1].szDependencies[nDependencyCount], lpszLine);
						nDependencyCount++;
						lpszLine = strtok(NULL, " \t");
					}
					targetList[nTargetCount-1].nDependencyCount = nDependencyCount;
					nDependencyCount = 0;
				}
			}
			// If there is a leading tab
			else
			{
				// Check for any illegal characters in line
				while (strlen(lpszLine) > 0) 
				{
					if (isalnum(*lpszLine) || isblank(*lpszLine) || strchr("-.'\"", *lpszLine))
					{
						lpszLine++;
					}
					else
					{
						printf("ERROR: Illegal character on Line %d\n", nLine);
						return -1;
					}
				}

				lpszLine = szLine;
				// Skip all leading whitespace
				while (isspace(*lpszLine)) {lpszLine++;}
				// If the line is not empty or a comment
				if (strlen(lpszLine) > 0 && strncmp(lpszLine, "#", 1) != 0)
				{
					// If we already have a command
					if (boolHasCommand) 
					{
						printf("ERROR: Second command on Line %d\n", nLine);
						return -1;
					}
					// If we don't have a command
					else
					{
						strcpy(targetList[nTargetCount-1].szCommand, lpszLine);
						boolHasCommand = 1;
					}
				}
			}
		}
	}
	
	// DEBUG CODE: Prints the target information to terminal
	if (DEBUG)
	{
		i = 0;
		while (i < nTargetCount)
		{
			printf("%d: %s\n", i+1, targetList[i].szTarget);
			nDependencyCount = targetList[i].nDependencyCount;
			j = 0;
			while (j < nDependencyCount) {
				printf("\tDependency %d: %s\n", j+1, targetList[i].szDependencies[j]);
				j++;
			}
			printf("\tCommand: %s\n", targetList[i].szCommand);
			i++;
		}
	}

	// Close the makefile. 
	fclose(fp);

	// Build the graph
	i = 0;
	while (i < nTargetCount)
	{
		j = 0;
		while (j < targetList[i].nDependencyCount)
		{
			k = 0;
			targetList[i].child[j] = NULL;
			while (k < nTargetCount)
			{
				if (strcmp(targetList[i].szDependencies[j], targetList[k].szTarget) == 0)
				{
					targetList[i].child[j] = &targetList[k];
					k = nTargetCount;
				}
				k++;
			}
			j++;
		}
		i++;
	}
	
	return 0;
}


void process(target_t** target, int size)
{
	int i, j;
	int stat_loc;
	int success = 0;
	target_t* n_lvl[10];
	int n_lvl_size = 0;
	char** cmd;
	
	// Create Next Level
	for (j = 0; j < size; j++)
	{

		// Add Children
		for (i = 0; i < target[j]->nDependencyCount; i++)
		{
			if(target[j]->child[i] != NULL)
			{
				n_lvl[n_lvl_size] = target[j]->child[i];
				n_lvl_size++;
				success = 1;
			}
		}
	}
	
	cmd = (char**) malloc(sizeof(char*) * 12);
	// Only if the next level isn't empty
	if(success)
	{
		// Process the next level
		process(n_lvl, n_lvl_size);
	}
	// Check if dependencies have been modified
	if (boolCheckModTimes)
	{
		for (j = 0; j < size; j++)
		{
			if (target[j]->nDependencyCount) { target[j]->boolModified = 0; }
			else { target[j]->boolModified = 1; }
			for (i = 0; i < target[j]->nDependencyCount; i++)
			{
				if (!target[j]->boolModified && (compare_modification_time(target[j]->szCommand, target[j]->szDependencies[i]) == 2 || (target[j]->child[i] != NULL && target[j]->child[i]->boolModified)))
				{
					target[j]->boolModified = 1;
				}
			}
		}
		// Execute targets
		for (j = 0; j < size; j++)
		{
			// If dependencies have been modified
			if (target[j]->boolModified && target[j]->nStatus == 0)
			{
				// Building tag
				target[j]->nStatus = 1;
				
				// Fork and execute
				target[j]->pid = fork();
				if (target[j]->pid == 0)
				{
					makeargv(target[j]->szCommand, " ", &cmd);
					execvp(cmd[0], cmd);
				}
			}
		}
		// Wait until all forks end
		for (j = 0; j < size; j++)
		{
			// If the pid is 0, don't wait
			if (!target[j]->pid) { waitpid(target[j]->pid, &stat_loc, 0); }
		}
	}
	else
	{
		// Excute targets
		for (j = 0; j < size; j++)
		{
			if (target[j]->nStatus == 0)
			{
				// Building tag
				target[j]->nStatus = 1;
				
				// Fork and execute
				target[j]->pid = fork();
				if (target[j]->pid == 0)
				{
					makeargv(target[j]->szCommand, " ", &cmd);
					execvp(cmd[0], cmd);
				}
			}
		}
		// Wait until all forks end
		for (j = 0; j < size; j++)
		{
			waitpid(target[j]->pid, &stat_loc, 0);	
		}
	}
	free(cmd);
}


void beginprocessing(char* t_name)
{
	int i;
	target_t* target = NULL;
	char** cmd = (char**) malloc(sizeof(char*) * 12);

	// Find the starting string
	// Prep nStatus and pid
	for (i = 0; i < nTargetCount; i++)
	{
		targetList[i].nStatus = 0;
		targetList[i].pid = -1;
		if (strcmp(t_name, targetList[i].szTarget) == 0)
		{
			target = &targetList[i];
		}

	}
	// Run Process
	if (target != NULL)
	{
		process(&target, 1);
	}
}

void show_error_message(char * lpszFileName)
{
	fprintf(stderr, "Usage: %s [options] [target] : only single target is allowed.\n", lpszFileName);
	fprintf(stderr, "-f FILE\t\tRead FILE as a maumfile.\n");
	fprintf(stderr, "-h\t\tPrint this message and exit.\n");
	fprintf(stderr, "-n\t\tDon't actually execute commands, just print them.\n");
	fprintf(stderr, "-B\t\tDon't check files timestamps.\n");
	fprintf(stderr, "-m FILE\t\tRedirect the output to the file specified .\n");
	exit(0);
}


int main(int argc, char **argv) 
{
	// Declarations for getopt
	extern int optind;
	extern char * optarg;
	int ch;
	char * format = "f:hnBm:";
	
	// Default makefile name will be Makefile
	char szMakefile[64] = "Makefile";
	char szTarget[64];
	char szLog[64];

	while((ch = getopt(argc, argv, format)) != -1) 
	{
		switch(ch) 
		{
			case 'f':
				strcpy(szMakefile, strdup(optarg));
				break;
			case 'n':
				break;
			case 'B':
				boolCheckModTimes = 0;
				break;
			case 'm':
				strcpy(szLog, strdup(optarg));
				break;
			case 'h':
			default:
				show_error_message(argv[0]);
				exit(1);
		}
	}

	argc -= optind;
	argv += optind;

	// If there are still arguments, kill the program
	if(argc > 1)
	{
		show_error_message(argv[0]);
		return EXIT_FAILURE;
	}

	// If target is not set, set it to default (first target from makefile)
	if(argc == 1)
	{
		strncpy(szTarget, argv[0], 64);
	}
	else
	{
		strncpy(szTarget, "\0", 64);
	}


	// Parse graph file or die
	if((parse(szMakefile)) == -1) 
	{
		return EXIT_FAILURE;
	}

	// Execute all processes
	if(strcmp(szTarget, "\0") == 0)
	{
		beginprocessing(targetList[0].szTarget);
	}
	else
	{
		beginprocessing(szTarget);
	}
	
	return EXIT_SUCCESS;
}
