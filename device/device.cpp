#include "device.h"
#include "common.h"
#include "packet.h"
#include <cstdint>
#include <iostream>
#include <tlm_core/tlm_2/tlm_generic_payload/tlm_gp.h>

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

	// data formats
	setup_request_t *request;

	switch (pid->type) {
	case PID_TOKEN_OUT:
	case PID_TOKEN_IN:
	case PID_TOKEN_SOF:
	case PID_TOKEN_SETUP:

		// cast to a token packet
		token = (token_t *)trans.get_data_ptr();

		// check FSM ordering
		if (tr_state != USB_TOKEN) {
			std::cerr << "[USB_Device] Unexpected packet received!"
				  << std::endl;
			trans.set_response_status(
			    tlm::TLM_GENERIC_ERROR_RESPONSE);
			break;
		}
		std::cout << "[USB_Controller] Received a TOKEN packet."
			  << std::endl;

		// Check address - ignore if incorrect
		if (token->address != addr) {
			break;
		}

		// we cannot service other than endpoint 0
		if (token->endp != 0) {
			std::cerr
			    << "[USB_Device] Non-zero endpoint referenced!"
			    << std::endl;
			trans.set_response_status(
			    tlm::TLM_GENERIC_ERROR_RESPONSE);
			break;
		}

		/* Process Packet */
		if (token->pid.type == PID_TOKEN_SETUP) {
			data_toggle = 0;
		} else if (token->pid.type == PID_TOKEN_IN ||
			   token->pid.type == PID_TOKEN_OUT) {
			data_toggle = 1;
		}

		last_token = (enum pid_token)token->pid.type;
		trans.set_response_status(tlm::TLM_OK_RESPONSE);
		/******************/

		tr_state = USB_DATA;
		break;
	case PID_DATA_DATA0:
	case PID_DATA_DATA1:
	case PID_DATA_DATA2:
	case PID_DATA_MDATA:

		// cast to a data packet
		data = (data_t *)trans.get_data_ptr();

		if (tr_state != USB_DATA) {
			std::cerr << "[USB_Device] Unexpected packet received!"
				  << std::endl;
			trans.set_response_status(
			    tlm::TLM_GENERIC_ERROR_RESPONSE);
			break;
		}
		std::cout << "[USB_Controller] Received a DATA packet."
			  << std::endl;

		/* Error States */
		if ((pid->type == PID_DATA_DATA0 && data_toggle == 1) ||
		    (pid->type == PID_DATA_DATA1 && data_toggle == 0)) {
			std::cerr
			    << "[USB_Device] Out-of-order data packet received!"
			    << std::endl;
			trans.set_response_status(
			    tlm::TLM_GENERIC_ERROR_RESPONSE);
			break;
		}
		/****************/

		/* Process Packet */
		if (last_token == PID_TOKEN_SETUP) {
			request = (setup_request_t *)data->data;

			// sanity check
			if (data->length != 8) {
				std::cerr << "[USB_Device] Received Setup "
					     "Request with "
					     "invalid length: "
					  << std::hex << (int)request->bRequest
					  << std::endl;
				trans.set_response_status(
				    tlm::TLM_GENERIC_ERROR_RESPONSE);
				break;
			}

			// GET_DESCRIPTOR
			if (request->bRequest == REQ_GET_DESCRIPTOR) {

				uint8_t desc_type = (request->wValue >> 8);
				uint8_t desc_index = (request->wValue & 0xFF);

				// DEVICE
				if (desc_type == DEVICE) {
					data_stage_ptr =
					    (uint8_t *)&dev_desc; // prepare to
								  // send device
								  // descriptor
					data_stage_len = sizeof(dev_desc);

					trans.set_response_status(
					    tlm::TLM_OK_RESPONSE);
				}

				// CONFIGURATION

				// INTERFACE

				// Make sure we do not send more than request
				// Some hosts usually get first 8 bytes only
				if (data_stage_len > request->wLength) {
					data_stage_len = request->wLength;
				}
			}
		} else if (last_token ==)

			// SET_ADDRESS
			/******************/

			tr_state = USB_TOKEN;
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