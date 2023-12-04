#define SOH 0x01 // start of header
#define EOT 0x04// end of trans
#define ACK 0x06 // ack
#define NAK 0x15 // ! ack
#define CAN 0x18 // cancel
#define BUFFER_SIZE 128
#define PACKET_SIZE 131

typedef struct {
    uint8_t* buffer;
    size_t fsize;
} xmodem_file_t;

typedef struct  {
    uint8_t header_value;
    uint8_t packet_number;
    uint8_t packet_number_complement;
    uint8_t data[BUFFER_SIZE];
    uint8_t checksum;
} xmodem_packet_t;