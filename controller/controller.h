#include <systemc>

class Controller : public sc_core::sc_module
{
	bool data_toggle;

      public:
	SC_CTOR(Controller) { data_toggle = 0; }
};