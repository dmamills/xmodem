#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "xmodem.h"

#define SERVER_IP "127.0.0.1"  // Replace with your server's IP address
#define SERVER_PORT 3000

int setup_socket() {
    int sockfd;
    struct sockaddr_in server_addr;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Error converting IP address");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error connecting to server");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    return sockfd;
}

void hexdump(const void *data, size_t size) {
    const unsigned char *p = (const unsigned char *)data;
    for (size_t i = 0; i < size; ++i) {
        printf("%02X ", p[i]);
        if ((i + 1) % 16 == 0 || i == size - 1) {
            for (size_t j = 0; j < 15 - (i % 16); ++j) {
                printf("   "); // Add extra spaces for alignment
            }
            printf("| ");
            for (size_t j = i - i % 16; j <= i; ++j) {
                printf("%c", (p[j] >= 32 && p[j] <= 126) ? p[j] : '.');
            }
            printf("\n");
        }
        else if ((i + 1) % 8 == 0) {
            printf(" ");
        }
    }
}

int is_packet_valid(uint8_t* buffer, int current_packet_number) {
    if(buffer[0] != SOH && buffer[0] != EOT) {
        printf("Invalid header\n");
        return 0;
    }

    if(buffer[1] != current_packet_number) {
        printf("Invalid packet number\n");
        return 0;
    }

    if(buffer[2] != (current_packet_number ^ 0xFF)) {
        printf("Invalid packet compliment\n");
        return 0;
    }

    uint8_t checksum = 0;
    for(int i = 0; i < BUFFER_SIZE-1; i++) {
        checksum += buffer[i+3];
    }

    uint8_t sentChecksum = buffer[130];
    if(checksum != sentChecksum) {
        printf("Invalid checksum\n");
        return 0;
    }

    return 1;
}


int sent_byte(int fd, uint8_t byte) {
    uint8_t buf[1];
    buf[0] = byte;
    int bytes_sent = send(fd, buf, 1, 0);
    if (bytes_sent != 1) {
        perror("Error sending data");
        close(fd);
        exit(EXIT_FAILURE);
    }
    return bytes_sent;
}

int main() {
    uint8_t final_buffer[BUFFER_SIZE * 8];
  
    int client_socket = setup_socket();
    if(client_socket == -1) {
        printf("Error setting up socket\n");
        exit(EXIT_FAILURE);
    }
   
    printf("Connected to server\n");

    //send inital nak
    char buf[1];
    buf[0] = NAK;

    int bytes_sent = send(client_socket, buf, 1, 0);
    if (bytes_sent != 1) {
        perror("Error sending data");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    int current_packet = 1;
    int current_offset = 0;
    int transfer_complete = 0;
    int did_error = 0;
    uint8_t buffer[PACKET_SIZE];
    while(!transfer_complete) {
        int bytes_received = recv(client_socket, buffer, PACKET_SIZE, 0);

        if(bytes_received == 0) {
            printf("Connection closed by peer\n");
            close(client_socket);
            exit(EXIT_SUCCESS);
        } else if (bytes_received == -1) {
            perror("Error receiving data");
            close(client_socket);
            exit(EXIT_FAILURE);
        }

        hexdump(buffer, bytes_received);

        //respond to packet
        if(is_packet_valid(buffer, current_packet)) {
           printf("Packet is valid! Send dat ACK\n");
           memcpy(final_buffer + current_offset, buffer + 3, BUFFER_SIZE);
           current_packet++;
           current_offset += BUFFER_SIZE;
           sent_byte(client_socket, ACK);
        } else {
            sent_byte(client_socket, NAK);
        }

        if(buffer[0] == EOT) {
            transfer_complete = 1;
        }

        //write data buffer to file
        memcpy(final_buffer + current_offset, buffer + 3, BUFFER_SIZE);
    }

    printf("Done recieving blocks, got %d blocks\n", current_packet);
    close(client_socket);

    return 0;
}