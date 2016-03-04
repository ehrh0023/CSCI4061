#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include "util.h"

/*
 * Identify the command used at the shell 
 */
int parse_command(char *buf)
{
	int cmd;

	if (starts_with(buf, CMD_CHILD_PID))
		cmd = CHILD_PID;
	else if (starts_with(buf, CMD_P2P))
		cmd = P2P;
	else if (starts_with(buf, CMD_LIST_USERS))
		cmd = LIST_USERS;
	else if (starts_with(buf, CMD_ADD_USER))
		cmd = ADD_USER;
	else if (starts_with(buf, CMD_EXIT))
		cmd = EXIT;
	else if (starts_with(buf, CMD_KICK))
		cmd = KICK;
	else
		cmd = BROADCAST;

	return cmd;
}

/*
 * List the existing users on the server shell
 */
int list_users(user_chat_box_t *users, int fd)
{
	/* 
	 * Construct a list of user names
	 * Don't forget to send the list to the requester!
	 */
	 
	 /***** Insert YOUR code *******/
}

/*
 * Add a new user
 */
int add_user(user_chat_box_t *users, char *buf, int server_fd)
{
	/***** Insert YOUR code *******/
	/* 
	 * Check if user limit reached.
	 *
	 * If limit is okay, add user, set up non-blocking pipes and
	 * notify on server shell
	 *
	 * NOTE: You may want to remove any newline characters from the name string 
	 * before adding it. This will help in future name-based search.
	 */
	int slot, err, flags;
	char fd1[2], fd2[2];
	char info[MSG_SIZE];
	for (slot = 0; slot < MAX_USERS; slot++)
	{
		if (users[slot].status == SLOT_EMPTY) {
			users[slot].status = SLOT_FULL;
			break;
		}
	}
	if (slot == MAX_USERS)
		return 99;
	 
	if (pipe(users[slot].ptoc) == -1 || pipe(users[slot].ctop) == -1)
	{
		return EXIT_FAILURE;
	}
	
	flags = fcntl(users[slot].ptoc[0], F_GETFL, 0);
	fcntl(users[slot].ptoc[0], F_SETFL, flags | O_NONBLOCK);
	flags = fcntl(users[slot].ptoc[1], F_GETFL, 0);
	fcntl(users[slot].ptoc[1], F_SETFL, flags | O_NONBLOCK);
	flags = fcntl(users[slot].ctop[0], F_GETFL, 0);
	fcntl(users[slot].ctop[0], F_SETFL, flags | O_NONBLOCK);
	flags = fcntl(users[slot].ctop[1], F_GETFL, 0);
	fcntl(users[slot].ctop[1], F_SETFL, flags | O_NONBLOCK);
	
	sprintf(info, "Adding user %s...", buf);
	write(server_fd, info, MSG_SIZE);
	
	strcpy(users[slot].name, buf);
	users[slot].pid = fork();
	if (users[slot].pid == -1)
	{
		return EXIT_FAILURE;
	}
	else if (users[slot].pid == 0)
	{
		close(users[slot].ptoc[1]);
		close(users[slot].ctop[0]);
		sprintf(fd1, "%d", users[slot].ptoc[0]);
		sprintf(fd2, "%d", users[slot].ctop[1]);
		
		err = execl(XTERM_PATH, XTERM, "+hold", "-e", "./shell", fd1, fd2, users[slot].name, (char *) NULL);
		if (err = -1)
		{
			perror("error");
			return EXIT_FAILURE;
		}
		exit(0);
	}
	return EXIT_SUCCESS;
}

/*
 * Broadcast message to all users. Completed for you as a guide to help with other commands :-).
 */
int broadcast_msg(user_chat_box_t *users, char *buf, int fd, char *sender)
{
	int i;
	const char *msg = "Broadcasting...", *s;
	char text[MSG_SIZE];

	/* Notify on server shell */
	if (write(fd, msg, strlen(msg) + 1) < 0)
		perror("writing to server shell");
	
	/* Send the message to all user shells */
	s = strtok(buf, "\n");
	sprintf(text, "%s : %s", sender, s);
	for (i = 0; i < MAX_USERS; i++) {
		if (users[i].status == SLOT_EMPTY)
			continue;
		if (write(users[i].ptoc[1], text, strlen(text) + 1) < 0)
			perror("write to child shell failed");
	}
}

/*
 * Close all pipes for this user
 */
void close_pipes(int idx, user_chat_box_t *users)
{
	/***** Insert YOUR code *******/
}

/*
 * Cleanup single user: close all pipes, kill user's child process, kill user 
 * xterm process, free-up slot.
 * Remember to wait for the appropriate processes here!
 */
void cleanup_user(int idx, user_chat_box_t *users)
{
	/***** Insert YOUR code *******/
}

/*
 * Cleanup all users: given to you
 */
void cleanup_users(user_chat_box_t *users)
{
	int i;

	for (i = 0; i < MAX_USERS; i++) {
		if (users[i].status == SLOT_EMPTY)
			continue;
		cleanup_user(i, users);
	}
}

/*
 * Cleanup server process: close all pipes, kill the parent process and its 
 * children.
 * Remember to wait for the appropriate processes here!
 */
void cleanup_server(server_ctrl_t server_ctrl)
{
	/***** Insert YOUR code *******/
}

/*
 * Utility function.
 * Find user index for given user name.
 */
int find_user_index(user_chat_box_t *users, char *name)
{
	int i, user_idx = -1;

	if (name == NULL) {
		fprintf(stderr, "NULL name passed.\n");
		return user_idx;
	}
	for (i = 0; i < MAX_USERS; i++) {
		if (users[i].status == SLOT_EMPTY)
			continue;
		if (strncmp(users[i].name, name, strlen(name)) == 0) {
			user_idx = i;
			break;
		}
	}

	return user_idx;
}

/*
 * Utility function.
 * Given a command's input buffer, extract name.
 */
char *extract_name(int cmd, char *buf)
{
	char *s = NULL;

	s = strtok(buf, " ");
	s = strtok(NULL, " ");
	if (cmd == P2P)
		return s;	/* s points to the name as no newline after name in P2P */
	s = strtok(s, "\n");	/* other commands have newline after name, so remove it*/
	return s;
}

/*
 * Send personal message. Print error on the user shell if user not found.
 */
void send_p2p_msg(int idx, user_chat_box_t *users, char *buf)
{
	/* get the target user by name (hint: call (extract_name() and send message */
	
	/***** Insert YOUR code *******/
}

int main(int argc, char **argv)
{
	
	/***** Insert YOUR code *******/
	
	/* open non-blocking bi-directional pipes for communication with server shell */
	server_ctrl_t server;
	if (pipe(server.ptoc) == -1 || pipe(server.ctop) == -1)
	{
		return EXIT_FAILURE;
	}
	fcntl(server.ptoc[0], F_SETFL, F_GETFL | O_NONBLOCK);
	fcntl(server.ptoc[1], F_SETFL, F_GETFL | O_NONBLOCK);
	fcntl(server.ctop[0], F_SETFL, F_GETFL | O_NONBLOCK);
	fcntl(server.ctop[1], F_SETFL, F_GETFL | O_NONBLOCK);

	/* Fork the server shell */
	server.pid = fork();
	if (server.pid == -1)
	{
		return EXIT_FAILURE;
	}
	else if (server.pid == 0)
	{
		/* 
	 	 * Inside the child.
		 * Start server's shell.
	 	 * exec the SHELL program with the required program arguments.
	 	 */
		close(server.ptoc[1]);
		close(server.ctop[0]);
		char fd1[2], fd2[2];
		int err;
		sprintf(fd1, "%d", server.ptoc[0]);
		sprintf(fd2, "%d", server.ctop[1]);
		err = execlp("./shell", "./shell", fd1, fd2, "SERVER", (char *) NULL);
		if (err == -1)
		{
			_exit(EXIT_FAILURE);
		}
	}
	/* Inside the parent. This will be the most important part of this program. */
	char msg[MSG_SIZE];
	int cmd, num_users, err;
	num_users = 0;
	user_chat_box_t user_list[MAX_USERS];
	int i;
	for (i = 0; i < MAX_USERS; i++)
	{
		user_list[i].status = SLOT_EMPTY;
	}
	close(server.ptoc[0]);
	close(server.ctop[1]);

	/* Start a loop which runs every 1000 usecs.
	 * The loop should read messages from the server shell, parse them using the 
	 * parse_command() function and take the appropriate actions. */
	while (1) {
		/* Let the CPU breathe */
		usleep(1000);

		/* 
		 * 1. Read the message from server's shell, if any
		 * 2. Parse the command
		 * 3. Begin switch statement to identify command and take appropriate action
		 *
		 * 		List of commands to handle here:
		 * 			CHILD_PID
		 * 			LIST_USERS
		 * 			ADD_USER
		 * 			KICK
		 * 			EXIT
		 * 			BROADCAST 
		 */
		if(read(server.ctop[0], msg, MSG_SIZE) > 0)
		{
			cmd = parse_command(msg);
		
			switch(cmd)
			{
				case CHILD_PID :
					break;
				
				case LIST_USERS :
					break;
				
			/* Fork a process if a user was added (ADD_USER) */
				case ADD_USER :
					strcpy(msg, extract_name(ADD_USER, msg));
					err = add_user(user_list, msg, server.ptoc[1]);
					if (err == 99)
					{
						write(server.ptoc[1], "ERROR: users at maximum", MSG_SIZE);
					}
					else if (err == EXIT_FAILURE)
					{
						return EXIT_FAILURE;
					}
					break;		

				case KICK :			
					break; 
				
				case EXIT :
					break;
				
				case BROADCAST :
					broadcast_msg(user_list, msg, server.ptoc[1], "SERVER");
					break;
				
				default :
				;
				
			}
			/* Back to our main while loop for the "parent" */
			/* 
			 * Now read messages from the user shells (ie. LOOP) if any, then:
			 * 		1. Parse the command
			 * 		2. Begin switch statement to identify command and take appropriate action
			 *
			 * 		List of commands to handle here:
			 * 			CHILD_PID
			 * 			LIST_USERS
			 * 			P2P
			 * 			EXIT
			 * 			BROADCAST
			 *
			 * 		3. You may use the failure of pipe read command to check if the 
			 * 		user chat windows has been closed. (Remember waitpid with WNOHANG 
			 * 		from recitations?)
			 * 		Cleanup user if the window is indeed closed.
			 */

		}	/* while loop ends when server shell sees the \exit command */
	}

	return 0;
}
