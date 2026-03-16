#include <cstdint>

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