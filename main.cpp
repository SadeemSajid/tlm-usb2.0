#include "device.h"
#include "device_tb.h"
#include <systemc>

int sc_main(int argc, char *argv[])
{
	USB_Device_TB usb_dev_tb("usb_dev_tb");
	USB_Device usb_dev("usb_dev");

	usb_dev_tb.socket.bind(usb_dev.target);

	sc_core::sc_start();
	return 0;
}