#ifndef DEVICE_H
#define DEVICE_H

#include <cstdint>
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include "common.h"
#include "packet.h"

/* Device Descriptor */
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
#define CONFIG_B_NUM_INTERFACES 0x01
#define CONFIG_B_TOTAL_LENGTH 18
#define CONFIG_B_CONFIGURATION_VALUE 0x01
#define CONFIG_B_M_ATTRIBUTES ((1 << 7) | (1 << 6) | (0 << 5))

/* Interface Descriptor */
#define INTERFACE_B_LENGTH 9
#define INTERFACE_B_DESCRIPTOR_TYPE 0x04 // INTERFACE type
#define INTERFACE_B_INTERFACE_NUMBER 0x0
#define INTERFACE_B_NUM_ENDPOINTS 0x0 // Only need EP0 for now
#define INTERFACE_B_ALTERNATE_SETTING 0x0

/* LangID Descriptor */
#define LANGID_B_LENGTH 4
#define LANGID_B_DESCRIPTOR_TYPE 0x03 // String type
#define LANGID_B_LANG_ID 0x0409	      // English - United States

/* String Descriptors */
#define STRING_VEN_B_LENGTH 10
#define STRING_VEN_B_DESCRIPTOR_TYPE 0x03 // String type
#define STRING_VEN_B_LANG_ID "DEAD"	  // UTF-16

#define STRING_PROD_B_LENGTH 10
#define STRING_PROD_B_DESCRIPTOR_TYPE 0x03 // String type
#define STRING_PROD_B_LANG_ID "BEEF"	   // UTF-16

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
} device_descriptor_t;

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

typedef struct __attribute__((packed)) {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bInterfaceNumber;
	uint8_t bAlternateSetting;
	uint8_t bNumEndpoints;
	uint8_t bInterfaceClass;
	uint8_t bInterfaceSubclass;
	uint8_t bInterfaceProtocol;
	uint8_t iInterface;
} interface_descriptor_t;

typedef struct __attribute__((packed)) {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint16_t wLANGID;
} langid_descriptor_t;

typedef struct __attribute__((packed)) {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint16_t bString[20];
} string_descriptor_t;

/* USB Device Module */
class USB_Device : public sc_core::sc_module
{
	/* Descriptors */
	device_descriptor_t dev_desc;
	config_descriptor_t conf_desc;
	interface_descriptor_t int_desc;

	/* String Descriptors*/
	langid_descriptor_t lang_desc;
	string_descriptor_t vendor_desc;
	string_descriptor_t product_desc;

	/* Device USB state */
	enum device_state state;

      public:
	/* TLM Sockets */
	tlm_utils::simple_target_socket<USB_Device> target;

	SC_CTOR(USB_Device)
	{
		dev_desc.bLength = B_LENGTH;
		dev_desc.bDescriptorType = B_DESCRIPTOR_TYPE;
		dev_desc.bcdUSB = BCD_USB;
		dev_desc.bDeviceClass = B_DEVICE_CLASS;
		dev_desc.bDeviceSubClass = B_DEVICE_SUB_CLASS;
		dev_desc.bDeviceProtocol = B_DEVICE_PROTOCOL;
		dev_desc.bMaxPacketSize0 = B_MAX_PACKET_SIZE_0;
		dev_desc.idVendor = 0;
		dev_desc.idProduct = 0;
		dev_desc.bcdDevice = 0;
		dev_desc.iManufacturer = I_MANAFACTURER;
		dev_desc.iProduct = I_PRODUCT;
		dev_desc.iSerialNumber = 0;
		dev_desc.bNumConfigurations = 1;

		conf_desc.bLength = CONFIG_B_LENGTH;
		conf_desc.bDescriptorType = CONFIG_B_DESCRIPTOR_TYPE;
		conf_desc.wTotalLength = CONFIG_B_TOTAL_LENGTH;
		conf_desc.bNumInterfaces = CONFIG_B_NUM_INTERFACES;
		conf_desc.bConfigurationValue = CONFIG_B_CONFIGURATION_VALUE;
		conf_desc.iConfiguration = 0;
		conf_desc.bmAttributes = CONFIG_B_M_ATTRIBUTES;
		conf_desc.bMaxPower = 0;

		int_desc.bLength = INTERFACE_B_LENGTH;
		int_desc.bDescriptorType = INTERFACE_B_DESCRIPTOR_TYPE;
		int_desc.bInterfaceNumber = INTERFACE_B_INTERFACE_NUMBER;
		int_desc.bAlternateSetting = INTERFACE_B_ALTERNATE_SETTING;
		int_desc.bNumEndpoints = INTERFACE_B_NUM_ENDPOINTS;
		int_desc.bInterfaceClass = 0;
		int_desc.bInterfaceSubclass = 0;
		int_desc.bInterfaceProtocol = 0;
		int_desc.iInterface = 0;

		lang_desc.bLength = LANGID_B_LENGTH;
		lang_desc.bDescriptorType = LANGID_B_DESCRIPTOR_TYPE;
		lang_desc.wLANGID = LANGID_B_LANG_ID;

		vendor_desc.bLength = STRING_VEN_B_LENGTH;
		vendor_desc.bDescriptorType = STRING_VEN_B_DESCRIPTOR_TYPE;
		const char *vendor_str = STRING_VEN_B_LANG_ID;
		for (int i = 0; i < 4; i++) {
			vendor_desc.bString[i] = vendor_str[i];
		}

		product_desc.bLength = STRING_PROD_B_LENGTH;
		product_desc.bDescriptorType = STRING_PROD_B_DESCRIPTOR_TYPE;
		const char *product_str = STRING_PROD_B_LANG_ID;
		for (int i = 0; i < 4; i++) {
			product_desc.bString[i] = product_str[i];
		}

		/* We will assume device is reset */
		state = USB_DEFAULT;

		/* Thread Registration */
		target.register_b_transport(this, &USB_Device::b_transport);
	}

	void b_transport(tlm::tlm_generic_payload &trans,
			 sc_core::sc_time &delay);
};

#endif // DEVICE_H