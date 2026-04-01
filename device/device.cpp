#include "device.h"
#include "common.h"
#include "log.h"
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

	// Sanity check if not IN
	if (last_token != PID_TOKEN_IN &&
	    pid->type != (uint8_t)(~pid->check & 0x0F)) {
		LOG_ERROR("[USB_Device] pid " << std::hex << pid->type
					      << " failed sanity check!");
		trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
		return;
	}

	// Packet decoding

	bool return_status = true;

	if (tr_state == USB_TOKEN) {
		// We are expecting a Token (SETUP, IN, or OUT)
		LOG_INFO("[USB Device] Processing token...");

		return_status =
		    process_token((token_t *)trans.get_data_ptr(), trans);
		if (return_status) {
			tr_state = USB_DATA;
			trans.set_response_status(tlm::TLM_OK_RESPONSE);
		} else {
			LOG_ERROR(
			    "[USB_Device] process_token() returned false");
			trans.set_response_status(
			    tlm::TLM_GENERIC_ERROR_RESPONSE);
		}
	} else if (tr_state == USB_DATA) {
		// We are expecting a Data packet (DATA0/DATA1)

		LOG_INFO("[USB Device] Processing data...");

		return_status =
		    process_data((data_t *)trans.get_data_ptr(), trans);
		if (return_status) {
			tr_state = USB_TOKEN;
			trans.set_response_status(tlm::TLM_OK_RESPONSE);
		} else {
			LOG_ERROR("[USB_Device] process_data() returned false");
			trans.set_response_status(
			    tlm::TLM_GENERIC_ERROR_RESPONSE);
		}
	}

	LOG_INFO("[USB_Device] TR_STATE / Expecting: "
		 << (tr_state == USB_DATA ? "DATA" : "TOKEN"));
}

bool USB_Device::process_token(token_t *token, tlm::tlm_generic_payload &trans)
{
	// check FSM ordering
	if (tr_state != USB_TOKEN) {
		LOG_ERROR("[USB_Device] Unexpected packet received!");
		LOG_ERROR("[USB_Device] pid: " << std::hex << token->pid.type);
		trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
		return false;
	}
	LOG_INFO("[USB_Device] Received a TOKEN packet.");

	// Check address - ignore if incorrect
	if (token->address != addr) {
		return false;
		LOG_DEBUG("[USB_Device] Ignored packet");
	}

	// we cannot service other than endpoint 0
	if (token->endp != 0) {
		LOG_ERROR("[USB_Device] Non-zero endpoint referenced!");
		trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
		return false;
	}

	/* Process Packet */
	if (token->pid.type == PID_TOKEN_SETUP) {
		data_toggle = 0;
		LOG_INFO("[USB_Device] CTRL FSM: "
			 "USB_CTRL_SETUP");
		ctrl_state = USB_CTRL_SETUP; // We now expect DATA (Standard
					     // Request) --> Handshake
	} else if (token->pid.type == PID_TOKEN_IN) {
		data_toggle = 1;

		// move to STATUS if ctrl_data is skipped
		if (ctrl_data_skip) {
			ctrl_state = USB_CTRL_STATUS;

			LOG_INFO("[USB_Device] CTRL FSM: "
				 "USB_CTRL_STATUS");

			ctrl_data_skip = false;
		}
		// if in SETUP, transition to DATA
		else if (ctrl_state == USB_CTRL_SETUP) {
			ctrl_state = USB_CTRL_DATA;

			LOG_INFO("[USB_Device] CTRL FSM: "
				 "USB_CTRL_DATA");
		}
		// ...else we have a problem in CTRL phases
		else if (ctrl_state != USB_CTRL_NONE) {

			LOG_ERROR(
			    "[USB_Device] CTRL FSM violation: IN received "
			    "out of order for CTRL phases");
			return false;
		}

	} else if (token->pid.type == PID_TOKEN_OUT) {
		data_toggle = 1;

		// if in DATA, transition to STATUS
		if (ctrl_state == USB_CTRL_DATA) {
			ctrl_state = USB_CTRL_STATUS;
			LOG_INFO("[USB_Device] CTRL FSM: "
				 "USB_CTRL_STATUS");
		}
		// ...else we have a problem in CTRL phases
		else if (ctrl_state != USB_CTRL_NONE) {

			LOG_ERROR(
			    "[USB_Device] CTRL FSM violation: OUT received "
			    "out of order for CTRL phases");
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
		LOG_ERROR("[USB_Device] Unexpected packet received!");
		LOG_ERROR("[USB_Device] pid: " << std::hex << data->pid.type);
		trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
		return false;
	}
	LOG_INFO("[USB_Device] Received a DATA packet.");

	LOG_DEBUG("[USB Device] PID: 0x" << std::hex
					 << (unsigned int)data->pid.type);

	/* Error States */
	if ((data->pid.type == PID_DATA_DATA0 && data_toggle == 1) ||
	    (data->pid.type == PID_DATA_DATA1 && data_toggle == 0)) {
		LOG_ERROR("[USB_Device] Out-of-order data packet received!");
		trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
		return false;
	}
	/****************/

	/* Process Packet */
	if (last_token == PID_TOKEN_SETUP) {

		// We are in SETUP phase

		request = (setup_request_t *)(trans.get_data_ptr() +
					      sizeof(packet_pid_t));

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
			}

			// CONFIGURATION
			else if (desc_type == CONFIGURATION) {
				// Return Config + Interface + all
				// Endpoints in one go. Copy Config Descriptor
				memcpy(buffer, &conf_desc, sizeof(conf_desc));
				data_stage_len = sizeof(conf_desc);

				// Copy Interface Descriptor immediately after
				memcpy(buffer + data_stage_len, &int_desc,
				       sizeof(int_desc));
				data_stage_len += sizeof(int_desc);

				// TODO: End-point descriptors
			}
			// INTERFACE
			else if (desc_type == INTERFACE) {
				memcpy(buffer, &int_desc, sizeof(int_desc));
				data_stage_len = sizeof(int_desc);
			}

			// STRING
			else if (desc_type == STRING) {
				if (desc_index == 0) {
					memcpy(buffer, &lang_desc,
					       sizeof(lang_desc));
					data_stage_len = sizeof(lang_desc);
				} else if (desc_index == 1) { // iManufacturer
					memcpy(buffer, &vendor_desc,
					       vendor_desc.bLength);
					data_stage_len = vendor_desc.bLength;
				} else if (desc_index == 2) { // iProduct
					memcpy(buffer, &product_desc,
					       product_desc.bLength);
					data_stage_len = product_desc.bLength;
				}
			}

			// Make sure we do not send more than request
			// Some hosts usually get first 8 bytes only
			if (data_stage_len > 0) {
				// Clamp to host's requested length
				if (data_stage_len > request->wLength) {
					data_stage_len = request->wLength;
				}

				// TODO: Replace this with an explicit ACK
				trans.set_response_status(tlm::TLM_OK_RESPONSE);
			} else {
				// Stall if descriptor type isn't supported
				trans.set_response_status(
				    tlm::TLM_GENERIC_ERROR_RESPONSE);
			}
		}
		// SET ADDRESS
		else if (request->bRequest == REQ_SET_ADDRESS) {

			pending_addr = (uint8_t)request->wValue;

			LOG_INFO("[USB_Device] Received SET_ADDRESS: "
				 << (int)pending_addr << " (Pending)");

			// SET_ADDRESS has no Data Stage, go straight to STATUS
			ctrl_data_skip = true;
			trans.set_response_status(tlm::TLM_OK_RESPONSE);
		}

	} else if (last_token == PID_TOKEN_IN) {

		// STATUS stages needs a ZLP
		if (ctrl_state == USB_CTRL_STATUS) {

			trans.set_data_length(
			    sizeof(packet_pid_t)); // Just the PID
			trans.set_response_status(tlm::TLM_OK_RESPONSE);

			LOG_INFO("[USB_Device] Sending Status Stage ZLP...");

			// See if address needs updating
			if (pending_addr != 0) {
				LOG_INFO("[USB_Device] Address changed from "
					 << (int)addr << " to "
					 << (int)pending_addr);
				addr = pending_addr;
				pending_addr = 0; // Reset pending

				state = USB_ADDRESS;
			}

			// Transition to NONE control
			if (ctrl_state == USB_CTRL_STATUS) {
				ctrl_state = USB_CTRL_NONE;
			}

		} else {
			// Copy data into the packet sent by the host
			// Keep PID intact
			memcpy(trans.get_data_ptr() + sizeof(packet_pid_t),
			       buffer, data_stage_len);

			// TODO: Set transaction data length?

			// TODO: Replace this with an explicit ACK
			trans.set_response_status(tlm::TLM_OK_RESPONSE);

			if (data_stage_len != 0) {
				// Clear buffer and related variables
				memset(buffer, 0, MAX_PACKET_SIZE);
				data_stage_len = 0;
			}
		}

		// flip data toggle & update state
		data_toggle = !data_toggle;
		tr_state = USB_TOKEN;

	} else if (last_token == PID_TOKEN_OUT) {

		uint32_t payload_len =
		    trans.get_data_length()
			? trans.get_data_length() - sizeof(packet_pid_t)
			: 0;

		// legnth safety check
		if (payload_len > MAX_PACKET_SIZE) {
			payload_len = MAX_PACKET_SIZE;
		}

		// Copy data from the packet into local buffer
		memcpy(buffer, trans.get_data_ptr() + sizeof(packet_pid_t),
		       payload_len);

		// TODO: Replace this with an explicit ACK
		trans.set_response_status(tlm::TLM_OK_RESPONSE);

		// flip data toggle & update state
		data_toggle = !data_toggle;
		tr_state = USB_TOKEN;

		// Transition to NONE control
		if (ctrl_state == USB_CTRL_STATUS) {
			ctrl_state = USB_CTRL_NONE;
			LOG_INFO("[USB_Device] CTRL FSM: "
				 "USB_CTRL_NONE");
		}
	}

	return true;
}