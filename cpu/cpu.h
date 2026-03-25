#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

class CPU : public sc_core::sc_module
{
      public:
	tlm_utils::simple_initiator_socket<CPU> socket;

	SC_CTOR(CPU) { SC_THREAD(testbench_thread); }

	void testbench_thread();
};