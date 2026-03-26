#include "common.h"
#include <systemc>

#define DEVICE_ADDRESS                                                         \
	0x1 // The default address to assign to the USB device connected to this
	    // host controller

class Controller : public sc_core::sc_module
{
	bool data_toggle;
	enum device_state dev_state;

	/* Buffer (Read/Write) */
	uint8_t *buffer;

	// TODO: HUB IN endpoint for reporting device attachment events to the
	// HOST
	// TODO: HOST periodic polling to learn new or removed devices

      public:
	SC_CTOR(Controller)
	{
		data_toggle = 0;
		dev_state = USB_DEFAULT;
	}
};