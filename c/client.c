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

int is_packet_valid(xmodem_packet_t* packet, int current_packet_number) {
    if(packet->header_value != SOH && packet->header_value != EOT) {
        printf("Invalid header\n");
        return 0;
    }

    if(packet->packet_number != current_packet_number) {
        printf("Invalid packet number\n");
        return 0;
    }

    if(packet->packet_number_complement != (current_packet_number ^ 0xFF)) {
        printf("Invalid packet compliment\n");
        return 0;
    }

    uint8_t checksum = 0;
    for(int i = 0; i < BUFFER_SIZE-1; i++) {
        checksum += packet->data[i];
    }

    if(checksum != packet->checksum) {
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
    //zero out buffer
    for(int i = 0; i < BUFFER_SIZE * 8; i++) {
        final_buffer[i] = 0;
    }
  
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
    uint8_t buffer[sizeof(xmodem_packet_t)];
    while(!transfer_complete) {
        int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);

        if(bytes_received == 0) {
            printf("Connection closed by peer\n");
            close(client_socket);
            exit(EXIT_SUCCESS);
        } else if (bytes_received == -1) {
            perror("Error receiving data");
            close(client_socket);
            exit(EXIT_FAILURE);
        }

        xmodem_packet_t received_packet;
        memcpy(&received_packet, buffer, sizeof(xmodem_packet_t));
        // hexdump(buffer, bytes_received);

        //respond to packet
        if(is_packet_valid(&received_packet, current_packet)) {
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


    //todo trim final buffer
    // int lastZeroIdx = (BUFFER_SIZE * 8)-1;
    // for(int i = (BUFFER_SIZE * 8)-1; i > -1; i--) {
    //     if(final_buffer[i] == 0) {
    //         lastZeroIdx = i;
    //     } else {
    //         break;
    //     }
    // }

    // printf("Last zero idx: %d\n", lastZeroIdx);
    // uint8_t final_buffer_trimmed[lastZeroIdx];
    // memcpy(final_buffer_trimmed, final_buffer, lastZeroIdx);

    // //write final buffer to file
    FILE *fp = fopen("test-586-copy.bin", "wb");
    if(fp == NULL) {
        perror("Error opening file");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    int bytes_written = fwrite(final_buffer, 1, BUFFER_SIZE*8, fp);
    fclose(fp);

    printf("Done recieving blocks, got %d blocks\n", current_packet);
    close(client_socket);

    return 0;
}