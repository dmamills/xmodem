#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1"  // Replace with your server's IP address
#define SERVER_PORT 3000

#define SOH 0x01 // start of header
#define EOT 0x04// end of trans
#define ACK 0x06 // ack
#define NAK 0x15 // ! ack
#define CAN 0x18 // cancel

#define BUFFER_SIZE 128
#define PACKET_SIZE BUFFER_SIZE+3

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
int main() {
    int client_socket;
    struct sockaddr_in server_addr;

    // Create socket
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Initialize server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);

    // Convert IP address from presentation to network format
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Error converting IP address");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error connecting to server");
        close(client_socket);
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

        //Get packet
        //validate packet
        //respond to packet

        //write data buffer to file
    }

    close(client_socket);

    return 0;
}