#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "process.h"

int main(int argc, char** argv) {
    if (argc < 7) {
        printf("args: process_name, key, window_size, max_delay, timeout, rate\n");
        return -1;
    }

    // initialize basic information and write it to a file
    if (init(argv[1], atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), atoi(argv[5]), atoi(argv[6])) < 0) {
        printf("Failed in init.\n");
        return -1;
    }

    char role[MAX_NAME_LEN];
    char receiver[MAX_NAME_LEN];
    char data[MAX_SIZE];

    while(1) {
        printf("\nRole (sender/receiver): ");
        scanf("%s", role);

        if (strcmp(role, "sender") == 0) {
            // get the receiver name and the data
            printf("\nReceiver name: ");
            scanf("%s", receiver);
            printf("Data: ");
            scanf("%s", data);
            if (send_message(receiver, data) < 0) {
                printf("Failed sending data to %s\n", receiver);
            }
        } else if (strcmp(role, "receiver") == 0) {
            if (receive_message(data) >= 0) {
                printf("Message: %s\n", data);
            }
        } else {
            printf("Invalid role: %s\n", role);
        }
    }
    return 0;
}
