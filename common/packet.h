#ifndef PACKET_H
#define PACKET_H

#include <cstdint>

#define MAX_PACKET_SIZE 32

enum pid_token {
	PID_TOKEN_INIT = 0,
	PID_TOKEN_OUT = 0x1,
	PID_TOKEN_IN = 0x9,
	PID_TOKEN_SOF = 0x5,
	PID_TOKEN_SETUP = 0xd
};

enum pid_data {
	PID_DATA_DATA0 = 0x3,
	PID_DATA_DATA1 = 0xb,
	PID_DATA_DATA2 = 0x7,
	PID_DATA_MDATA = 0xf
};

enum pid_handshake {
	PID_HANDSHAKE_ACK = 0x2,
	PID_HANDSHAKE_NAK = 0xa,
	PID_HANDSHAKE_STALL = 0xe,
	PID_HANDSHAKE_NYET = 0x6
};

enum request { REQ_GET_DESCRIPTOR = 0x06 };

enum descriptor_type { DEVICE = 0x1, CONFIGURATION, STRING, INTERFACE };

/* STANDARD PACKETS */
typedef struct __attribute__((packed)) {
	uint8_t type : 4;
	uint8_t check : 4; // one's complement of type
} packet_pid_t;

typedef struct __attribute__((packed)) {
	packet_pid_t pid;
	uint8_t address : 7;
	uint8_t endp : 4; // endpoint address
	uint8_t crc : 5;  // covers the address and the endp only
} token_t;

typedef struct __attribute__((packed)) {
	packet_pid_t pid;
	uint8_t *data;
	uint32_t length; // deviation from the spec for simplicity
	uint16_t crc : 16;
} data_t;

typedef struct __attribute__((packed)) {
	packet_pid_t pid;
} handshake_t;

/********************/

typedef struct __attribute__((packed)) {
	uint8_t bmRequestType; // D7: Direction, D6..5: Type, D4..0: Recipient
	uint8_t bRequest; // The actual command (e.g., 0x06 for GET_DESCRIPTOR)
	uint16_t wValue;  // Varies by request (Descriptor Type/Index)
	uint16_t wIndex;  // Varies by request (Language ID)
	uint16_t wLength; // Number of bytes to transfer in the Data stage
} setup_request_t;

enum token_type { TOKEN_IN, TOKEN_OUT, TOKEN_SETUP, TOKEN_SOF };

enum handshake_type { ACK, NAK, STALL, NYET };

#endif // PACKET_H