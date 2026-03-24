#include "common.h"
#include <systemc>

class Controller : public sc_core::sc_module
{
	bool data_toggle;
	enum device_state dev_state;

      public:
	SC_CTOR(Controller)
	{
		data_toggle = 0;
		dev_state = USB_DEFAULT;
	}
};