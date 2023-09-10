#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>

#define MAX_FILENAME_LENGTH 256
#define MAX_BUFFER_SIZE 1024

void upload_file(int connfd, char file_name[MAX_FILENAME_LENGTH]) {
    // Open file
    FILE *file = fopen(file_name, "rb");
    if (file == NULL) {
        printf("Failed in opening file!\n");
        exit(1);
    }

    // Send file size to server
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (send(connfd, &file_size, sizeof(file_size), 0) == -1) {
        perror("Failed in sending file size!\n");
        fclose(file);
        exit(1);
    }

    // Read file to buffer and send with progress
    char buffer[MAX_BUFFER_SIZE];
    size_t bytes_read, bytes_sent, total_sent = 0;

    while ((bytes_read = fread(buffer, sizeof(char), sizeof(buffer), file)) > 0) {
        if ((bytes_sent = send(connfd, buffer, bytes_read, 0)) == -1) {
            perror("Failed in sending file!\n");
            fclose(file);
            exit(1);
        }

        // Print progress
        total_sent += bytes_sent;
        printf("Uploading... %.2f%%\r", (float)total_sent / file_size * 100);
        fflush(stdout);
    }

    fclose(file);
    printf("\nSuccessfully sending file to server!\n");
}

void download_file(int connfd, char file_name[MAX_FILENAME_LENGTH]) {
    // Receive file size from server
    long file_size;
    if (recv(connfd, &file_size, sizeof(file_size), 0) == -1) {
        perror("Failed in receiving file size!\n");
        exit(1);
    }

    // Open file
    FILE *file = fopen(file_name, "wb");
    if (file == NULL) {
        printf("Failed in opening file!\n");
        exit(1);
    }

    // Receive and write file data with progress
    char buffer[MAX_BUFFER_SIZE];
    size_t bytes_received, total_received = 0;

    while (total_received < file_size) {
        if ((bytes_received = recv(connfd, buffer, sizeof(buffer), 0)) <= 0) {
            perror("Failed in receiving file!\n");
            fclose(file);
            exit(1);
        }
        fwrite(buffer, 1, bytes_received, file);

        // Print progress
        total_received += bytes_received;
        printf("Downloading... %.2f%%\r", (float)total_received / file_size * 100);
        fflush(stdout);   
    }

    fclose(file);
    printf("\nSuccessfully downloading file from server!\n");
}

int main(int argc, char *argv[]) {
    int serv_port;

    if (argc == 2) {
        serv_port = 9500; // Default
    } else if (argc == 3) {
        serv_port = atoi(argv[2]); // Specify Port
    } else {
        fprintf(stderr, "Usage: %s <hostname/IP> [port](*optional)\n", argv[0]);
        exit(1);
    }

    struct sockaddr_in serv_addr;
    struct hostent *he;
    int connfd;

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(serv_port);

    // Check if argv[1] is an IP address
    struct in_addr ip_addr;
    if (inet_pton(AF_INET, argv[1], &ip_addr) == 1) {
        serv_addr.sin_addr = ip_addr;
    } else {
        if ((he = gethostbyname(argv[1])) == NULL) {
            perror("Failed in resolving address!\n");
            exit(1);
        }
        memcpy(&serv_addr.sin_addr.s_addr, he->h_addr, he->h_length);
    }

    // Create socket
    printf("Creating socket...\t");
    if ((connfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Error!\n");
        exit(1);
    }
    printf("Success!\nConnecting...\t\t");

    // Connect server
    if (connect(connfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        perror("Error!\n");
        exit(1);
    }
    printf("Success!\n");

    printf("**********************************\n");
    printf("*    1.Upload file to server.    *\n");
    printf("*  2.Download file from server.  *\n");
    printf("*            3.exit.             *\n");
    printf("**********************************\n");

    int choice;
    while (1) {
        printf("Choose: ");
        scanf("%d", &choice);
        
        if (choice != 1 && choice != 2 && choice != 3) {
            printf("Please input correct choice!\n");
        } else {
            // Send choice to server
            if (send(connfd, &choice, sizeof(choice), 0) == -1) {
                perror("Failed in sending choice!\n");
                exit(1);
            }

            // Wait server for receiving choice
            int ack_1;
            if (recv(connfd, &ack_1, sizeof(ack_1), 0) == -1) {
                perror("Failed in sending choice!\n");
                exit(1);
            }

            if (choice == 3) {
                close(connfd);
                break;
            }

            char file_name[MAX_FILENAME_LENGTH];
            printf("Input file name: ");
            scanf("%s", file_name);

            // Send file name to server
            if (send(connfd, file_name, strlen(file_name), 0) == -1) {
                perror("Failed in sending file name!\n");
                exit(1);
            }

            // Wait server for receiving file name
            int ack_2;
            if (recv(connfd, &ack_2, sizeof(ack_2), 0) == -1) {
                perror("Failed in sending file name!\n");
                exit(1);
            }

            if (choice == 1) {
                upload_file(connfd, file_name);
            } else if (choice == 2) {
                download_file(connfd, file_name);
            }
        }
    }

    return 0;
}
