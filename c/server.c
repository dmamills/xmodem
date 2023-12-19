#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include "xmodem.h"

#define PORT "3000"


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

int get_connection_socket(int sockfd) {
    struct sockaddr_storage their_addr;
    socklen_t addr_size;
    int new_fd;
    addr_size = sizeof their_addr;
    new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);

    if(new_fd == -1) {
        printf("Error accepting connection\n");
        return -1;
    }

    return new_fd;
}

int setup_socket() {
    struct addrinfo hints, *res;
    int sockfd;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // use IPv4 or IPv6, whichever
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // fill in my IP for me

    getaddrinfo(NULL, PORT, &hints, &res);

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    bind(sockfd, res->ai_addr, res->ai_addrlen);
    listen(sockfd, 1);

    return sockfd;
}

int read_file_to_xmodem_file(const char *filename, xmodem_file_t *xmodem_file) {
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        perror("Error opening file");
        return 1;
    }

    fseek(file, 0, SEEK_END);
    xmodem_file->fsize = ftell(file);
    rewind(file);

    xmodem_file->buffer = malloc(xmodem_file->fsize);
    if (xmodem_file->buffer == NULL) {
        perror("Error allocating memory");
        fclose(file);
        return 1;
    }

    size_t bytesRead = fread(xmodem_file->buffer, 1, xmodem_file->fsize, file);
    if (bytesRead != xmodem_file->fsize) {
        perror("Error reading file");
        free(xmodem_file->buffer);
        fclose(file);
        return 1;
    }

    fclose(file);

    return 0;
}

int send_xmodem(int fd, xmodem_file_t* file) {
    // wait for initial NAK
     int got_nak = 0;
     while(!got_nak) {
        //read single byte from fd
        uint8_t buf[1];
        int bytes_read = recv(fd, buf, 1, 0);
        printf("Got bytes: %d char: %u\n", bytes_read, buf[0]);
        got_nak = (buf[0] == NAK);
    }

    int transfer_done = 0;
    int current_packet = 1;
    int current_offset = 0;
    int NAK_count = 0;

    while(!transfer_done) {
        /// read next 128 bytes from file
        uint8_t packet_data[BUFFER_SIZE];
        memcpy(packet_data, file->buffer + current_offset, BUFFER_SIZE);

        // calculate checksum
        uint8_t checksum = 0;
        for(int i = 0; i < BUFFER_SIZE-1; i++) {
            checksum += packet_data[i];
        }

        // hexdump(packet_data, BUFFER_SIZE);
        if(current_offset + BUFFER_SIZE >= file->fsize) {
            transfer_done = 1;
        }

        xmodem_packet_t packet;
        packet.header_value = transfer_done ? EOT : SOH;
        packet.packet_number = current_packet;
        packet.packet_number_complement = current_packet ^ 0xFF;
        memcpy(packet.data, packet_data, BUFFER_SIZE);
        packet.checksum = checksum;

        uint8_t sBuf[sizeof(packet)];
        memcpy(sBuf, &packet, sizeof(packet));
        send(fd, &sBuf, sizeof(sBuf), 0);

        // wait for ACK or NAK
        int got_response = 0;
        while(!got_response) {
            //read single byte from fd
            uint8_t buf[1];
            int bytes_read = recv(fd, buf, 1, 0);
            printf("Got bytes: %d char: %u\n", bytes_read, buf[0]);
            got_response = (buf[0] == ACK || buf[0] == NAK);
            if(buf[0] == ACK) {
                current_packet++;
                current_offset += BUFFER_SIZE;
            } else if(buf[0] == NAK) {
                NAK_count++;
                if(NAK_count > 10) {
                    printf("Too many NAKs Ending transfer\n");
                    transfer_done = 1;
                }
            }
        }

        if(packet.header_value == EOT) {
            transfer_done = 1;
        }
    }

    return 1;
}

int main(int argc, char *argv[]) {
    xmodem_file_t file;
    if(read_file_to_xmodem_file("test-586.bin", &file) != 0) {
        printf("Error reading file\n");
        return 1;
    }

    //hexdump(file.buffer, file.fsize);

    int sockfd = setup_socket();
    if(sockfd == -1) {
        printf("Error setting up socket\n");
        return 1;
    }

    int new_fd = get_connection_socket(sockfd);
    if(new_fd == -1) {
        printf("Error accepting connection\n");
        return 1;
    }

    send_xmodem(new_fd, &file);

	shutdown(new_fd, 2);
	shutdown(sockfd, 2);
    free(file.buffer);
    return 0;
}