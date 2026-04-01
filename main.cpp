#include "controller.h"
#include "cpu.h"
#include "device.h"
#include "device_tb.h"
#include <systemc>

int sc_main(int argc, char *argv[])
{
	USB_Device usb_dev("usb_dev");
	// USB_Device_TB usb_dev_tb("usb_dev_tb");

	/* USB Device Testing */
	// usb_dev.target.bind(usb_dev_tb.socket);

	Controller controller("controller");

	CPU cpu("cpu");

	/* Bindings */
	usb_dev.target.bind(controller.dev_out_sock);
	controller.cpu_in_sock.bind(cpu.socket);
	cpu.dma.bind(controller.dma_sock);

	sc_core::sc_start();
	return 0;
}