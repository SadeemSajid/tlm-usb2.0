#ifndef DEVICE_H
#define DEVICE_H

#include <cstdint>

#include "packet.h"

/* Descriptor Table */
#define B_LENGTH 18
#define B_DESCRIPTOR_TYPE 0x01 // DEVICE
#define BCD_USB 0x0200	       // 2.0
#define B_DEVICE_CLASS 0x00
#define B_DEVICE_SUB_CLASS 0x00
#define B_DEVICE_PROTOCOL 0x00
#define B_MAX_PACKET_SIZE_0 MAX_PACKET_SIZE
#define I_MANAFACTURER 1 // 0 is reserved for language ID
#define I_PRODUCT 2

/* Configuration Descriptor */
#define CONFIG_B_LENGTH 9
#define CONFIG_B_DESCRIPTOR_TYPE 0x02

typedef struct __attribute__((packed)) {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint16_t bcdUSB;
	uint8_t bDeviceClass;
	uint8_t bDeviceSubClass;
	uint8_t bDeviceProtocol;
	uint8_t bMaxPacketSize0;
	uint16_t idVendor;
	uint16_t idProduct;
	uint16_t bcdDevice;
	uint8_t iManufacturer;
	uint8_t iProduct;
	uint8_t iSerialNumber;
	uint8_t bNumConfigurations;
} descriptor_table_t;

typedef struct __attribute__((packed)) {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint16_t wTotalLength;
	uint8_t bNumInterfaces;
	uint8_t bConfigurationValue;
	uint8_t iConfiguration;
	uint8_t bmAttributes;
	uint8_t bMaxPower;
} config_descriptor_t;

#endif // DEVICE_H