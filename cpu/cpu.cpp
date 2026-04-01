#include "cpu.h"
#include "controller.h"
#include "log.h"
#include "packet.h"
#include <cstdint>
#include <sysc/kernel/sc_time.h>
#include <tlm_core/tlm_2/tlm_generic_payload/tlm_gp.h>

void CPU::firmware_thread()
{
	// TLM variables
	tlm::tlm_generic_payload trans;
	sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

	// Controller variables
	uint32_t cmd = HC_CMD_RUN;
	uint32_t status = HC_STS_STOP;

	/* Enable Controller */

	// write cmd
	trans.set_command(tlm::TLM_WRITE_COMMAND);
	trans.set_address(REG_USB_CMD);
	trans.set_data_ptr((uint8_t *)&cmd);
	trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

	socket->b_transport(trans, delay);

	// poll status
	while (status != HC_STS_IDLE) {
		poll_status(status);

		std::cout << "[CPU] polled status 0x" << std::hex << status
			  << std::endl;
	}

	/* GET_DESCRIPTOR */
	std::cout << "\n--- GET_DESCRIPTOR (0) ---\n" << std::endl;
	get_descriptor(0, 0);

	/* SET_ADDRESS */
	std::cout << "\n--- SET_ADDRESS (4) ---\n" << std::endl;
	set_address(0, 4, 0);

	/* GET_DESCRIPTOR */
	// std::cout << "\n--- GET_DESCRIPTOR (4) ---\n" << std::endl;
	// get_descriptor(4, 0);
}

void CPU::poll_status(uint32_t &status)
{
	tlm::tlm_generic_payload trans;
	sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

	trans.set_command(tlm::TLM_READ_COMMAND);
	trans.set_address(REG_USB_STS);
	trans.set_data_ptr((uint8_t *)&status);
	trans.set_data_length(4);
	trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

	socket->b_transport(trans, delay);
}

void CPU::get_descriptor(uint8_t addr, uint8_t endp)
{
	// TODO: Error status handling
	uint32_t status = HC_STS_STOP;

	/* SETUP stage */

	// setup token
	send_token(0, addr, endp, 0, REQ_GET_DESCRIPTOR, (DEVICE << 8), 18);

	// ack

	/* DATA stage */
	// poll status
	while (status != HC_STS_TR_COMP) {
		poll_status(status);
		LOG_DEBUG("[CPU] polled status 0x" << std::hex << status);
		wait(1, sc_core::SC_MS);
	}
	status = HC_STS_STOP;

	// in token
	send_token(1, addr, endp, 1, 0xff, 0xffff, 18);

	// ack

	/* STATUS stage */
	// poll status
	while (status != HC_STS_TR_COMP) {
		poll_status(status);
		LOG_DEBUG("[CPU] polled status 0x" << std::hex << status);
		wait(1, sc_core::SC_MS);
	}
	status = HC_STS_STOP;

	// out token
	send_token(2, addr, endp, 1, 0xff, 0xffff, 0);

	// ack
	// poll status
	while (status != HC_STS_TR_COMP) {
		poll_status(status);
		LOG_DEBUG("[CPU] polled status 0x" << std::hex << status);
		wait(1, sc_core::SC_MS);
	}
}

void CPU::set_address(uint32_t old_addr, uint32_t new_addr, uint32_t endp)
{
	// TODO: Error status handling
	uint32_t status = HC_STS_STOP;

	/* SETUP stage */

	// setup token
	send_token(0, old_addr, endp, 0, REQ_SET_ADDRESS, new_addr, 0);

	// ack

	/* STATUS stage */
	// poll status
	while (status != HC_STS_TR_COMP) {
		poll_status(status);
		LOG_DEBUG("[CPU] polled status 0x" << std::hex << status);
		wait(1, sc_core::SC_MS);
	}
	status = HC_STS_STOP;

	// in token
	send_token(1, old_addr, endp, 1, 0xff, 0xffff, 0);

	// ack
	// poll status
	while (status != HC_STS_TR_COMP) {
		poll_status(status);
		LOG_DEBUG("[CPU] polled status 0x" << std::hex << status);
		wait(1, sc_core::SC_MS);
	}
}

void CPU::send_token(uint8_t type, uint8_t addr, uint8_t endp, bool data_toggle,
		     uint8_t req, uint16_t value, uint16_t length)
{
	tlm::tlm_generic_payload trans;
	sc_core::sc_time delay;

	// we make the PID, DMA expects only the payload
	uint32_t ram_offset = sizeof(packet_pid_t);

	// write address & endp
	uint32_t addr_ep = (static_cast<uint32_t>(endp) << 7) | (addr & 0x7F);

	trans.set_command(tlm::TLM_WRITE_COMMAND);
	trans.set_address(REG_ADDR_ENDP);
	trans.set_data_ptr((uint8_t *)&addr_ep);
	trans.set_data_length(sizeof(addr_ep));
	trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

	socket->b_transport(trans, delay);

	LOG_DEBUG("[CPU] Sent addr & endp: 0x" << std::hex << addr_ep);

	// prepare DATA buffer for RAM and write to data register

	packet_pid_t *pid = (packet_pid_t *)ram;
	pid->type = data_toggle == 0 ? PID_DATA_DATA0 : PID_DATA_DATA1;
	pid->check = (uint8_t)(~pid->type & 0x0F);

	if (type == 0) {

		LOG_DEBUG("[CPU] Sending SETUP");
		setup_request_t *s_req =
		    (setup_request_t *)(ram + sizeof(packet_pid_t));
		s_req->bmRequestType =
		    (req == REQ_GET_DESCRIPTOR) ? 0x80 : 0x00;
		s_req->bRequest = req;
		s_req->wValue = value;
		s_req->wIndex = 0;
		s_req->wLength = length;

	} else if (type == 1) {
		LOG_DEBUG("[CPU] Sending IN");
		// nothing?
	} else if (type == 2) {
		LOG_DEBUG("[CPU] Sending OUT");
	}

	trans.set_command(tlm::TLM_WRITE_COMMAND);
	trans.set_address(REG_DATA_PTR);
	trans.set_data_ptr((uint8_t *)&ram_offset);
	trans.set_data_length(sizeof(ram_offset));
	trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

	socket->b_transport(trans, delay);

	LOG_DEBUG("[CPU] Sent data at: 0x" << std::hex << (uint64_t)ram_offset);

	// write token to initiate
	uint32_t token = 0;
	token = type;

	trans.set_command(tlm::TLM_WRITE_COMMAND);
	trans.set_address(REG_TOKEN);
	trans.set_data_ptr((uint8_t *)&token);
	trans.set_data_length(sizeof(token));
	trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

	socket->b_transport(trans, delay);

	LOG_DEBUG("[CPU] Sent token: " << (unsigned int)type);
}

void CPU::dma_b_transport(tlm::tlm_generic_payload &trans,
			  sc_core::sc_time &delay)
{
	tlm::tlm_command cmd = trans.get_command();
	uint32_t adr = trans.get_address();
	uint8_t *ptr = trans.get_data_ptr();
	uint32_t len = trans.get_data_length();

	// RAM bound check
	if (adr + len > sizeof(ram)) {
		LOG_ERROR("[CPU] DMA Access Out of Bounds! Addr: 0x" << std::hex
								     << adr);
		trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
		return;
	}

	// Perform the Move
	if (cmd == tlm::TLM_READ_COMMAND) {
		// Controller wants to READ from RAM
		std::memcpy(ptr, &ram[adr], len);
		// std::cout << "[CPU/RAM] DMA Read at 0x" << std::hex << adr <<
		// std::endl;
	} else if (cmd == tlm::TLM_WRITE_COMMAND) {
		// Controller wants to WRITE to RAM
		std::memcpy(&ram[adr], ptr, len);
		// std::cout << "[CPU/RAM] DMA Write at 0x" << std::hex << adr
		// << std::endl;
	}

	// Complete the Transaction
	trans.set_response_status(tlm::TLM_OK_RESPONSE);
}