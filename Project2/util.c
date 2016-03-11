/* CSci4061 S2016 Assignment 2
 * section: 7
 * section:
 * section: 
 * date: 03/11/16
 * name: Caleb Biasco, Meghan Jonas, Dennis Ehrhardt
 * id: biasc007, jonas050, ehrh0023  */

#include <stdio.h>
#include <string.h>
#include "util.h"

/*
 * Print shell prompt. Pass the username as argument.
 */
void print_prompt(char *name)
{
	printf("%s >> ", name);
}

/*
 * Checks if the first string starts with the second. Return 1 if true.
 */
int starts_with(const char *a, const char *b)
{
	if (strncmp(a, b, strlen(b)) == 0)
		return 1;
	else
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