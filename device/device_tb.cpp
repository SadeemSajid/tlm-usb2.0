#include "device_tb.h"
#include "packet.h"
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

void USB_Device_TB::run_test()
{
	sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

	// --- STEP 1: Initial GET_DESCRIPTOR (Address 0) ---
	std::cout << "\n--- Part 1: GET_DESCRIPTOR on Address 0 ---"
		  << std::endl;
	send_setup_stage(0, 0, REQ_GET_DESCRIPTOR, (DEVICE << 8), 18);
	send_data_stage_in(0, 0, 18);
	send_status_stage_out(0, 0);

	// --- STEP 2: SET_ADDRESS to 2 (Address 0) ---
	// Note: The device still responds to Address 0 during this entire
	// transfer.
	std::cout << "\n--- Part 2: SET_ADDRESS to 2 ---" << std::endl;
	send_setup_stage(0, 0, REQ_SET_ADDRESS, 2, 0);

	// Status Stage for SET_ADDRESS is an IN transaction (Device says "ACK")
	send_status_stage_in(0, 0);

	// --- STEP 3: GET_DESCRIPTOR (Address 2) ---
	// Now we test if the device actually migrated its address.
	std::cout << "\n--- Part 3: GET_DESCRIPTOR on New Address 2 ---"
		  << std::endl;
	send_setup_stage(2, 0, REQ_GET_DESCRIPTOR, (DEVICE << 8), 18);
	send_data_stage_in(2, 0, 18);
	send_status_stage_out(2, 0);

	// --- PART 4: GET CONFIGURATION DESCRIPTOR (ADDR 2) ---
	std::cout << "\n--- Part 4: GET_DESCRIPTOR (Config) on New Addr 2 ---"
		  << std::endl;
	// We'll ask for the Config Descriptor (type 0x02).
	// Usually host asks for 9 bytes first, then full length. We'll ask for
	// 25 here.
	send_setup_stage(2, 0, REQ_GET_DESCRIPTOR, (CONFIGURATION << 8), 25);
	send_data_stage_in(2, 0, 25);
	send_status_stage_out(2, 0);

	// --- PART 5: GET STRING DESCRIPTORS (Addr 2) ---
	// String 0: The Language ID list (usually 4 bytes)
	std::cout << "\n--- [USB_Device_TB] 5a. GET_DESCRIPTOR (String 0 - "
		     "LANGID) ---"
		  << std::endl;
	send_setup_stage(2, 0, REQ_GET_DESCRIPTOR, (STRING << 8) | 0, 255);
	send_data_stage_in(2, 0, 4);
	send_status_stage_out(2, 0);

	// String 1: Manufacturer String ("VNDR")
	std::cout << "\n--- [USB_Device_TB] 5b. GET_DESCRIPTOR (String 1 - "
		     "Manufacturer) ---"
		  << std::endl;
	send_setup_stage(2, 0, REQ_GET_DESCRIPTOR, (STRING << 8) | 1, 255);
	send_data_stage_in(2, 0, 255);
	send_status_stage_out(2, 0);

	// String 2: Product String ("DEADBEEF")
	std::cout << "\n--- [USB_Device_TB] 5c. GET_DESCRIPTOR (String 2 - "
		     "Product) ---"
		  << std::endl;
	send_setup_stage(2, 0, REQ_GET_DESCRIPTOR, (STRING << 8) | 2, 255);
	send_data_stage_in(2, 0, 255);
	send_status_stage_out(2, 0);

	std::cout << "\n--- All Enumeration Tests Complete ---" << std::endl;
}

// Helper to send a raw TLM transaction
void USB_Device_TB::transport_packet(uint8_t *buf, size_t len)
{
	tlm::tlm_generic_payload trans;
	sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

	trans.set_command(tlm::TLM_WRITE_COMMAND);
	trans.set_data_ptr(buf);
	trans.set_data_length(len);
	trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

	socket->b_transport(trans, delay);

	if (trans.is_response_error()) {
		std::cerr << "[USB_Device_TB] TLM Transaction Error!"
			  << std::endl;
	}
}

void USB_Device_TB::send_setup_stage(uint8_t addr, uint8_t ep, uint8_t req,
				     uint16_t value, uint16_t length)
{
	std::cout << "[USB_Device_TB] Initiating SETUP Stage..." << std::endl;

	// A. Token Packet
	token_t token_pkt;
	token_pkt.pid.type = PID_TOKEN_SETUP;
	token_pkt.pid.check = (uint8_t)(~PID_TOKEN_SETUP & 0x0F);
	token_pkt.address = addr;
	token_pkt.endp = ep;
	transport_packet((uint8_t *)&token_pkt, sizeof(token_t));

	// B. Data Packet (8-byte Request)
	// Note: We use a raw buffer to avoid the pointer-issue in your
	// data_t struct
	uint8_t data_buf[sizeof(packet_pid_t) + 8];
	packet_pid_t *pid = (packet_pid_t *)data_buf;
	pid->type = PID_DATA_DATA0;
	pid->check = (uint8_t)(~PID_DATA_DATA0 & 0x0F);

	setup_request_t *s_req =
	    (setup_request_t *)(data_buf + sizeof(packet_pid_t));
	s_req->bmRequestType = 0x80; // Device-to-USB_Device_TB
	s_req->bRequest = req;
	s_req->wValue = value;
	s_req->wIndex = 0;
	s_req->wLength = length;

	transport_packet(data_buf, sizeof(data_buf));
}

void USB_Device_TB::send_data_stage_in(uint8_t addr, uint8_t ep,
				       uint16_t expected_len)
{
	token_t token_pkt;
	token_pkt.pid.type = PID_TOKEN_IN;
	token_pkt.pid.check = (uint8_t)(~PID_TOKEN_IN & 0x0F);
	token_pkt.address = addr;
	token_pkt.endp = ep;
	transport_packet((uint8_t *)&token_pkt, sizeof(token_t));

	uint8_t rx_buf[MAX_PACKET_SIZE];
	transport_packet(rx_buf, MAX_PACKET_SIZE);

	// Calculate actual payload length (TLM length - 1 byte PID)
	// We assume your transport_packet updates the trans object length
	size_t payload_len =
	    18; // For simplicity, or use a return value from transport_packet

	std::cout << "[USB_Device_TB] Raw Data: ";
	for (size_t i = 0; i < 16; i++) {
		printf("%02X ", rx_buf[i + 1]);
	}
	std::cout << std::endl;
}

void USB_Device_TB::send_status_stage_out(uint8_t addr, uint8_t ep)
{
	std::cout << "[USB_Device_TB] Initiating STATUS Stage (OUT)..."
		  << std::endl;

	// A. Token Packet
	token_t token_pkt;
	token_pkt.pid.type = PID_TOKEN_OUT;
	token_pkt.pid.check = (uint8_t)(~PID_TOKEN_OUT & 0x0F);
	token_pkt.address = addr;
	token_pkt.endp = ep;
	transport_packet((uint8_t *)&token_pkt, sizeof(token_t));

	// B. Data Packet (ZLP - Zero Length Packet)
	uint8_t zlp[sizeof(packet_pid_t)];
	packet_pid_t *pid = (packet_pid_t *)zlp;
	pid->type = PID_DATA_DATA1;
	pid->check = (uint8_t)(~PID_DATA_DATA1 & 0x0F);

	transport_packet(zlp, sizeof(zlp));
}

void USB_Device_TB::send_status_stage_in(uint8_t addr, uint8_t ep)
{
	std::cout << "[USB_Device_TB] Initiating STATUS Stage (IN)..."
		  << std::endl;

	// A. Token Packet (Host asks Device for Status)
	token_t token_pkt;
	token_pkt.pid.type = PID_TOKEN_IN;
	token_pkt.pid.check = (uint8_t)(~PID_TOKEN_IN & 0x0F);
	token_pkt.address = addr;
	token_pkt.endp = ep;
	transport_packet((uint8_t *)&token_pkt, sizeof(token_t));

	// B. Data Packet (Host provides a buffer, Device should fill with ZLP
	// DATA1)
	uint8_t rx_buf[MAX_PACKET_SIZE];
	transport_packet(rx_buf, MAX_PACKET_SIZE);

	// Sanity check: Ensure the Device sent a DATA1 PID
	packet_pid_t *pid = (packet_pid_t *)rx_buf;
	if (pid->type == PID_DATA_DATA1) {
		std::cout
		    << "[USB_Device_TB] Status Stage Received: DATA1 (ACK)"
		    << std::endl;
	}
}