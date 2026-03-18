#ifndef PACKET_H
#define PACKET_H

#include <cstdint>

#define MAX_PACKET_SIZE 32

typedef struct __attribute__((packed)) {
	uint8_t type : 4;
	uint8_t check : 4; // one's complement of type
} pid_t;

typedef struct __attribute__((packed)) {
	pid_t pid;
	uint8_t address : 7;
	uint8_t endp : 4; // endpoint address
	uint8_t crc : 5;  // covers the address and the endp only
} token_t;

typedef struct __attribute__((packed)) {
	pid_t pid;
	uint8_t *data;
	uint32_t length; // deviation from the spec for simplicity
	uint16_t crc : 16;
} data_t;

typedef struct __attribute__((packed)) {
	pid_t pid;
} handshake_t;

enum token_type { TOKEN_IN, TOKEN_OUT, TOKEN_SETUP, TOKEN_SOF };

enum handshake_type { ACK, NAK, STALL, NYET };

#endif // PACKET_H