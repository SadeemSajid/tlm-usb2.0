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

enum transmission_state { USB_TOKEN, USB_DATA, USB_NO_DATA };

enum control_state {
	USB_CTRL_NONE,
	USB_CTRL_SETUP,
	USB_CTRL_DATA,
	USB_CTRL_STATUS
};

#endif // COMMON_H