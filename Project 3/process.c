#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#include "process.h"

// messaging config
int WINDOW_SIZE;
int MAX_DELAY;
int TIMEOUT;
int DROP_RATE;

// process information
process_t myinfo;
int mailbox_id;
// a message id is used by the receiver to distinguish a message from other messages
// you can simply increment the message id once the message is completed
int message_id = 0;
// the message status is used by the sender to monitor the status of a message
message_status_t message_stats;
// the message is used by the receiver to store the actual content of a message
message_t *message;

int num_available_packets; // number of packets that can be sent (0 <= n <= WINDOW_SIZE)
int is_receiving = 0; // a helper variable may be used to handle multiple senders

int num_timeouts = 0; // counter variable for sender timeouts

/**
 * DONE
 * 1. Save the process information to a file and a process structure for future use.
 * 2. Setup a message queue with a given key.
 * 3. Setup the signal handlers (SIGIO for handling packet, SIGALRM for timeout).
 * Return 0 if success, -1 otherwise.
 */
int init(char *process_name, key_t key, int wsize, int delay, int to, int drop) {
    myinfo.pid = getpid();
    strcpy(myinfo.process_name, process_name);
    myinfo.key = key;

    // open the file
    FILE* fp = fopen(myinfo.process_name, "wb");
    if (fp == NULL) {
        printf("Failed opening file: %s\n", myinfo.process_name);
        return -1;
    }
    // write the process_name and its message keys to the file
    if (fprintf(fp, "pid:%d\nprocess_name:%s\nkey:%d\n", myinfo.pid, myinfo.process_name, myinfo.key) < 0) {
        printf("Failed writing to the file\n");
        return -1;
    }
    fclose(fp);

    WINDOW_SIZE = wsize;
    MAX_DELAY = delay;
    TIMEOUT = to;
    DROP_RATE = drop;

    printf("[%s] pid: %d, key: %d\n", myinfo.process_name, myinfo.pid, myinfo.key);
    printf("window_size: %d, max delay: %d, timeout: %d, drop rate: %d%%\n", WINDOW_SIZE, MAX_DELAY, TIMEOUT, DROP_RATE);

    // setup a message queue and save the id to the mailbox_id
	mailbox_id = msgget(myinfo.key, IPC_CREAT | IPC_EXCL | 0666);
	if (errno == EEXIST) {
		printf("Key already exists: %s\n", myinfo.key);
		return -1;
	}
	else if (mailbox_id == -1) {
		printf("Message queue creation failed\n");
		return -1;
	}

    //set the signal handlers for receiving packets
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGIO);
	sigaddset(&set, SIGALRM);
	
	struct sigaction actIO;
	actIO.sa_handler = receive_packet;
	actIO.sa_mask = set;
	actIO.sa_flags = 0;
	sigaction(SIGIO, &actIO, NULL);
	
	struct sigaction actALRM;
	actALRM.sa_handler = timeout_handler;
	actALRM.sa_mask = set;
	actALRM.sa_flags = 0;
	sigaction(SIGALRM, &actALRM, NULL);
	
    return 0;
}

/**
 * Get a process' information and save it to the process_t struct.
 * Return 0 if success, -1 otherwise.
 */
int get_process_info(char *process_name, process_t *info) {
    char buffer[MAX_SIZE];
    char *token;

    // open the file for reading
    FILE* fp = fopen(process_name, "r");
    if (fp == NULL) {
        return -1;
    }
    // parse the information and save it to a process_info struct
    while (fgets(buffer, MAX_SIZE, fp) != NULL) {
        token = strtok(buffer, ":");
        if (strcmp(token, "pid") == 0) {
            token = strtok(NULL, ":");
            info->pid = atoi(token);
        } else if (strcmp(token, "process_name") == 0) {
            token = strtok(NULL, ":");
            strcpy(info->process_name, token);
        } else if (strcmp(token, "key") == 0) {
            token = strtok(NULL, ":");
            info->key = atoi(token);
        }
    }
    fclose(fp);
    return 0;
}

/**
 * DONE Send a packet to a mailbox identified by the mailbox_id, and send a SIGIO to the pid.
 * Return 0 if success, -1 otherwise.
 */
int send_packet(packet_t *packet, int mailbox_id, int pid) {
	if (msgsnd(mailbox_id, packet, sizeof(packet_t), 0) < 0) {
		perror("msgsnd failed");
		return -1;
	}
	kill(pid, SIGIO);
    return 0;
}

/**
 * Get the number of packets needed to send a data, given a packet size.
 * Return the number of packets if success, -1 otherwise.
 */
int get_num_packets(char *data, int packet_size) {
    if (data == NULL) {
        return -1;
    }
    if (strlen(data) % packet_size == 0) {
        return strlen(data) / packet_size;
    } else {
        return (strlen(data) / packet_size) + 1;
    }
}

/**
 * Create packets for the corresponding data and save it to the message_stats.
 * Return 0 if success, -1 otherwise.
 */
int create_packets(char *data, message_status_t *message_stats) {
    if (data == NULL || message_stats == NULL) {
        return -1;
    }
    int i, len;
    for (i = 0; i < message_stats->num_packets; i++) {
        if (i == message_stats->num_packets - 1) {
            len = strlen(data)-(i*PACKET_SIZE);
        } else {
            len = PACKET_SIZE;
        }
        message_stats->packet_status[i].is_sent = 0;
        message_stats->packet_status[i].ACK_received = 0;
        message_stats->packet_status[i].packet.message_id = -1;
        message_stats->packet_status[i].packet.mtype = DATA;
        message_stats->packet_status[i].packet.pid = myinfo.pid;
        strcpy(message_stats->packet_status[i].packet.process_name, myinfo.process_name);
        message_stats->packet_status[i].packet.num_packets = message_stats->num_packets;
        message_stats->packet_status[i].packet.packet_num = i;
        message_stats->packet_status[i].packet.total_size = strlen(data);
        memcpy(message_stats->packet_status[i].packet.data, data+(i*PACKET_SIZE), len);
        message_stats->packet_status[i].packet.data[len] = '\0';
    }
    return 0;
}

/**
 * Get the index of the next packet to be sent.
 * Return the index of the packet if success, -1 otherwise.
 */
int get_next_packet(int num_packets) {
    int packet_idx = rand() % num_packets;
    int i = 0;

    i = 0;
    while (i < num_packets) {
        if (message_stats.packet_status[packet_idx].is_sent == 0) {
            // found a packet that has not been sent
            return packet_idx;
        } else if (packet_idx == num_packets-1) {
            packet_idx = 0;
        } else {
            packet_idx++;
        }
        i++;
    }
    // all packets have been sent
    return -1;
}

/**
 * Use probability to simulate packet loss.
 * Return 1 if the packet should be dropped, 0 otherwise.
 */
int drop_packet() {
    if (rand() % 100 > DROP_RATE) {
        return 0;
    }
    return 1;
}

/**
 * DONE Send a message (broken down into multiple packets) to another process.
 * We first need to get the receiver's information and construct the status of
 * each of the packet.
 * Return 0 if success, -1 otherwise.
 */
int send_message(char *receiver, char* content) {
    if (receiver == NULL || content == NULL) {
        printf("Receiver or content is NULL\n");
        return -1;
    }
	if (strcmp(receiver, myinfo.process_name) == 0) {
		printf("Cannot send to self\n");
		return -1;
	}
    // get the receiver's information
    if (get_process_info(receiver, &message_stats.receiver_info) < 0) {
        printf("Failed getting %s's information.\n", receiver);
        return -1;
    }
    // get the receiver's mailbox id
    message_stats.mailbox_id = msgget(message_stats.receiver_info.key, 0666);
    if (message_stats.mailbox_id == -1) {
        printf("Failed getting the receiver's mailbox.\n");
        return -1;
    }
    // get the number of packets
    int num_packets = get_num_packets(content, PACKET_SIZE);
    if (num_packets < 0) {
        printf("Failed getting the number of packets.\n");
        return -1;
    }
    // set the number of available packets
    if (num_packets > WINDOW_SIZE) {
        num_available_packets = WINDOW_SIZE;
    } else {
        num_available_packets = num_packets;
    }
    // setup the information of the message
    message_stats.is_sending = 1;
    message_stats.num_packets_received = 0;
    message_stats.num_packets = num_packets;
    message_stats.packet_status = malloc(num_packets * sizeof(packet_status_t));
    if (message_stats.packet_status == NULL) {
        return -1;
    }
    // parition the message into packets
    if (create_packets(content, &message_stats) < 0) {
        printf("Failed paritioning data into packets.\n");
        message_stats.is_sending = 0;
        free(message_stats.packet_status);
        return -1;
    }

    // DONE send packets to the receiver
    // the number of packets sent at a time depends on the WINDOW_SIZE.
    // you need to change the message_id of each packet (initialized to -1)
    // with the message_id included in the ACK packet sent by the receiver

	int first_packet;
	if ((first_packet = get_next_packet(num_packets)) == -1) {
		printf("No packets to initially send.\n");
		return -1;
	}
	if (send_packet(&(message_stats.packet_status[first_packet].packet), message_stats.mailbox_id, message_stats.receiver_info.pid) < 0) {
		return -1;
	}
	printf("Send a packet [%d] to pid:%d\n", first_packet, message_stats.receiver_info.pid);
	message_stats.packet_status[first_packet].is_sent = 1;
	
	while (message_stats.is_sending) {
		alarm(TIMEOUT);
        pause();
    }
	
	if (message_stats.packet_status) {
		free(message_stats.packet_status);
	}
	
    return 0;
}

/**
 * DONE Handle TIMEOUT. Resend previously sent packets whose ACKs have not been
 * received yet. Reset the TIMEOUT.
 */
void timeout_handler(int sig) {
	num_timeouts++;
	if (num_timeouts == MAX_TIMEOUT) {
		num_timeouts = 0;
		message_stats.is_sending = 0;
		return;
	}
	int i;
	int message_length = message_stats.num_packets;
	printf("TIMEOUT!\n");
	for (i = 0; i < message_length; i++) {
		if (message_stats.packet_status[i].is_sent && !message_stats.packet_status[i].ACK_received) {
			if (send_packet(&(message_stats.packet_status[i].packet), message_stats.mailbox_id, message_stats.receiver_info.pid) < 0) {
				printf("send_packet error\n");
				return;
			}
			printf("Send a packet [%d] to pid:%d\n", i, message_stats.receiver_info.pid);
		}
	}
}

/**
 * DONE Send an ACK to the sender's mailbox.
 * The message id is determined by the receiver and has to be included in the ACK packet.
 * Return 0 if success, -1 otherwise.
 */
int send_ACK(int mailbox_id, pid_t pid, int packet_num) {
    // DONE construct an ACK packet

    if(mailbox_id == -1 || pid == -1 || packet_num == -1){
        printf("send_ACK criteria missing");
        return -1;
    }

    packet_t ackp;
    ackp.mtype = ACK;
    ackp.pid = pid;
    ackp.packet_num = packet_num;
	ackp.message_id = message_id;

    int delay = rand() % MAX_DELAY;
    sleep(delay);

    // DONE send an ACK for the packet it received
    if (msgsnd(mailbox_id, (void *) &ackp, sizeof(packet_t), 0) < 0) {
        perror("ACK send failed");
        return -1;
    }
    kill(pid, SIGIO);
    return 0;
}

/**
 * TODO Handle DATA packet. Save the packet's data and send an ACK to the sender.
 * You should handle unexpected cases such as duplicate packet, packet for a different message,
 * packet from a different sender, etc.
 */
void handle_data(packet_t *packet, process_t *sender, int sender_mailbox_id) {
    if(message->sender.pid == -1)
        message->sender = *sender;
    // if the packet is from a different sender
    if(strcmp(message->sender.process_name, sender->process_name) != 0)
        return;
    
    // DONE? if the packet is from a different message
	if ((message->num_packets_received > 0 && packet->message_id == -1) || \
		(message_id != packet->message_id && packet->message_id > -1)) {
		return;
	}
	
	if (!message->data) {
		message->data = (char *) malloc(packet->num_packets*PACKET_SIZE);
	}
	if (!message->is_received) {
		message->is_received = (int *) malloc(packet->num_packets*sizeof(int));
	}
	
    send_ACK(sender_mailbox_id, packet->pid, packet->packet_num);
	printf("Send an ACK for packet %d to pid:%d\n", packet->packet_num, sender->pid);

    // if message is not a duplicate
    if(message->is_received[packet->packet_num] == 0) {
        message->is_received[packet->packet_num] = 1;
        strncpy(message->data + packet->packet_num*PACKET_SIZE, packet->data, PACKET_SIZE);
        message->num_packets_received++;
        if(message->num_packets_received == packet->num_packets){
            message->is_complete = 1;
			message_id++;
			printf("All packets received.\n");
        }
    }
}

void set_message_id(int m_id) {
	int i;
	int message_length = message_stats.num_packets;
	for (i = 0; i < message_length; i++) {
		message_stats.packet_status[i].packet.message_id = m_id;
	}
}

/**
 * DONE: Handle ACK packet. Update the status of the packet to indicate that the packet
 * has been successfully received and reset the TIMEOUT.
 * You should handle unexpected cases such as duplicate ACKs, ACK for completed message, etc.
 */
void handle_ACK(packet_t *packet) {
	// if a message_id is not yet set
	if (message_stats.packet_status[0].packet.message_id == -1) {
		set_message_id(packet->message_id);
	}
	
	// if the packet is a duplicate or from another message
	if (packet->message_id != message_stats.packet_status[0].packet.message_id ||
		message_stats.packet_status[packet->packet_num].ACK_received) {
		return;
	}
	
	alarm(0);
	num_timeouts = 0;
	printf("Receive an ACK for packet [%d]\n", packet->packet_num);
	message_stats.packet_status[packet->packet_num].ACK_received = 1;
	message_stats.num_packets_received++;
	// if all packets have been received
	if (message_stats.num_packets_received == message_stats.num_packets) {
		message_stats.is_sending = 0;
		printf("All packets sent.\n");
		return;
	}
	int next_packet;
	if (message_stats.num_packets_received == 1) {
		int i;
		for (i = 0; i < WINDOW_SIZE; i++) {
			// if no more packets to send
			if ((next_packet = get_next_packet(message_stats.num_packets)) == -1) {
				return;
			}
			message_stats.packet_status[next_packet].is_sent = 1;
			if (send_packet(&(message_stats.packet_status[next_packet].packet), message_stats.mailbox_id, message_stats.receiver_info.pid) < 0) {
				printf("send_packet error\n");
				return;
			}
			printf("Send a packet [%d] to pid:%d\n", next_packet, message_stats.receiver_info.pid);
		}
	}
	else {
		// if no more packets to send
		if ((next_packet = get_next_packet(message_stats.num_packets)) == -1) {
			return;
		}
		message_stats.packet_status[next_packet].is_sent = 1;
		if (send_packet(&(message_stats.packet_status[next_packet].packet), message_stats.mailbox_id, message_stats.receiver_info.pid) < 0) {
			printf("send_packet error\n");
			return;
		}
		printf("Send a packet [%d] to pid:%d\n", next_packet, message_stats.receiver_info.pid);
	}
}

/**
 * Get the next packet (if any) from a mailbox.
 * Return 0 (false) if there is no packet in the mailbox
 */
int get_packet_from_mailbox(int mailbox_id) {
    struct msqid_ds buf;

    return (msgctl(mailbox_id, IPC_STAT, &buf) == 0) && (buf.msg_qnum > 0);
}

/**
 * DONE: Receive a packet. (don't forget to test drop_packet)
 * If the packet is DATA, send an ACK packet and SIGIO to the sender.
 * If the packet is ACK, update the status of the packet.
 */
void receive_packet(int sig) {
	if (!drop_packet()) {
		packet_t *packet = (packet_t *) malloc(sizeof(packet_t));
		if (msgrcv(mailbox_id, packet, sizeof(packet_t), 0, 0) < 0) {
			printf("msgrcv failed\n");
			return;
		}
		if (packet->mtype == DATA) {
			process_t *process = (process_t *) malloc(sizeof(process_t));
			get_process_info(packet->process_name, process);
			int mqid = msgget((key_t)process->key, 0);
			handle_data(packet, process, mqid);
			//free(process);
		}
		else {
			handle_ACK(packet);
		}
		//free(packet);
	}
}

/**
 * DONE Initialize the message structure and wait for a message from another process.
 * Save the message content to the data and return 0 if success, -1 otherwise
 */
int receive_message(char *data) {
    
	message = (message_t *) malloc(sizeof(message_t));
    message->sender.pid = -1;
    message->num_packets_received = 0;
    message->is_complete = 0;
    
    while(!message->is_complete){
		pause();
    }
	
	strcpy(data, message->data);
	
	if (message) {
		if (message->is_received) {
			//free(message->is_received);
		}
		if (message->data) {
			//free(message->data);
		}
		//free(message);
	}
    
    return 0;
    
}
