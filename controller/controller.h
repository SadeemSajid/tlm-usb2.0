#include "common.h"
#include "packet.h"
#include <cstring>
#include <sysc/kernel/sc_event.h>
#include <systemc>
#include <tlm_core/tlm_2/tlm_generic_payload/tlm_gp.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

enum HC_REGISTERS {
	REG_USB_CMD = 0x00, // Bit 0: Run/Stop, Bit 1: Reset
	REG_USB_STS = 0x04, // Bit 0: Idle, Bit 1: Error, Bit 2:
			    // Transaction Complete , Bit 3: Busy,
			    // Bit 4: Stopped
	REG_PORT_SC = 0x08,   // Bit 0: Connect, Bit 1: Port Reset
	REG_ADDR_ENDP = 0x0C, // [6:0] Address, [10:7] Endpoint
	REG_DATA_PTR = 0x10,  // Pointer to System RAM (DMA Address)
	REG_TOKEN = 0x14      // Write here to trigger: 0=SETUP, 1=IN, 2=OUT
};

enum HC_CMD { HC_CMD_STOP = 0, HC_CMD_RUN = 1, HC_CMD_RESET = 0x1 << 1 };

enum HC_STATUS {
	HC_STS_STOP = 0x1 << 4,
	HC_STS_IDLE = 0,
	HC_STS_BUSY = 0x1 << 3,
	HC_STS_TR_COMP = 0x1 << 2,
	HC_STS_ERROR = 0x1 << 1
};

enum HC_STATE { HC_STOPPED, HC_RUNNING, HC_RESET, HC_OPERATION, HC_ERROR };

class Controller : public sc_core::sc_module
{
	/* Device Control */
	bool data_toggle;
	enum device_state dev_state;

	/* Internal Variables */
	enum HC_STATE state;

	/* Buffer (Read/Write) */
	uint8_t buffer[MAX_PACKET_SIZE];

	/* Registers */
	uint8_t registers[0x18];

	/* Events */
	sc_core::sc_event token_write_ev;

	// TODO: HUB functionality
	// TODO: HUB IN endpoint for reporting device attachment events to
	// the HOST
	// TODO: HOST periodic polling to learn new or removed devices

      public:
	/* TLM Sockets */
	tlm_utils::simple_initiator_socket<Controller> dev_out_sock;
	tlm_utils::simple_target_socket<Controller> cpu_in_sock;

	tlm_utils::simple_initiator_socket<Controller> dma_sock;

	SC_CTOR(Controller)
	    : dev_out_sock("dev_out_sock"), cpu_in_sock("cpu_in_sock"),
	      dma_sock("dma_sock")
	{
		// init internal state
		data_toggle = 0;
		dev_state = USB_DEFAULT;
		state = HC_STOPPED;

		// init registers
		std::memset(registers, 0, sizeof(registers));
		registers[REG_USB_STS] = HC_STS_STOP;

		// register transport
		cpu_in_sock.register_b_transport(this,
						 &Controller::b_transport);
		SC_THREAD(process_thread);
	}

      private:
	/* CPU Interface */
	void b_transport(tlm::tlm_generic_payload &trans,
			 sc_core::sc_time &delay);
	void register_write(uint8_t offset, uint32_t data);
	uint32_t register_read(uint8_t offset);
	void dma_read(uint32_t addr, uint8_t *dest, uint32_t len);
	void dma_write(uint32_t addr, uint8_t *src, uint32_t len);

	/* Device Interface */
	void execute_usb_transaction(uint8_t token_type);

	/* Low Level Helpers */
	tlm::tlm_response_status send_token(uint8_t type, uint8_t addr,
					    uint8_t endp);
	tlm::tlm_response_status send_data_packet(uint8_t pid_type,
						  uint8_t *src, uint32_t len);
	tlm::tlm_response_status
	receive_data_packet(uint8_t pid_type, uint8_t *dest, uint32_t &len);

	/* State Watcher */
	void process_thread();
};