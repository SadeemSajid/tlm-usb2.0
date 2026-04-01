#include <cstdint>
#include <cstring>
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

class CPU : public sc_core::sc_module
{
	uint8_t ram[64];

      public:
	tlm_utils::simple_initiator_socket<CPU> socket;
	tlm_utils::simple_target_socket<CPU> dma;

	SC_CTOR(CPU) : socket("socket"), dma("dma")
	{
		memset(ram, 0, sizeof(ram));

		dma.register_b_transport(this, &CPU::dma_b_transport);

		SC_THREAD(firmware_thread);
	}

      private:
	void firmware_thread();
	void poll_status(uint32_t &);

	/* Control Requests */
	void get_descriptor(uint8_t, uint8_t);
	void set_address(uint32_t old_addr, uint32_t new_addr, uint32_t endp);

	/* DMA */
	void dma_b_transport(tlm::tlm_generic_payload &trans,
			     sc_core::sc_time &delay);

	/* Low-level transactors */
	void send_token(uint8_t type, uint8_t addr, uint8_t endp,
			bool data_toggle, uint8_t req, uint16_t value,
			uint16_t length);
};