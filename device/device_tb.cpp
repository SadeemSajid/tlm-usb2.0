#include "device_tb.h"
#include "packet.h"
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

void USB_Device_TB::run_test()
{
	sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

	std::cout << "\n--- Starting USB GET_DESCRIPTOR Test ---\n"
		  << std::endl;

	// 1. SETUP STAGE
	send_setup_stage(0, 0, REQ_GET_DESCRIPTOR, (DEVICE << 8), 18);

	// 2. DATA STAGE (Device sends data to USB_Device_TB)
	send_data_stage_in(0, 0, 18);

	// 3. STATUS STAGE (USB_Device_TB sends ZLP to Device)
	send_status_stage_out(0, 0);

	std::cout << "\n--- Test Complete ---" << std::endl;
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
		std::cerr << "TLM Transaction Error!" << std::endl;
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
	std::cout << "[USB_Device_TB] Initiating DATA Stage (IN)..."
		  << std::endl;

	// A. Token Packet
	token_t token_pkt;
	token_pkt.pid.type = PID_TOKEN_IN;
	token_pkt.pid.check = (uint8_t)(~PID_TOKEN_IN & 0x0F);
	token_pkt.address = addr;
	token_pkt.endp = ep;
	transport_packet((uint8_t *)&token_pkt, sizeof(token_t));

	// B. Data Packet (USB_Device_TB provides buffer for device to fill)
	uint8_t rx_buf[MAX_PACKET_SIZE];
	transport_packet(rx_buf, MAX_PACKET_SIZE);

	// Print what we received
	std::cout << "[USB_Device_TB] Received Descriptor Bytes: ";
	for (int i = 0; i < 8; i++)
		printf("%02X ", rx_buf[i + 1]); // +1 to skip PID
	std::cout << "..." << std::endl;
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
