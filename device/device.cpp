#include "device.h"
#include "common.h"
#include "packet.h"
#include <cstdint>
#include <cstring>
#include <iostream>
#include <tlm_core/tlm_2/tlm_generic_payload/tlm_gp.h>

// TODO: return STALL for un-supported requests in STATUS stage

void USB_Device::b_transport(tlm::tlm_generic_payload &trans,
			     sc_core::sc_time &delay)
{
	// Get PID to determine request type
	packet_pid_t *pid = (packet_pid_t *)trans.get_data_ptr();

	// Sanity check
	if (pid->type != (uint8_t)(~pid->check & 0x0F)) {
		std::cerr << "[USB_Device] pid " << std::hex << pid->type
			  << " failed sanity check!\n"
			  << std::endl;
		trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
		return;
	}

	// Packet decoding

	// general formats
	token_t *token;
	data_t *data;
	handshake_t *handshake;

	bool return_status;

	switch (pid->type) {
	case PID_TOKEN_OUT:
	case PID_TOKEN_IN:
	case PID_TOKEN_SOF:
	case PID_TOKEN_SETUP:

		return_status = false;

		// cast to a token packet
		token = (token_t *)trans.get_data_ptr();

		return_status = process_token(token, trans);

		if (return_status) {
			tr_state = USB_DATA;
		}

		break;
	case PID_DATA_DATA0:
	case PID_DATA_DATA1:
	case PID_DATA_DATA2:
	case PID_DATA_MDATA:

		return_status = false;

		// cast to a data packet
		data = (data_t *)trans.get_data_ptr();

		return_status = process_data(data, trans);

		if (return_status) {
			tr_state = USB_TOKEN;
		}

		break;
	case PID_HANDSHAKE_ACK:
	case PID_HANDSHAKE_NAK:
	case PID_HANDSHAKE_STALL:
	case PID_HANDSHAKE_NYET:
	default:
		std::cerr << "[USB_Device] Unknown pid received!" << std::endl;
		break;
	}
}

bool USB_Device::process_token(token_t *token, tlm::tlm_generic_payload &trans)
{
	// check FSM ordering
	if (tr_state != USB_TOKEN) {
		std::cerr << "[USB_Device] Unexpected packet received!"
			  << std::endl;
		trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
		return false;
	}
	std::cout << "[USB_Device] Received a TOKEN packet." << std::endl;

	// Check address - ignore if incorrect
	if (token->address != addr) {
		return false;
	}

	// we cannot service other than endpoint 0
	if (token->endp != 0) {
		std::cerr << "[USB_Device] Non-zero endpoint referenced!"
			  << std::endl;
		trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
		return false;
	}

	/* Process Packet */
	if (token->pid.type == PID_TOKEN_SETUP) {
		data_toggle = 0;
		std::cout << "[USB_Device] FSM: "
			     "USB_CTRL_SETUP"
			  << std::endl;
		ctrl_state = USB_CTRL_SETUP; // We now expect DATA (Standard
					     // Request) --> Handshake
	} else if (token->pid.type == PID_TOKEN_IN) {
		data_toggle = 1;

		// if in SETUP, transition to DATA
		if (ctrl_state == USB_CTRL_SETUP) {
			ctrl_state = USB_CTRL_DATA;
		}
		// ...else we have a problem in CTRL phases
		else if (ctrl_state != USB_CTRL_NONE) {

			std::cout << "[USB_Device] FSM violation: IN received "
				     "out of order for CTRL phases"
				  << std::endl;
			return false;
		}

	} else if (token->pid.type == PID_TOKEN_OUT) {
		data_toggle = 1;

		// if in DATA, transition to STATUS
		if (ctrl_state == USB_CTRL_DATA) {
			ctrl_state = USB_CTRL_STATUS;
		}
		// ...else we have a problem in CTRL phases
		else if (ctrl_state != USB_CTRL_NONE) {

			std::cout << "[USB_Device] FSM violation: OUT received "
				     "out of order for CTRL phases"
				  << std::endl;
			return false;
		}
	}

	last_token = (enum pid_token)token->pid.type;
	trans.set_response_status(tlm::TLM_OK_RESPONSE);
	/******************/

	return true;
}

bool USB_Device::process_data(data_t *data, tlm::tlm_generic_payload &trans)
{
	setup_request_t *request;

	if (tr_state != USB_DATA) {
		std::cerr << "[USB_Device] Unexpected packet received!"
			  << std::endl;
		trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
		return false;
	}
	std::cout << "[USB_Device] Received a DATA packet." << std::endl;

	/* Error States */
	if ((data->pid.type == PID_DATA_DATA0 && data_toggle == 1) ||
	    (data->pid.type == PID_DATA_DATA1 && data_toggle == 0)) {
		std::cerr << "[USB_Device] Out-of-order data packet received!"
			  << std::endl;
		trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
		return false;
	}
	/****************/

	/* Process Packet */
	if (last_token == PID_TOKEN_SETUP) {

		// We are in SETUP phase

		request = (setup_request_t *)(data->data);

		// sanity check
		// TODO: Is this really correct? Looks like OS-Controlled
		// behavior instead

		// if (data->length != 8) {
		// 	std::cerr << "[USB_Device] Received Setup "
		// 		     "Request with "
		// 		     "invalid length: "
		// 		  << std::hex << (int)request->bRequest
		// 		  << std::endl;
		// 	trans.set_response_status(
		// 	    tlm::TLM_GENERIC_ERROR_RESPONSE);
		// 	return false;
		// }

		// GET_DESCRIPTOR
		if (request->bRequest == REQ_GET_DESCRIPTOR) {

			uint8_t desc_type = (request->wValue >> 8);
			uint8_t desc_index = (request->wValue & 0xFF);

			// DEVICE
			if (desc_type == DEVICE) {

				// Write device descriptor to local buffer for
				// copyin in DATA phase
				memcpy(buffer, &dev_desc, sizeof(dev_desc));
				data_stage_len = sizeof(dev_desc);

				// TODO: Replace this with an explicit ACK
				trans.set_response_status(tlm::TLM_OK_RESPONSE);

				// We are now in DATA phase if data is available
				// STATUS phase if data is not available
				// if (request->wLength == 0) {
				// 	std::cout << "[USB_Device] FSM: "
				// 		     "USB_CTRL_STATUS"
				// 		  << std::endl;
				// 	ctrl_state = USB_CTRL_STATUS;
				// } else {
				// 	std::cout << "[USB_Device] FSM: "
				// 		     "USB_CTRL_DATA"
				// 		  << std::endl;
				// 	ctrl_state = USB_CTRL_DATA;
				// }
			}

			// CONFIGURATION

			// INTERFACE

			// Make sure we do not send more than request
			// Some hosts usually get first 8 bytes only
			if (data_stage_len > request->wLength) {
				data_stage_len = request->wLength;
			}
		}
	} else if (last_token == PID_TOKEN_IN) {
		// Copy data into the packet sent by the host
		// Keep PID intact
		memcpy(trans.get_data_ptr() + sizeof(packet_pid_t), buffer,
		       data_stage_len);

		// TODO: Set transaction data length?

		// TODO: Replace this with an explicit ACK
		trans.set_response_status(tlm::TLM_OK_RESPONSE);

		if (data_stage_len != 0) {
			// Clear buffer and related variables
			memset(buffer, 0, MAX_PACKET_SIZE);
			data_stage_len = 0;
		}

		// flip data toggle & update state
		data_toggle = !data_toggle;
		tr_state = USB_TOKEN;

	} else if (last_token == PID_TOKEN_OUT) {

		// Copy data from the packet into local buffer
		memcpy(buffer, trans.get_data_ptr() + sizeof(packet_pid_t),
		       ((data_t *)trans.get_data_ptr())->length);

		// TODO: Replace this with an explicit ACK
		trans.set_response_status(tlm::TLM_OK_RESPONSE);

		// flip data toggle & update state
		data_toggle = !data_toggle;
		tr_state = USB_TOKEN;

		// Transition to NONE control
		if (ctrl_state == USB_CTRL_STATUS) {
			ctrl_state = USB_CTRL_NONE;
		}
	}

	return true;
}