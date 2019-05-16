/* Teensyduino Core Library
 * http://www.pjrc.com/teensy/
 * Copyright (c) 2017 PJRC.COM, LLC.
 * Copyright (c) 2018-2019 dogtopus
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * 1. The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * 2. If the Software is incorporated into a build system that allows
 * selection among a list of target devices, then similar target
 * devices manufactured by PJRC.COM must be included in the list of
 * target devices and selectable in the same manner.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "usb_dev.h"
#include "usb_ds4.h"
#include "core_pins.h" // for yield()
#include "HardwareSerial.h"
#include <string.h> // for memcpy()

#if defined(DS4_INTERFACE) && defined(USB_DS4)
#if F_CPU >= 20000000

#define TX_PACKET_LIMIT 2

#ifndef DS4_PACKET_TIMEOUT
#define DS4_PACKET_TIMEOUT 1000
#endif

#if defined(DS4_DEBUG_INFO) && DS4_DEBUG_INFO == 1
#define debug_print(args) serial_print(args)
#define debug_phex(args) serial_phex(args)
#define debug_phex16(args) serial_phex16(args)
#define debug_phex32(args) serial_phex32(args)
#else
#define debug_print(args) while (0) {}
#define debug_phex(args) while (0) {}
#define debug_phex16(args) while (0) {}
#define debug_phex32(args) while (0) {}
#endif

// Was ripped from GIMX HoriPad emulation firmware, now ripped from Hori Mini
// packet capture, with patched feature bits.
// (The philosophical question is, since it seems that the only reason why a
// Hori Mini behaves like a Hori Mini is because of the feature bits, are we
// still emulating a Hori Mini if the feature bits got patched?)
// Anyway this seems to be a configuration that used by DS4 to determine what
// hardware are available on the controller (byte 5) and what type of controller
// it is (byte 6).
static const uint8_t replay_report_0x03[] = {
    0x03, 0x21, 0x27, 0x04, 0x4d, 0x00, 0x2c, 0x56,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x0d, 0x0d, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Response when PS4 asks for resetting the security chip used in authentication
// process.
static const uint8_t replay_report_0xf3[] = {
    0xf3, 0x00, 0x38, 0x38, 0x00, 0x00, 0x00, 0x00
};

// Used by usb_dev as a extended buffer
uint8_t usb_ds4_reply_buffer[64];
uint8_t usb_ds4_feature_buffer[64];
static uint8_t _auth_seq;
static bool _auth_challenge_available;
static bool _auth_challenge_sent;
static bool _auth_response_available;
static bool _auth_response_buffered;

void usb_ds4_report_init(ds4_report_t *report) {
    memset(report, 0, sizeof(ds4_report_t));
    report->type = 0x01;
    // Center the D-Pad
    DS4_DPAD_SET(report->buttons, DS4_DPAD_C);
    // Analog sticks
    report->analog_l_x = 0x80;
    report->analog_l_y = 0x80;
    report->analog_r_x = 0x80;
    report->analog_r_y = 0x80;
    // Ext
    report->state_ext = DS4_EXT_NULL;
    // Touch
    for (uint8_t i=0; i<3; i++) {
        report->frames[i].pos1 = DS4_TOUCH_POS_PACK(0, 0, 0, 0);
        report->frames[i].pos2 = DS4_TOUCH_POS_PACK(0, 0, 0, 0);
    }
    report->battery = 0xff;
    report->gyro_x = 0xffe7;
    report->gyro_y = 0x206e;
    report->gyro_z = 0x09d9;
    //report->padding[1] = 0x80;
}

void usb_ds4_auth_state_init(void) {
    _auth_seq = 0;
    _auth_challenge_available = false;
    _auth_challenge_sent = false;
    _auth_response_available = false;
    _auth_response_buffered = false;
}

struct setup_struct {
  union {
   struct {
	uint8_t bmRequestType;
	uint8_t bRequest;
   };
	uint16_t wRequestAndType;
  };
	uint16_t wValue;
	uint16_t wIndex;
	uint16_t wLength;
};

int usb_ds4_on_get_report(void *setup_ptr, uint8_t *data, uint32_t *len) {
    struct setup_struct setup = *((struct setup_struct *)setup_ptr);
    ds4_auth_t *resp = (ds4_auth_t *) data;
    ds4_auth_result_t *result = (ds4_auth_result_t *) data;
    switch (setup.wValue) {
        case 0x03f1: // getChallengeResponse
            debug_print("I: getChallengeResponse\n");
            _auth_challenge_sent = false;
            if (_auth_response_buffered) {
                memcpy(resp, &usb_ds4_feature_buffer, sizeof(ds4_auth_t));
            } else {
                debug_print("W: f1 off sync, feeding junk\n");
                memset(data, 0, sizeof(ds4_auth_t));
            }
            // allow user to send response if not reaching end of page
            if (resp->page < 0x12) {
                _auth_response_available = true;
            }
            _auth_response_buffered = false;
            *len = sizeof(ds4_auth_t);
            break;
        case 0x03f2: // challengeResponseAvailable
            debug_print("I: challengeResponseAvailable\n");
            _auth_challenge_sent = true;
            result->type = 0xf2;
            result->seq = _auth_seq;
            memset(result->padding, 0, 9);
            if (!_auth_response_buffered) {
                result->status = 0x10;
            } else {
                result->status = 0x00;
                _auth_challenge_sent = false;
            }
            result->crc32 = 0;
            *len = sizeof(ds4_auth_result_t);
            break;
        case 0x0303: // licensedGetHWConfig
            debug_print("I: licensedGetHWConfig\n");
            memcpy(data, replay_report_0x03, sizeof(replay_report_0x03));
            *len = sizeof(replay_report_0x03);
            break;
        case 0x03f3: // licensedResetAuth
            debug_print("I: licensedResetAuth\n");
            memcpy(data, replay_report_0xf3, sizeof(replay_report_0xf3));
            *len = sizeof(replay_report_0xf3);
            break;
        default:
            debug_print("W: Unknown get_report ");
            debug_phex16(setup.wValue);
            debug_print("\n");
            return 1;
    }
    debug_print("I: Get report OK\n");
    return 0;
}

int usb_ds4_on_set_report(void *setup_ptr, uint8_t *data) {
    struct setup_struct setup = *((struct setup_struct *)setup_ptr);
    ds4_auth_t *authbuf = (ds4_auth_t *) data;
    switch (setup.wValue) {
        case 0x03f0: // setChallenge
            debug_print("I: setChallenge\n");
            if (setup.wLength != sizeof(ds4_auth_t)) {
                debug_print("E: Packet len mismatch (");
                debug_phex16(sizeof(ds4_auth_t));
                debug_print(" != ");
                debug_phex16(setup.wLength);
                debug_print(")\n");
                return 1;
            } else if (authbuf->type != 0xf0) { // magic check
                debug_print("E: Invalid magic (0xf0 != ");
                debug_phex16(authbuf->type);
                debug_print(")\n");
                return 1;
            } else {
                if (_auth_seq != authbuf->seq || authbuf->page == 0) {
                    debug_print("I: clearing state\n");
                    usb_ds4_auth_state_init();
                    _auth_seq = authbuf->seq;
                }
                if (_auth_challenge_available) {
                    debug_print("W: f0 off sync, overwriting\n");
                }
                memcpy(&usb_ds4_feature_buffer, authbuf, sizeof(ds4_auth_t));
                _auth_challenge_available = true;
            }
            break;
        default:
            debug_print("W: Unknown set_report ");
            debug_phex16(setup.wValue);
            debug_print("\n");
            return 1;
    }
    debug_print("I: Set report OK\n");
    return 0;
}

// Ported from usb_rawhid.c
static int usb_ds4_recv(void *buffer, uint32_t timeout)
{
	usb_packet_t *rx_packet;
	uint32_t begin = millis();

	while (1) {
		if (!usb_configuration) return -1;
		rx_packet = usb_rx(DS4_RX_ENDPOINT);
		if (rx_packet) break;
		if (millis() - begin > timeout || !timeout) return 0;
		yield();
	}
	memcpy(buffer, rx_packet->buf, DS4_RX_SIZE);
	usb_free(rx_packet);
	return DS4_RX_SIZE;
}

static int usb_ds4_available(void)
{
	uint32_t count;

	if (!usb_configuration) return 0;
	count = usb_rx_byte_count(DS4_RX_ENDPOINT);
	return count;
}

static int usb_ds4_send(const void *buffer, uint16_t len, uint32_t timeout, bool async) {
	usb_packet_t *tx_packet;
	uint32_t begin = millis();

	while (1) {
		if (!usb_configuration) return -1;
		if (usb_tx_packet_count(DS4_TX_ENDPOINT) < TX_PACKET_LIMIT) {
			tx_packet = usb_malloc();
			if (tx_packet) break;
		}
		if (async) return 0;
		if (millis() - begin > timeout) {
		    debug_print("send: timeout\n");
		    return 0;
		}
		yield();
	}
	memcpy(tx_packet->buf, buffer, len);
	tx_packet->len = len;
	usb_tx(DS4_TX_ENDPOINT, tx_packet);
	//debug_print("send: enqueued len=");
	//debug_phex16(len);
	//debug_print("\n");
	return len;
}

int usb_ds4_send_report(const ds4_report_t *report, bool async) {
    int result = usb_ds4_send((const void *) report, sizeof(ds4_report_t), DS4_PACKET_TIMEOUT, async);
    if (result == sizeof(ds4_report_t)) {
        return 0;
    } else if (result == -1) {
        return -1;
    } else {
        return 1;
    }
}

int usb_ds4_recv_feedback(ds4_feedback_t *feedback) {
    int available = usb_ds4_available();
    uint8_t recv_buffer[DS4_RX_SIZE];
    if (available != 0) {
        debug_phex32(available);
        debug_print("\n");
    }
    if (available >= (int) sizeof(ds4_feedback_t)) {
        int result = usb_ds4_recv((void *) &recv_buffer, DS4_PACKET_TIMEOUT);
        if (result > 0) {
            if (recv_buffer[0] == 0x05) {
                debug_print("I: report ok\n");
                memcpy((void *) feedback, (const void *) recv_buffer, sizeof(ds4_feedback_t));
                for (int i=0; i<32; i++) {
                    debug_phex(recv_buffer[i]);
                    if (i == 15) {
                        debug_print("\n");
                    } else {
                        debug_print(" ");
                    }
                }
                debug_print("\n");
            } else {
                debug_print("E: unknown OUT report");
                debug_phex(recv_buffer[0]);
                debug_print("\n");
                return 1;
            }
        } else if (result == 0) {
            debug_print("recv: timeout\n");
            return 1;
        }
    } else {
        if (available > 0) {
            debug_print("W: recv: underflow len=");
            debug_phex32(available);
            debug_print("\n");
            return 1;
        }
    }
    return 0;
}

bool usb_ds4_class::authChallengeAvailable(void) {
    return _auth_challenge_available;
}

const ds4_auth_t *usb_ds4_class::authGetChallenge(void) const {
    _auth_challenge_available = false;
    return (const ds4_auth_t *) &usb_ds4_feature_buffer;
}

// TODO rename this to authCheckNeeded or so
bool usb_ds4_class::authChallengeSent(void) {
    if (_auth_challenge_sent) {
        _auth_challenge_sent = false;
        return true;
    } else {
        return false;
    }
}

ds4_auth_t *usb_ds4_class::authGetResponseBuffer(void) {
    return (ds4_auth_t *) &usb_ds4_feature_buffer;
}

void usb_ds4_class::authSetBufferedFlag(void) {
    _auth_response_buffered = true;
    // block user from sending more responses
    _auth_response_available = false;
}

bool usb_ds4_class::authResponseAvailable(void) {
    return _auth_response_available;
}

#endif // F_CPU >= 20000000
#endif // DS4_INTERFACE
