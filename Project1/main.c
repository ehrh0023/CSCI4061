#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h> 
#include <unistd.h>

#include "util.h"

#define DEBUG 1


struct node{
	target_t name;
	//struct node *parent[MAX_NODES];
	struct node *child[MAX_CHILDREN];
	int boolRun; //Check if a process has been run yet.
};

struct node graphRoot;

target_t targetList[MAX_NODES]; //Array of target structs
int nTargetCount=-1; //Counter for placing targets in array

//This function will parse makefile input from user or default makeFile. 
int parse(char * lpszFileName)
{
	int nLine = 0;
	char szLine[1024];
	char * lpszLine;

	FILE * fp = file_open(lpszFileName);
	
	int nDependencyCount=0; //Counter for placing dependencies in array inside target
	
	int boolHasCommand = 0; //Boolean for checking if we already have a command

	if(fp == NULL)
	{
		return -1;
	}

	while(file_getline(szLine, fp) != NULL) 
	{
		nLine++;

		//Remove newline character at end if there is one
		lpszLine = strtok(szLine, "\n");

		if (lpszLine != NULL) //Checks if "blank line"
		{
			if (strncmp(lpszLine, "\t", 1) != 0) //If there is no leading tab
			{
				if (strncmp(lpszLine, "#", 1) != 0) //If the line is not a comment
				{
					int boolHasColon = 0;
					while (strlen(lpszLine) > 0) //Check for any illegal characters in line
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
					while (*lpszLine == ' ') {lpszLine++;} //Skip all leading spaces
					if (strchr(lpszLine, ':') == NULL) //If there is no colon in line
					{
						printf("ERROR: No target on Line %d\n", nLine);
						return -1;
					}
					
					nTargetCount++;
					nDependencyCount = 0;
					boolHasCommand = 0;

					lpszLine = strtok(lpszLine, ":");
					if (strchr(lpszLine, ' ')) //If there is a space in the target name
					{
						printf("ERROR: Illegal target name on Line %d\n", nLine);
						return -1;
					}
					int i;
					for (i = 0; i < nTargetCount; i++) //Check for duplicate target names
					{
						if (strcmp(lpszLine, targetList[i].szTarget) == 0)
						{
							printf("ERROR: Duplicate target on Line %d\n", nLine);
							return -1;
						}
					}
					strcpy(targetList[nTargetCount].szTarget, lpszLine);
					
					lpszLine = strtok(NULL, " \t");
					while (lpszLine != NULL)
					{
						strcpy(targetList[nTargetCount].szDependencies[nDependencyCount], lpszLine);
						nDependencyCount++;
						lpszLine = strtok(NULL, " \t");
					}
					targetList[nTargetCount].nDependencyCount = nDependencyCount;
					nDependencyCount = 0;
				}
			}
			else //If there is a leading tab
			{
				while (strlen(lpszLine) > 0) //Check for any illegal characters in line
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
				while (isspace(*lpszLine)) {lpszLine++;} //Skip all leading whitespace
				if (strlen(lpszLine) > 0 && strncmp(lpszLine, "#", 1) != 0) //If the line is not empty or a comment
				{
					if (boolHasCommand) //If we already have a command
					{
						printf("ERROR: Second command on Line %d\n", nLine);
						return -1;
					}
					else //If we don't have a command
					{
						strcpy(targetList[nTargetCount].szCommand, lpszLine);
						boolHasCommand = 1;
					}
				}
			}
		}
	}
	
	//DEBUG CODE: Prints the target information to terminal
	if (DEBUG)
	{
		int i = 0;
		while (nTargetCount >= i)
		{
			printf("%d: %s\n", i+1, targetList[i].szTarget);
			nDependencyCount = targetList[i].nDependencyCount;
			int j = 0;
			while (nDependencyCount > j) {
				printf("\tDependency %d: %s\n", j+1, targetList[i].szDependencies[j]);
				j++;
			}
			printf("\tCommand: %s\n", targetList[i].szCommand);
			i++;
		}
	}

	//Close the makefile. 
	fclose(fp);

/*
	//Building graph here, to use all these instance vars.
	struct node tempnodes[nTargetCount];
	int i=0;
	while(i<nTargetCount){
		tempnodes[i]=new struct node();
		tempnodes[i].name= targetList[i].szCommand;
		i++;
	}
	int temp =0;
	int temp1 =0;
	struct node tempnod = NULL;
	char * tempstr = NULL;
	i =0;
	while(i<nTargetCount){
		if(targetList[i].nDependencyCount == 0){
			graphRoot.child[temp]= *tempnodes[i];
			temp++;
		}
		i++;
	}
	temp=0;
	i =0;
	while(i< nTargetCount){
		tempnod=targetList[i];
		temp=tempnod.nDependencyCount;
		int j=0;
		while(j<temp){
			tempstr= tempnod.szDependencies[j];
			int k=0;
			while(k< nTargetCount){
				if(tempstr==targetList[k].szCommand){
					int l=0;
					while(tempnodes[k].child[l]!=null) l++;
					tempnodes[k].child[l]= *tempnod;
					l=0;
					while(tempnod.parent[l]!=null) l++;
					tempnod.parent[l]= *tempnodes[k];
					k= nTargetCount;
				}
				k++;
			}
			j++;
		}
		i++;
	}
*/


	//Graph
	
	int i=0;
	int found;
	char tempchar[64];

	//node temparray[100];
	while(i<nTargetCount){
		int j=0;
		while(j<targetList[i].nDependencyCount){
			int k=0;
			targetList[i].child[j]=NULL;
			while(k<nTargetCount){
				if(strcmp(targetList[i].szDependencies[j], targetList[k].szTarget)== 0 ){
					int l=0;
					targetList[i].child[j]=&targetList[k];
					k=nTargetCount;
				}
				k++;
			}
			j++;
		}
		i++;
	}

	//delete temp, temp1, tempnod, tempstr; //may also delete tempnodes pointers, since nodes are now linked via graphRoot.

	return 0;
}


void process(target_t** target, int size)
{
	int i, j;
	int stat_loc;
	target_t* n_lvl[10];
	int n_lvl_size = 0;
	char* cmd[12];

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
			}
		}
	}
	
	// Only if the next level isn't empty
	if(n_lvl_size != 0)
	{
		// process the next level
		process(n_lvl, n_lvl_size);
		
		// Excute targets
		for (j = 0; j < size; j++)
		{
			if(target[j]->nStatus == 0)
			{
				// Building Tag
				target[j]->nStatus = 1;
				
				// Fork and execute
				target[j]->pid = fork();
				if(target[j]->pid == 0)
				{
					cmd[0] = target[j]->szCommand;
					for(i = 1; i <= target[j]->nDependencyCount; i++)
					{
						cmd[i] = target[j]->szDependencies[i];
					}		
					cmd[target[j]->nDependencyCount+1] = "\0";		
					execvp(cmd[0], cmd);
					return;
				}
			}
		}
		// Wait until all forks end
		for (j = 0; j < size; j++)
		{
			waitpid(target[j]->pid, &stat_loc, 0);	
		}
	}
}


void beginprocessing(char* t_name)
{
	int i;
	target_t* target = NULL;

	// Find the starting string
	// Prep Status and pid
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
	char szMakefile[64] = "./testcases/make_testcase/Makefile"; // default is Makefile
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

	// at this point, what is left in argv is the targets that were 
	// specified on the command line. argc has the number of them.
	// If getopt is still really confusing,
	// try printing out what's in argv right here, then just running 
	// with various command-line arguments.

	if(argc > 1)
	{
		show_error_message(argv[0]);
		return EXIT_FAILURE;
	}

	//You may start your program by setting the target that make4061 should build.
	//if target is not set, set it to default (first target from makefile)
	if(argc == 1)
	{
		strncpy(szTarget, argv[0], 64);
	}
	else
	{
		strncpy(szTarget, "\0", 64);
	}


	/* Parse graph file or die */
	if((parse(szMakefile)) == -1) 
	{
		return EXIT_FAILURE;
	}

	//after parsing the file, you'll want to check all dependencies (whether they are available targets or files)
	//then execute all of the targets that were specified on the command line, along with their dependencies, etc.
	
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
