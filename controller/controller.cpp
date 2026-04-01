#include "controller.h"
#include "log.h"
#include "packet.h"
#include <cstdint>
#include <cstring>
#include <iostream>
#include <sysc/kernel/sc_time.h>
#include <systemc>
#include <tlm_core/tlm_2/tlm_generic_payload/tlm_gp.h>

void Controller::b_transport(tlm::tlm_generic_payload &trans,
			     sc_core::sc_time &delay)
{
	LOG_INFO("[Controller] CPU invoked b_transport");

	// register write, else read
	if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
		register_write(trans.get_address(),
			       *(uint32_t *)trans.get_data_ptr());
		trans.set_response_status(tlm::TLM_OK_RESPONSE);
	} else {
		uint8_t *data_ptr = trans.get_data_ptr();
		*data_ptr = register_read(trans.get_address());
		trans.set_response_status(tlm::TLM_OK_RESPONSE);
	}
}

void Controller::process_thread()
{

	while (true) {

		// yield control
		sc_core::wait(token_write_ev);

		// state machine
		if (state == HC_OPERATION) {

			switch (registers[REG_TOKEN]) {
			case PID_TOKEN_SETUP:
			case PID_TOKEN_IN:
			case PID_TOKEN_OUT:
				execute_usb_transaction(registers[REG_TOKEN]);
				break;
			default:
				LOG_ERROR("[Controller] unknown token!");
				state = HC_ERROR;
				registers[REG_USB_STS] = HC_STS_ERROR;
				break;
			}
		}
	}
}

void Controller::register_write(uint8_t offset, uint32_t data)
{
	// Sanity checks
	if (offset % 0x4 != 0) {
		LOG_ERROR("[Controller] Un-aligned register offset!");
		return;
	}

	if (offset >= 0x18) {
		LOG_ERROR("[Controller] Out of bound register offset!");
		return;
	}

	// decoding
	switch (offset) {
	case REG_USB_CMD:
		if (data & HC_CMD_RUN) {
			state = HC_RUNNING;
			// IDLE states
			registers[REG_USB_STS] = (uint32_t)0;

		} else if (data & HC_CMD_RESET) {
			state = HC_RESET;
			// Reset registers
			memset(registers, 0, sizeof(registers));
			state = HC_STOPPED;

			// STOPPED status
			registers[REG_USB_STS] = HC_STS_STOP;

		} else if (state != HC_ERROR && (data & HC_CMD_STOP) == 0) {
			state = HC_STOPPED;

			// STOPPED status
			registers[REG_USB_STS] = HC_STS_STOP;
		} else if (state == HC_ERROR) {
			LOG_ERROR("[Controller] ERROR state!");
		} else {
			LOG_ERROR("[Controller] Invalid CMD issued!");
		}

		break;
	case REG_USB_STS:
		LOG_ERROR("[Controller] Cannot write to STS register!");
		break;
	case REG_PORT_SC:
		// TODO: Port control
		break;
	case REG_ADDR_ENDP:
		*(uint32_t *)(registers + REG_ADDR_ENDP) = data;
		break;
	case REG_DATA_PTR:
		*(uint32_t *)(registers + REG_DATA_PTR) = data;
		break;
	case REG_TOKEN:

		if (state == HC_OPERATION) {
			LOG_ERROR("[Controller] Controller BUSY!");
			break;
		} else if (state == HC_ERROR) {
			LOG_ERROR("[Controller] ERROR state!");
			break;
		}

		LOG_DEBUG("[Controller] Writing token: " << (unsigned int)data);

		if (data == 0) {
			registers[REG_TOKEN] = PID_TOKEN_SETUP;
		} else if (data == 1) {
			registers[REG_TOKEN] = PID_TOKEN_IN;
		} else if (data == 2) {
			registers[REG_TOKEN] = PID_TOKEN_OUT;
		} else {
			LOG_ERROR("[Controller] Invalid token data!");
		}

		if (data <= 2) {
			state = HC_OPERATION;
			registers[REG_USB_STS] = HC_STS_BUSY;

			LOG_DEBUG("[Controller] Token available! Fired event.");
			token_write_ev.notify();
		}
		break;
	default:
		LOG_ERROR("[Controller] Unknown register offset!");
		break;
	}
}

uint32_t Controller::register_read(uint8_t offset)
{
	// sanity checks
	if (offset % 0x4 != 0) {
		LOG_ERROR("[Controller] Un-aligned register offset!");
		return 0xFFFFFFFF;
	}

	if (offset >= 0x18) {
		LOG_ERROR("[Controller] Out of bound register offset!");
		return 0xFFFFFFFF;
	}

	uint32_t data;

	// decoding
	switch (offset) {
	case REG_USB_CMD:
	case REG_USB_STS:
	case REG_PORT_SC:
	case REG_ADDR_ENDP:
	case REG_DATA_PTR:
	case REG_TOKEN:
		data = *(uint32_t *)(registers + offset);
		LOG_DEBUG("[Controller] read data 0x" << std::hex << data);
		break;
	default:
		LOG_ERROR("[Controller] Unknown register offset!");
		break;
	}

	return data;
}

/* Device Interface */
void Controller::execute_usb_transaction(uint8_t token_type)
{

	/* Token */
	tlm::tlm_response_status response =
	    send_token(token_type, ((uint8_t)registers[REG_ADDR_ENDP]) & 0x7F,
		       registers[REG_ADDR_ENDP] >> 7);

	wait(1, sc_core::SC_MS);

	if (response == tlm::TLM_OK_RESPONSE) {
		LOG_INFO("[Controller] Transaction complete!");
	} else {
		LOG_ERROR("[Controller] Transaction failed!");

		state = HC_ERROR;
		registers[REG_USB_STS] = HC_STS_ERROR;
	}

	/* Data */
	if (token_type == PID_TOKEN_SETUP || token_type == PID_TOKEN_OUT) {
		// DMA: Read data from System RAM at REG_DATA_PTR
		LOG_DEBUG("[Controller] Attempting DMA read at 0x"
			  << std::hex
			  << *(uint32_t *)(registers + REG_DATA_PTR));
		this->dma_read(*(uint32_t *)(registers + REG_DATA_PTR), buffer,
			       8);
		LOG_DEBUG("[Controller] DMA succesful");

		// Send the data packet to the USB device
		if (token_type == PID_TOKEN_OUT)
			send_data_packet(PID_DATA_DATA1, buffer, 8);
		else
			send_data_packet(PID_DATA_DATA0, buffer, 8);

	} else if (token_type == PID_TOKEN_IN) {
		// Receive data from the device
		uint32_t len;
		receive_data_packet(PID_DATA_DATA1, buffer, len);

		LOG_DEBUG("[Controller] (IN) Attempting DMA write at 0x"
			  << std::hex
			  << *(uint32_t *)(registers + REG_DATA_PTR));

		// DMA: Write data back to System RAM
		this->dma_write(*(uint32_t *)(registers + REG_DATA_PTR), buffer,
				len);

		LOG_DEBUG("[Controller] (OUT) DMA succesful");
	}

	/* ACK */
	registers[REG_USB_STS] = HC_STS_TR_COMP;
	state = HC_RUNNING;
}

/* Low Level Helpers */
tlm::tlm_response_status Controller::send_token(uint8_t type, uint8_t addr,
						uint8_t endp)
{
	tlm::tlm_generic_payload trans;
	sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

	token_t token_pkt;
	token_pkt.pid.type = type;
	token_pkt.pid.check = (uint8_t)(~type & 0x0F);
	token_pkt.address = addr;
	token_pkt.endp = endp;

	trans.set_command(tlm::TLM_WRITE_COMMAND);
	trans.set_data_ptr((uint8_t *)&token_pkt);
	trans.set_data_length(sizeof(token_pkt));
	trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

	dev_out_sock->b_transport(trans, delay);

	return trans.get_response_status();
}

tlm::tlm_response_status
Controller::send_data_packet(uint8_t pid_type, uint8_t *src, uint32_t len)
{
	tlm::tlm_generic_payload trans;
	sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

	// Create a "Wire Buffer" that holds [PID | DATA]
	uint8_t wire_buf[MAX_PACKET_SIZE + 1];

	// Prepend the PID
	packet_pid_t *pid = (packet_pid_t *)wire_buf;
	pid->type = pid_type; // Use the type passed in (DATA0 or DATA1)
	pid->check = (uint8_t)(~pid_type & 0x0F);

	// Copy the payload from System RAM into the wire buffer
	if (len > 0) {
		std::memcpy(wire_buf + 1, src, len);
	}

	trans.set_command(tlm::TLM_WRITE_COMMAND);
	trans.set_data_ptr(wire_buf);
	trans.set_data_length(len + 1); // PID (1) + Data
	trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

	dev_out_sock->b_transport(trans, delay);

	return trans.get_response_status();
}
tlm::tlm_response_status
Controller::receive_data_packet(uint8_t pid_type, uint8_t *dest, uint32_t &len)
{
	tlm::tlm_generic_payload trans;
	sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

	// Prepare a buffer to catch the incoming packet
	uint8_t wire_buf[MAX_PACKET_SIZE + 1];
	packet_pid_t *pid = (packet_pid_t *)wire_buf;
	pid->type = pid_type; // Use the type passed in (DATA0 or DATA1)
	pid->check = (uint8_t)(~pid_type & 0x0F);

	trans.set_command(tlm::TLM_WRITE_COMMAND); // Still a 'WRITE' in TLM to
						   // pass the pointer
	trans.set_data_ptr(wire_buf);
	trans.set_data_length(MAX_PACKET_SIZE + 1);
	trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

	// Call the Device. The Device fills wire_buf
	dev_out_sock->b_transport(trans, delay);

	if (trans.is_response_error())
		return trans.get_response_status();

	// Verification
	if (pid->type != (uint8_t)(~pid->check & 0x0F)) {
		LOG_ERROR("[Controller] Device sent corrupted PID!");
		return tlm::TLM_GENERIC_ERROR_RESPONSE;
	}

	// Extract the payload and length
	len = trans.get_data_length() - 1; // Total length minus the 1-byte PID
	if (len > 0) {
		std::memcpy(dest, wire_buf + 1, len);
	}

	return tlm::TLM_OK_RESPONSE;
}

void Controller::dma_read(uint32_t addr, uint8_t *dest, uint32_t len)
{
	tlm::tlm_generic_payload trans;
	sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

	trans.set_command(tlm::TLM_READ_COMMAND);
	trans.set_address(addr);
	trans.set_data_ptr(dest);
	trans.set_data_length(len);

	// Call the System RAM (or Bus)
	dma_sock->b_transport(trans, delay);
}

void Controller::dma_write(uint32_t addr, uint8_t *src, uint32_t len)
{
	if (len == 0)
		return;

	tlm::tlm_generic_payload trans;
	sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

	// Configure the transaction as a WRITE to System Memory
	trans.set_command(tlm::TLM_WRITE_COMMAND);
	trans.set_address(addr);
	trans.set_data_ptr(src);
	trans.set_data_length(len);

	trans.set_byte_enable_ptr(nullptr);
	trans.set_streaming_width(len);
	trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

	// Send the transaction out through the DMA socket
	dma_sock->b_transport(trans, delay);

	// Error Checking
	if (trans.is_response_error()) {
		LOG_ERROR("[Controller] DMA Write Error at address 0x"
			  << std::hex << addr);
		state = HC_ERROR;
		registers[REG_USB_STS] = HC_STS_ERROR;
	}
}