/* CSci4061 S2016 Assignment 2
 * section: 7
 * section:
 * section: 
 * date: 03/11/16
 * name: Caleb Biasco, Meghan Jonas, Dennis Ehrhardt
 * id: biasc007, jonas050, ehrh0023  */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include "util.h"

#define ALL_SLOTS_FULL 99

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
	char list[MSG_SIZE];
	int i;
	int none;
	none = 1;
	
	sprintf(list, "\n");
	for (i = 0; i < MAX_USERS; i++)
	{
		if (users[i].status == SLOT_FULL)
		{
			strcat(list, users[i].name);
			strcat(list, "\n");
			none = 0;
		}
	}
	if (none)
	{
		write(fd, "<no users>", MSG_SIZE);
	}
	else
	{
		write(fd, list, MSG_SIZE);
	}
}

/*
 * If number of users is not at max, set up pipes and add a new user
 */
int add_user(user_chat_box_t *users, char *buf, int server_fd)
{
	int slot, err, flags;
	char fd1[2], fd2[2];
	char info[MSG_SIZE];
	
	/* Place user in slot */
	for (slot = 0; slot < MAX_USERS; slot++)
	{
		if (users[slot].status == SLOT_EMPTY) {
			users[slot].status = SLOT_FULL;
			break;
		}
	}
	if (slot == MAX_USERS)
	{
		return ALL_SLOTS_FULL;
	}
	
	/* Pipe failure */
	if (pipe(users[slot].ptoc) == -1 || pipe(users[slot].ctop) == -1)
	{
		return EXIT_FAILURE;
	}
	
	/* Set the pipes to non-blocking */
	fcntl(users[slot].ptoc[0], F_SETFL, F_GETFL | O_NONBLOCK);
	fcntl(users[slot].ptoc[1], F_SETFL, F_GETFL | O_NONBLOCK);
	fcntl(users[slot].ctop[0], F_SETFL, F_GETFL | O_NONBLOCK);
	fcntl(users[slot].ctop[1], F_SETFL, F_GETFL | O_NONBLOCK);
	
	sprintf(info, "Adding user %s...", buf);
	write(server_fd, info, MSG_SIZE);
	
	strcpy(users[slot].name, buf);
	users[slot].child_pid = -1;
	users[slot].pid = fork();
	
	/* If forking failed, exit */
	if (users[slot].pid == -1)
	{
		return EXIT_FAILURE;
	}
	
	/* Child: close appropriate pipes, then exec xterm */
	else if (users[slot].pid == 0)
	{
		close(users[slot].ptoc[1]);
		close(users[slot].ctop[0]);
		sprintf(fd1, "%d", users[slot].ptoc[0]);
		sprintf(fd2, "%d", users[slot].ctop[1]);
		
		err = execl(XTERM_PATH, XTERM, "+hold", "-e", "./shell", fd1, fd2, users[slot].name, (char *) NULL);
		if (err = -1)
		{
			perror("creating xterm failed");
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
	close(users[idx].ptoc[0]);
	close(users[idx].ctop[1]);
}

/*
 * Cleanup single user: close all pipes, kill user's child process, kill user 
 * xterm process, free-up slot.
 * Remember to wait for the appropriate processes here!
 */
void cleanup_user(int idx, user_chat_box_t *users)
{
    close_pipes(idx, users);
    kill(users[idx].child_pid, 9);
    kill(users[idx].pid, 9);
    wait(); // Only wait for the child; cannot wait for grandchild
    users[idx].status = SLOT_EMPTY;
}

/*
 * Cleanup all users: given to you
 */
void cleanup_users(user_chat_box_t *users)
{
	int i;

	for (i = 0; i < MAX_USERS; i++)
    {
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
    close(server_ctrl.ptoc[1]);
    close(server_ctrl.ctop[0]);
    kill(server_ctrl.child_pid, 9);
    kill(server_ctrl.pid, 9);
    wait(); // Only wait for the child; cannot wait for grandchild
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
	char name[MSG_SIZE], msg[MSG_SIZE];
	int target;

	if (strlen(buf) <= strlen(CMD_P2P)+1 || is_empty(buf + strlen(CMD_P2P)))
	{
		write(users[idx].ptoc[1], "User not found", MSG_SIZE);
		return;
	}

	strcpy(name, extract_name(P2P, buf));
	target = find_user_index(users, name);
	if (target == -1)
	{
		write(users[idx].ptoc[1], "User not found", MSG_SIZE);
		return;
	}
	strcpy(buf, buf+strlen(name)+strlen(CMD_P2P)+2);
	sprintf(msg, "%s : %s", users[idx].name, buf);
	strtok(msg, "\n");
	write(users[target].ptoc[1], msg, MSG_SIZE);
	return;
}

int main(int argc, char **argv)
{
	
	/* Open non-blocking bi-directional pipes for communication with server shell */
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
	int cmd, num_users, err, target;
	server.child_pid = -1;
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

		/* Read from the server shell */
		if(read(server.ctop[0], msg, MSG_SIZE) > 0)
		{
			cmd = parse_command(msg);
		
			switch(cmd)
			{
				/* Add the child_pid attribute to server_ctrl_t if empty (\child_pid) */
				case CHILD_PID :
					if (server.child_pid == -1)
					{
						strcpy(msg, extract_name(CHILD_PID, msg));
						server.child_pid = atoi(msg);
					}
					break;
				
				/* Fork a process if a user was added (\add) */
				case ADD_USER :
					if (strlen(msg) <= strlen(CMD_ADD_USER)+1 || is_empty(msg + strlen(CMD_ADD_USER)))
					{
						perror("<NULL name passed>");
						break;
					}
					strcpy(msg, extract_name(ADD_USER, msg));
					err = add_user(user_list, msg, server.ptoc[1]);
					if (err == ALL_SLOTS_FULL)
					{
						perror("<users at maximum>");
					}
					else if (err == EXIT_FAILURE)
					{
						return EXIT_FAILURE;
					}
					break;
					
				/* List all current users. Print "<no users>" if there are no users (\list) */
				case LIST_USERS :
					list_users(user_list, server.ptoc[1]);
					break;

				/* Kicks specified user (\kick) */
				case KICK :
					if (strlen(msg) <= strlen(CMD_KICK)+1 || is_empty(msg + strlen(CMD_KICK)))
					{
						perror("<NULL name passed>");
						break;
					}
					strcpy(msg, extract_name(KICK, msg));
                    target = find_user_index(user_list, msg);
                    if (target == -1)
                    {
                        perror("<user not found>");
                        break;
                    }
					cleanup_user(target, user_list);
					break; 
				
				/* Cleans up all child processes and exits server (\exit) */
				case EXIT :
                    cleanup_users(user_list);
                    cleanup_server(server);
                    return 0;
				
				/* Any text that doesn't start with a command */
				case BROADCAST :
					broadcast_msg(user_list, msg, server.ptoc[1], "SERVER");
					break;
				
				default :;
			}
		}
			
		/* Read from each user shell */
		for (i = 0; i < MAX_USERS; i++)
		{
			/* If the user exists and has written to its pipe */
			if (user_list[i].status == SLOT_FULL && read(user_list[i].ctop[0], msg, MSG_SIZE) > 0)
			{
				cmd = parse_command(msg);
			
				switch(cmd)
				{
					/* Add the child_pid attribute to user_chat_box_t if empty (\child_pid) */
					case CHILD_PID :
						if (user_list[i].child_pid == -1)
						{
							strcpy(msg, extract_name(CHILD_PID, msg));
							user_list[i].child_pid = atoi(msg);
						}
						break;
					
					/* List all current users. Print "<no users>" if there are no users (\list) */
					case LIST_USERS :
                        list_users(user_list, user_list[i].ptoc[1]);
						break;
						
					/* Send a private message to the specified user (\p2p) */
					case P2P :
						if (strlen(msg) <= strlen(CMD_P2P)+1 || is_empty(msg + strlen(CMD_P2P)))
						{
							perror("<NULL name passed>");
							break;
						}
						send_p2p_msg(i, user_list, msg);
						break;
					
					/* Cleans up shell process and exits xterm (\exit) */
					case EXIT :
                        cleanup_user(i, user_list);
						break;
					
					/* Any text that doesn't start with a command */
					case BROADCAST :
						broadcast_msg(user_list, msg, server.ptoc[1], user_list[i].name);
						break;
					
					default :;
				}
			}
			/* Check if the user has left unexpectedly */
            else if (user_list[i].status == SLOT_FULL)
            {
                if (waitpid(user_list[i].pid, NULL, WNOHANG) == -1)
                {
                    close_pipes(i, user_list);
                    kill(user_list[i].child_pid, 9);
                    user_list[i].status = SLOT_EMPTY;
                }
            }
		}	/* while loop ends when server shell sees the \exit command */
	}

	return 0;
}
