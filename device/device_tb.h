#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

class USB_Device_TB : public sc_core::sc_module
{
      public:
	tlm_utils::simple_initiator_socket<USB_Device_TB> socket;

	SC_CTOR(USB_Device_TB) : socket("socket") { SC_THREAD(run_test); }

      private:
	void run_test();

	// Helper to send a raw TLM transaction
	void transport_packet(uint8_t *buf, size_t len);

	void send_setup_stage(uint8_t addr, uint8_t ep, uint8_t req,
			      uint16_t value, uint16_t length);

	void send_data_stage_in(uint8_t addr, uint8_t ep,
				uint16_t expected_len);
	void send_status_stage_out(uint8_t addr, uint8_t ep);

	void send_status_stage_in(uint8_t addr, uint8_t ep);
};