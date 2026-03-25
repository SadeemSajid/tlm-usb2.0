#ifndef COMMON_H
#define COMMON_H

enum device_state {
	USB_ATTACHED,
	USB_POWERED,
	USB_DEFAULT,
	USB_ADDRESS,
	USB_CONFIGURED,
	USB_SUSPENDED
};

enum transmission_state { USB_TOKEN, USB_DATA };

#endif // COMMON_H