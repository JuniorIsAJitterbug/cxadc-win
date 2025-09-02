// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Jitterbug

#include "tusb.h"
#include "tusb_config.h"
#include "usb_vendor.h"
#include "usb_descriptors.h"
#include "clock_gen.h"
#include "build_info.h"

bool vendor_handle_req_ms_descriptor(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request)
{
	if( request->wIndex == 7 )
	{
		uint8_t len = 0;
		const uint8_t* data = usb_descriptor_get_ms_os_20(&len);

		// Microsoft OS 2.0 compatible descriptor
		return tud_control_xfer(rhport, request, (void*)data, len);
	}

	return false;
}

bool vendor_handle_req_cxadc_sample_rate(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request)
{
	static uint8_t rx_cx_buf = 0;

	if( stage == CONTROL_STAGE_SETUP )
	{
		if( request->bmRequestType_bit.direction == TUSB_DIR_OUT )
		{
			// wIndex = cxadc output device
			// buffer = new cxadc option index (1 * u8)
			if( request->wLength != sizeof(rx_cx_buf) )
			{
				return false;
			}

			// xfer to buffer for DATA stage
			return tud_control_xfer(rhport, request, (void*)&rx_cx_buf, sizeof(rx_cx_buf));
		}

		if( request->bmRequestType_bit.direction == TUSB_DIR_IN )
		{
			// wIndex = cxadc output device
			// return = current cxadc option index (1 * u8)
			uint8_t tx_cx_buf = clock_gen_get_cxadc_sample_rate(request->wIndex);

			if( request->wLength < sizeof(tx_cx_buf) )
			{
				return false;
			}

			return tud_control_xfer(rhport, request, (void*)&tx_cx_buf, sizeof(tx_cx_buf));
		}
	}

	if( stage == CONTROL_STAGE_DATA )
	{
		if( request->bmRequestType_bit.direction == TUSB_DIR_OUT )
		{
			return clock_gen_set_cxadc_sample_rate(request->wIndex, rx_cx_buf);
		}
	}

	return true;
}

bool vendor_handle_req_adc_sample_rate(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request)
{
	static uint32_t rx_adc_buf = 0;

	if( stage == CONTROL_STAGE_SETUP )
	{
		if( request->bmRequestType_bit.direction == TUSB_DIR_OUT )
		{
			// buffer = new adc sample rate (1 * u32)
			if( request->wLength != sizeof(rx_adc_buf) )
			{
				return false;
			}

			return tud_control_xfer(rhport, request, (void*)&rx_adc_buf, sizeof(rx_adc_buf));
		}

		if( request->bmRequestType_bit.direction == TUSB_DIR_IN )
		{
			// return = current adc sample rate (1 * u32)
			uint32_t tx_adc_buf = clock_gen_get_adc_sample_rate();

			if( request->wLength < sizeof(tx_adc_buf) )
			{
				return false;
			}

			return tud_control_xfer(rhport, request, (void*)&tx_adc_buf, sizeof(tx_adc_buf));
		}
	}

	if( stage == CONTROL_STAGE_DATA )
	{
		if( request->bmRequestType_bit.direction == TUSB_DIR_OUT )
		{
			return clock_gen_set_adc_sample_rate(rx_adc_buf);
		}
	}

	return true;
}

bool vendor_handle_req_cxadc_sample_rate_options(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request)
{
	if( stage == CONTROL_STAGE_SETUP && request->bmRequestType_bit.direction == TUSB_DIR_IN)
	{
		// return = str descriptor idxs of cxadc sample rates (rate_count * u16)
		uint8_t options_count = 0;
		const uint16_t* options = usb_descriptor_get_strd_idx_input_freqs(&options_count);
		const uint16_t options_len = options_count * sizeof(uint16_t);

		if( request->wLength < options_len )
		{
			return false;
		}

		return tud_control_xfer(rhport, request, (void*)options, options_len);
	}

	return true;
}

bool vendor_handle_req_adc_sample_rate_options(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request)
{
	if( stage == CONTROL_STAGE_SETUP && request->bmRequestType_bit.direction == TUSB_DIR_IN )
	{
		// return = adc sample rates (rate_count * u32)
		uint8_t options_count = 0;
		const uint32_t* options = clock_gen_get_adc_sample_rate_options(&options_count);
		const uint16_t options_len = options_count * sizeof(uint32_t);

		if( request->wLength < sizeof(options_len) )
		{
			return false;
		}

		return tud_control_xfer(rhport, request, (void*)options, options_len);
	}

	return true;
}

bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request)
{
	if( request->bmRequestType_bit.type == TUSB_REQ_TYPE_VENDOR )
	{
		switch( request->bRequest )
		{
			case VENDOR_REQUEST_MICROSOFT:
				return vendor_handle_req_ms_descriptor(rhport, stage, request);

			case VENDOR_REQUEST_CXADC_SAMPLE_RATE:
				return vendor_handle_req_cxadc_sample_rate(rhport, stage, request);

			case VENDOR_REQUEST_ADC_SAMPLE_RATE:
				return vendor_handle_req_adc_sample_rate(rhport, stage, request);

			case VENDOR_REQUEST_CXADC_SAMPLE_RATE_OPTIONS:
				return vendor_handle_req_cxadc_sample_rate_options(rhport, stage, request);

			case VENDOR_REQUEST_ADC_SAMPLE_RATE_OPTIONS:
				return vendor_handle_req_adc_sample_rate_options(rhport, stage, request);

			default:
				break;
		}

		// unknown request
		return false;
	}

	return true;
}
