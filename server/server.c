#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

#define BACKLOG	1
#define MAX_FILENAME_LENGTH 256
#define MAX_BUFFER_SIZE 1024

void client_info(struct sockaddr_in cli_addr) {
    printf("Connection with client(%s:%d): ", inet_ntoa(cli_addr.sin_addr), cli_addr.sin_port);
}

void recv_file(int connfd, char file_name[MAX_FILENAME_LENGTH], struct sockaddr_in cli_addr) {
    // Receive file size from client
    long file_size;
    if (recv(connfd, &file_size, sizeof(file_size), 0) == -1) {
        client_info(cli_addr);
        perror("Failed in receiving file size!\n");
        exit(1);
    }

    // Open file
    FILE *file = fopen(file_name, "wb");
    if (file == NULL) {
        client_info(cli_addr);
        printf("Failed in opening file!\n");
        exit(1);
    }

    // Receive bytes to buffer and write buffer to file
    char buffer[MAX_BUFFER_SIZE];
    size_t bytes_received, total_received = 0;

    while (total_received < file_size) {
        size_t remaining = file_size - total_received;
        size_t to_receive = (remaining < sizeof(buffer)) ? remaining : sizeof(buffer);

        if ((bytes_received = recv(connfd, buffer, sizeof(to_receive), 0)) <= 0) {
            client_info(cli_addr);
            perror("Failed in receiving file!\n");
            fclose(file);
            exit(1);
        }
        fwrite(buffer, 1, bytes_received, file);

        // Print progress
        total_received += bytes_received;
        client_info(cli_addr);
        printf("Downloading... %.2f%%\r", (float)total_received / file_size * 100);
        fflush(stdout);
    }

    fclose(file);
    client_info(cli_addr);
    printf("\nSuccessfully downloading file from client!\n");
}

void send_file(int connfd, char file_name[MAX_FILENAME_LENGTH], struct sockaddr_in cli_addr) {
    // Check if the requested file exists
    FILE *file = fopen(file_name, "rb");
    if (file == NULL) {
        client_info(cli_addr);
        perror("Failed in opening file!\n");
        exit(1);
    }

    // Send file size to client
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (send(connfd, &file_size, sizeof(file_size), 0) == -1) {
        client_info(cli_addr);
        perror("Failed in sending file size!\n");
        exit(1);
    }

    // Read file to buffer
    char buffer[MAX_BUFFER_SIZE];
    size_t bytes_read, bytes_sent, total_sent = 0;

    while ((bytes_read = fread(buffer, sizeof(char), sizeof(buffer), file)) > 0) {
        if ((bytes_sent = send(connfd, buffer, bytes_read, 0)) == -1) {
            client_info(cli_addr);
            perror("Failed in sending file!\n");
            fclose(file);
            exit(1);
        }

        // Print progress
        total_sent += bytes_sent;
        client_info(cli_addr);
        printf("Sending... %.2f%%\r", (float)total_sent / file_size * 100);
        fflush(stdout);
    }

    fclose(file);
    client_info(cli_addr);
    printf("Successfully sending file to client!\n");
}

void handle_client(int connfd, struct sockaddr_in cli_addr) {
    int choice = 0;
    char file_name[MAX_FILENAME_LENGTH];
    size_t choice_bytes;
    size_t file_name_bytes;

    while (1) {
        // Receive choice from client
        if ((choice_bytes = recv(connfd, &choice, sizeof(choice), 0)) <= 0) {
            client_info(cli_addr);
            printf("Failed in receiving choice!\n");
            exit(1);
        }
        client_info(cli_addr);
        printf("Choice sent by client: %d\n", choice);

        // Tell client choice has been received
        int ack_1 = 1;
        if (send(connfd, &ack_1, sizeof(ack_1), 0) == -1) {
            perror("Failed in receiving choice!\n");
            exit(1);
        }

        if (choice == 3) {
            close(connfd);
            client_info(cli_addr);
            printf("Connection closed.\n");
            break;
        }

        // Receive file name from client
        if ((file_name_bytes = recv(connfd, file_name, sizeof(file_name), 0)) <= 0) {
            client_info(cli_addr);
            printf("Failed in receiving file name!\n");
            exit(1);
        }
        file_name[file_name_bytes] = '\0'; // end at the end of the file name
        client_info(cli_addr);
        printf("Received file name: %s\n", file_name);

        // Tell client file name has been received
        int ack_2 = 1;
        if (send(connfd, &ack_2, sizeof(ack_2), 0) == -1) {
            perror("Failed in receiving file name!\n");
            exit(1);
        }

        if (choice == 1) {
            recv_file(connfd, file_name, cli_addr);
        } else if (choice == 2) {
            send_file(connfd, file_name, cli_addr);
        } else {
            client_info(cli_addr);
            printf("Wrong request from client! Expected: 1 or 2, Received: %d\n", choice);
        }
    }
}

int main(int argc, char *argv[]) {
    int serv_port;

    if (argc == 2) {
        serv_port = atoi(argv[1]);
    } else {
        fprintf(stderr, "Usage: %s [port]\n", argv[0]);
        exit(1);
    }

    pid_t pid;
    struct sockaddr_in serv_addr, cli_addr;
    int listenfd, connfd;
    socklen_t len = sizeof(cli_addr);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(serv_port);

    // Create socket
    printf("Creating socket...\t");
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Error!\n");
		exit(1);
    }
    int optval = 1;
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        perror("Error!\n");
        exit(1);
    }
    printf("Success!\nBinding...\t\t");

    // Bind
    if (bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        perror("Error!\n");
		exit(1);
    };
    printf("Success!\nListening...\t\t\n");

    // Listen
    if (listen(listenfd, BACKLOG) == -1) {
        perror("Error!\n");
		exit(1);
    }

    while(1) {
        // Accept connection
        if ((connfd = accept(listenfd, (struct sockaddr *)&cli_addr, &len)) == -1) {
            perror("\nFailed in receiving connection!\n");
		    exit(1);
        }
        printf("Receive connection from client(%s:%d)\n", inet_ntoa(cli_addr.sin_addr), cli_addr.sin_port);

        if ((pid = fork()) > 0) {
            close(connfd);
            continue;
        } else if (pid == 0) {
            close(listenfd);
            handle_client(connfd, cli_addr);
            exit(1);
        } else {
            printf("Failed in forking!\n");
            exit(1);
        }
    }

    close(listenfd);

    return 0;
}
