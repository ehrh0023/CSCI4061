/* CSci4061 S2016 Assignment 1							*
 * login: biasc007										*
 * date: 02/17/16										*
 * name: Caleb Biasco, Dennis Ehrhardt, Meghan Jonas	*
 * id: biasc007, ehrh0023, jonas050 					*/

 

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
int boolRunCommands = 1; // Boolean for -n command

// This function will parse makefile input from user or default makefile and build an execution graph
int parse(char * lpszFileName)
{
	int i, j, k;
	int nLine = 0;
	char szLine[1024];
	char * lpszLine;

	FILE * fp = file_open(lpszFileName);
	
	int nDependencyCount = 0; // Counter for placing dependencies in array inside target

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
							printf("make4061: ERROR: Illegal character on Line %d\n", nLine);
							return EXIT_FAILURE;
						}
					}
					
					lpszLine = szLine;
					// Skip all leading spaces
					while (*lpszLine == ' ') {lpszLine++;}
					// If there is no colon in line
					if (strchr(lpszLine, ':') == NULL)
					{
						printf("make4061: ERROR: No target on Line %d\n", nLine);
						return EXIT_FAILURE;
					}
					
					nTargetCount++;
					nDependencyCount = 0;
					targetList[nTargetCount-1].boolHasCommand = 0;

					lpszLine = strtok(lpszLine, ":");
					// If there is a space in the target name
					if (strchr(lpszLine, ' '))
					{
						printf("make4061: ERROR: Illegal target name on Line %d\n", nLine);
						return EXIT_FAILURE;
					}
					// If there is a duplicate target name
					for (i = 0; i < nTargetCount-1; i++)
					{
						if (strcmp(lpszLine, targetList[i].szTarget) == 0)
						{
							printf("make4061: ERROR: Duplicate target on Line %d\n", nLine);
							return EXIT_FAILURE;
						}
					}
					strcpy(targetList[nTargetCount-1].szTarget, lpszLine);
					
					// Add dependencies to target struct
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
					if (isalnum(*lpszLine) || isblank(*lpszLine) || strchr("-.'\"?!", *lpszLine))
					{
						lpszLine++;
					}
					else
					{
						printf("make4061: ERROR: Illegal character on Line %d\n", nLine);
						return EXIT_FAILURE;
					}
				}

				lpszLine = szLine;
				// Skip all leading whitespace
				while (isspace(*lpszLine)) {lpszLine++;}
				// If the line is not empty or a comment
				if (strlen(lpszLine) > 0 && strncmp(lpszLine, "#", 1) != 0)
				{
					// If we already have a command
					if (targetList[nTargetCount-1].boolHasCommand) 
					{
						printf("make4061: ERROR: Second command on Line %d\n", nLine);
						return EXIT_FAILURE;
					}
					// If we don't have a command
					else
					{
						strcpy(targetList[nTargetCount-1].szCommand, lpszLine);
						targetList[nTargetCount-1].boolHasCommand = 1;
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
			if (targetList[i].boolHasCommand) printf("\tCommand: %s\n", targetList[i].szCommand);
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
	
	return EXIT_SUCCESS;
}

// This function will find dependencies, compare modification times, and execute each level of the execution graph
int process(target_t** target, int size)
{
	int i, j, k;
	int stat_loc;
	int boolHasChildren = 0; // Boolean for creating new level for children
	int boolIsTarget; // Boolean for checking target and file existence
	int nCompareTimes; // Holds output for compare_modification_time method
	target_t* nextLevel[10]; // Stores children to be executed in next level
	int nLevelSize = 0;
	char** cmd;
	int err;

	// Create next level
	for (j = 0; j < size; j++)
	{
		// Add children
		for (i = 0; i < target[j]->nDependencyCount; i++)
		{
			if (target[j]->child[i] != NULL)
			{
				nextLevel[nLevelSize] = target[j]->child[i];
				nLevelSize++;
				boolHasChildren = 1;
			}
		}
	}
	
	cmd = (char**) malloc(sizeof(char*) * 12);
	// If the next level isn't empty
	if(boolHasChildren)
	{
		// Process the next level; if it fails, exit this process
		if (process(nextLevel, nLevelSize) == -1)
		{
			return -1;
		}
	}
	// If checking for modified dependencies
	if (boolCheckModTimes)
	{
		for (j = 0; j < size; j++)
		{
			// If there are no dependencies, must re-build
			if (target[j]->nDependencyCount > 0) { target[j]->boolModified = 0; }
			else { target[j]->boolModified = 1; }
			
			for (i = 0; i < target[j]->nDependencyCount; i++)
			{
				// Check if the dependency is a target 
				boolIsTarget = 0;
				for (k = 0; k < target[j]->nDependencyCount; k++)
				{
					if (target[j]->child[k] == NULL) { k = target[j]->nDependencyCount; }
					else if (strcmp(target[j]->szDependencies[i], target[j]->child[k]->szTarget) == 0)
					{
						boolIsTarget = 1;
						k = target[j]->nDependencyCount;
					}
				}
				// If a non-target dependency doesn't exist
				if (!boolIsTarget && is_file_exist(target[j]->szDependencies[i]) == -1)
				{
					printf("make4061: ERROR: no rule for '%s'\n", target[j]->szDependencies[i]);
					free(cmd);
					kill((long)getpid(), SIGKILL);
					return -1;
				}
				// If any dependencies have been modified (targets and non-targets)
				nCompareTimes = compare_modification_time(target[j]->szTarget, target[j]->szDependencies[i]);
				if (!target[j]->boolModified && nCompareTimes == 2 || (nCompareTimes == -1 && target[j]->boolHasCommand) || (target[j]->child[i] != NULL && target[j]->child[i]->boolModified))
				{ 
					target[j]->boolModified = 1;
				}
			}
		}
		// Execute targets
		for (j = 0; j < size; j++)
		{
			// If target has a command and dependencies have been modified
			if (target[j]->boolHasCommand && target[j]->boolModified && target[j]->nStatus == 0)
			{
				// Building tag
				target[j]->nStatus = 1;
				
				// Fork and execute
				target[j]->pid = fork();
				if (target[j]->pid == 0)
				{
					makeargv(target[j]->szCommand, " ", &cmd);
					printf("%s\n", target[j]->szCommand);
					if (boolRunCommands)
					{
						err = execvp(cmd[0], cmd);
						if(err == -1)
						{
							_exit(EXIT_FAILURE);
						}
					}
					exit(0);
					return;
				}
			}
		}
		// Wait until all forks end
		for (j = 0; j < size; j++)
		{
			// If the pid is 0, don't wait
			if (target[j]->pid)
			{
				waitpid(target[j]->pid, &stat_loc, 0);
				
				// If a command exits with an error
				if (WIFEXITED(stat_loc) && WEXITSTATUS(stat_loc) != 0)
				{
					printf("make4061: ERROR: '%s' failed\n", target[j]->szCommand);
					free(cmd);
					kill((long)getpid(), SIGKILL);
					return -1;
				}
			}
		}
	}
	// If not checking for modifications
	else
	{
		// Excute targets
		for (j = 0; j < size; j++)
		{
			for (i = 0; i < target[j]->nDependencyCount; i++)
			{
				// Check if the dependency is a target
				boolIsTarget = 0;
				for (k = 0; k < target[j]->nDependencyCount; k++)
				{
					if (target[j]->child[k] == NULL) { k = target[j]->nDependencyCount; }
					else if (strcmp(target[j]->szDependencies[i], target[j]->child[k]->szTarget) == 0)
					{
						boolIsTarget = 1;
						k = target[j]->nDependencyCount;
					}
				}
				// If a non-target dependency doesn't exist
				if (!boolIsTarget && is_file_exist(target[j]->szDependencies[i]) == -1)
				{
					printf("make4061: ERROR: no rule for '%s'\n", target[j]->szDependencies[i]);
					free(cmd);
					kill((long)getpid(), SIGKILL);
					return -1;
				}
			}
			// If target has a command
			if (target[j]->boolHasCommand && target[j]->nStatus == 0)
			{
				// Building tag
				target[j]->nStatus = 1;
				
				// Fork and execute
				target[j]->pid = fork();
				if (target[j]->pid == 0)
				{
					makeargv(target[j]->szCommand, " ", &cmd);
					printf("%s\n", target[j]->szCommand);
					if (boolRunCommands)
					{
						err = execvp(cmd[0], cmd);
						if(err == -1)
						{
							_exit(EXIT_FAILURE);
						}
					}
					exit(0);
					return;
				}
			}
		}
		// Wait until all forks end
		for (j = 0; j < size; j++)
		{
			// If the pid is 0, don't wait
			if (target[j]->pid)
			{
				waitpid(target[j]->pid, &stat_loc, 0);
				
				// If a command exits with an error
				if (WIFEXITED(stat_loc) && WEXITSTATUS(stat_loc) != 0)
				{
					printf("make4061: ERROR: '%s' failed\n", target[j]->szCommand);
					free(cmd);
					kill((long)getpid(), SIGKILL);
					return -1;
				}
			}
		}
	}
	free(cmd);
	return 0;
}

// This function will check for the target and wrap all the processes
int begin_processing(char* t_name)
{
	int i;
	target_t* target = NULL;
	char** cmd = (char**) malloc(sizeof(char*) * 12);

	// Find the initial target and prep all statuses and pids
	for (i = 0; i < nTargetCount; i++)
	{
		targetList[i].nStatus = 0;
		targetList[i].pid = -1;
		if (strcmp(t_name, targetList[i].szTarget) == 0)
		{
			target = &targetList[i];
		}

	}
	// If an initial target has been found
	if (target != NULL)
	{
		// Run process
		if (process(&target, 1) == -1)
		{
			return -1;
		}
		// If no files have been modified
		else if (boolCheckModTimes && target->boolModified == 0)
		{
			printf("make4061: '%s' is up to date.\n", t_name);
			return 0;
		}
		else
		{
			return 0;
		}
	}
	else
	{
		printf("make4061: ERROR: No target '%s'\n", t_name);
		return -1;
	}
}

void show_error_message(char * lpszFileName)
{
	fprintf(stderr, "Usage: %s [options] [target] : only single target is allowed.\n", lpszFileName);
	fprintf(stderr, "-f FILE\t\tRead FILE as a makefile.\n");
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
	
	int fd = -99;

	while((ch = getopt(argc, argv, format)) != -1) 
	{
		switch(ch) 
		{
			case 'f':
				strcpy(szMakefile, strdup(optarg));
				break;
			case 'n':
				boolRunCommands = 0;
				break;
			case 'B':
				boolCheckModTimes = 0;
				break;
			case 'm':
				strcpy(szLog, strdup(optarg));
				if ((fd = open(szLog, O_RDWR|O_CREAT|O_TRUNC, 0666)) < 0)
				{
					printf("make4061: ERROR: Could not open file\n");
					return EXIT_FAILURE;
				}
				dup2(fd, 1);
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
	if((parse(szMakefile)) == EXIT_FAILURE) 
	{
		return EXIT_FAILURE;
	}

	// Execute all processes
	if(strcmp(szTarget, "\0") == 0)
	{
		if (begin_processing(targetList[0].szTarget) == -1)
		{
			return EXIT_FAILURE;
		}
	}
	else
	{
		if (begin_processing(szTarget) == -1)
		{
			return EXIT_FAILURE;
		}
	}
	
	if (fd >= 0) { close(fd); }
	
	return EXIT_SUCCESS;
}
