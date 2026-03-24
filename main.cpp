#include "device.h"
#include <systemc>

int sc_main(int argc, char *argv[])
{

	USB_Device usb_dev("usb_dev");

	sc_core::sc_start();
	return 0;
}