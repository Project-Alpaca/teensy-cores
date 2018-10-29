/* Teensyduino Core Library
 * http://www.pjrc.com/teensy/
 * Copyright (c) 2017 PJRC.COM, LLC.
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

#ifndef __USB_DS4_H__
#define __USB_DS4_H__

#include "usb_desc.h"

#ifdef DS4_INTERFACE

#include <inttypes.h>
#include <string.h>
#include <HardwareSerial.h>
#include <core_pins.h> // for millis()

// D-Pad positions
#define DS4_DPAD_N 0
#define DS4_DPAD_NE 1
#define DS4_DPAD_E 2
#define DS4_DPAD_SE 3
#define DS4_DPAD_S 4
#define DS4_DPAD_SW 5
#define DS4_DPAD_W 6
#define DS4_DPAD_NW 7
#define DS4_DPAD_C 8

// Buttons
#define DS4_BTN_SQUARE 4
#define DS4_BTN_CROSS 5
#define DS4_BTN_CIRCLE 6
#define DS4_BTN_TRIANGLE 7
#define DS4_BTN_L1 8
#define DS4_BTN_R1 9
#define DS4_BTN_L2 10
#define DS4_BTN_R2 11
#define DS4_BTN_SHARE 12
#define DS4_BTN_OPTION 13
#define DS4_BTN_L3 14
#define DS4_BTN_R3 15
#define DS4_BTN_PS 16
#define DS4_BTN_TOUCH 17

// ext
//#define DS4_EXT_NULL 0x1b
#define DS4_EXT_NULL 0x08

typedef struct {
    uint8_t type; // 0
    uint8_t seq; // 1
    uint8_t page; // 2
    uint8_t sbz; // 3
    uint8_t data[56]; // 4-59
    uint32_t crc32; // 60-63
} __attribute__((packed)) ds4_auth_t;

// TODO: verify
typedef struct {
    uint8_t type; // 0
    uint8_t seq; // 1
    uint8_t status; // 2  0x10 = not ready, 0x00 = ready
    uint8_t padding[9]; // 3-11
    uint32_t crc32; // 12-15
} __attribute__((packed)) ds4_auth_result_t;

typedef struct {
    uint8_t seq; // 34
    uint32_t pos1; // 35-38
    uint32_t pos2; // 39-42
} __attribute__((packed)) ds4_touch_frame_t;

typedef struct {
    uint8_t type; // 0
    uint8_t analog_l_x; // 1
    uint8_t analog_l_y; // 2
    uint8_t analog_r_x; // 3
    uint8_t analog_r_y; // 4
    uint8_t buttons[3]; // 5-7
    uint8_t trigger_l; // 8
    uint8_t trigger_r; // 9
    uint16_t sensor_timestamp; // 10-11
    uint8_t battery; // 12
    uint8_t u13; // 13
    int16_t accel_z; // 14-15
    int16_t accel_y; // 16-17
    int16_t accel_x; // 18-19
    int16_t gyro_x; // 20-21
    int16_t gyro_y; // 22-23
    int16_t gyro_z; // 24-25
    uint32_t u26; // 26-29
    uint8_t state_ext; // 30
    uint16_t u31; // 31-32
    uint8_t tp_available_frame; // 33
    ds4_touch_frame_t frames[3]; // 34-60
    uint8_t padding[3]; // 61-62 (63?)
} __attribute__((packed)) ds4_report_t;

typedef struct {
    uint8_t type; // 0
    uint8_t flags; // 1
    uint8_t padding1[2]; // 2-3
    uint8_t rumble_right; // 4
    uint8_t rumble_left; // 5
    uint8_t led_color[3]; // 6-8
    uint8_t led_flash_on; // 9
    uint8_t led_flash_off; // 10
    uint8_t padding[20]; // 11-30
} __attribute__((packed)) ds4_feedback_t;

#define DS4_DPAD_SET(buttons, dir) \
    buttons[0] ^= buttons[0] & 0x0f; \
    buttons[0] |= dir & 0x0f;

#define DS4_GET_SENSOR_TS() ((millis() * 150) & 0xffff)

#define DS4_BTN_SET(buttons, btn_id) \
    buttons[(btn_id >> 3) & 3] |= 1 << (btn_id & 7);

#define DS4_BTN_CLR(buttons, btn_id) \
    buttons[(btn_id >> 3) & 3] &= ~(1 << (btn_id & 7));

#define DS4_BTN_RESET(buttons) \
    buttons[0] ^= buttons[0] & 0xf0; \
    buttons[1] ^= buttons[1] & 0xff; \
    buttons[2] ^= buttons[2] & 0x03;

#define DS4_BTN_CTR_INC(buttons) buttons[2] += 4;
#define DS4_BTN_CTR_RESET(buttons) buttons[2] ^= buttons[2] & 0xfc;

// Format: yyyyyyyyyyyyxxxxxxxxxxxxtiiiiiii
#define DS4_TOUCH_GET_STATE(tp) ((~(tp >> 7)) & 1)
#define DS4_TOUCH_GET_ID(tp) (tp & 0x7f)
#define DS4_TOUCH_GET_X(tp) ((tp >> 8) & 0xfff)
#define DS4_TOUCH_GET_Y(tp) ((tp >> 20) & 0xfff)
#define DS4_TOUCH_RELEASE(tp) tp &= 0xffffff7f

#define DS4_TOUCH_POS_UNPACK(tp) \
    DS4_TOUCH_GET_STATE(tp), DS4_TOUCH_GET_ID(tp), DS4_TOUCH_GET_X(tp), DS4_TOUCH_GET_Y(tp)
#define DS4_TOUCH_POS_PACK(touch, track_id, x, y) \
    ((y & 0xfff) << 20) | ((x & 0xfff) << 8) | (((~touch) & 1) << 7) | (track_id & 0x7f)

// C language implementation
#ifdef __cplusplus
extern "C" {
#endif

// buffers
extern uint8_t usb_ds4_reply_buffer[];
//extern ds4_report_t usb_ds4_report_buffer;

// C function prototypes
extern int usb_ds4_send_report(const ds4_report_t *report, bool async);
extern int usb_ds4_recv_feedback(ds4_feedback_t *feedback);

extern int usb_ds4_on_set_report(void *setup_ptr, uint8_t *data);
extern int usb_ds4_on_get_report(void *setup_ptr, uint8_t *data, uint32_t *len);

extern void usb_ds4_auth_state_init(void);

extern void usb_ds4_report_init(ds4_report_t *report);

#ifdef __cplusplus
}
#endif

// C++ interface
#ifdef __cplusplus
class usb_ds4_class {
// C++ function prototypes

public:
    usb_ds4_class(void) { return; } // initialize procedure will be handled in begin()
    void begin(void) {
        usb_ds4_report_init(&reportBuffer);
        memset(&feedbackBuffer, 0, sizeof(ds4_feedback_t));
        pointCtr = 0;
    }
    bool send(bool async) {
        reportBuffer.sensor_timestamp = DS4_GET_SENSOR_TS();
        if (usb_ds4_send_report(&reportBuffer, async) == 0) {
            DS4_BTN_CTR_INC(reportBuffer.buttons);
            return true;
        }
        return false;
    }
    bool send(void) {
        return send(false);
    }
    bool sendAsync(void) {
        return send(true);
    }
    void update(void) {
        usb_ds4_recv_feedback(&feedbackBuffer);
    }
    void pressButton(uint8_t buttonId) {
        DS4_BTN_SET(reportBuffer.buttons, buttonId);
    }
    void releaseButton(uint8_t buttonId) {
        DS4_BTN_CLR(reportBuffer.buttons, buttonId);
    }
    void releaseAllButton(void) {
        DS4_BTN_RESET(reportBuffer.buttons);
    }
    void pressDpad(uint8_t pos) {
        DS4_DPAD_SET(reportBuffer.buttons, pos);
        //serial_print("buttons: ");
        //serial_phex(reportBuffer.buttons[0]);
        //serial_phex(reportBuffer.buttons[1]);
        //serial_phex(reportBuffer.buttons[2]);
        //serial_print("\n");
    }
    void releaseDpad(void) {
        DS4_DPAD_SET(reportBuffer.buttons, DS4_DPAD_C);
    }

    void setLeftAnalog(uint8_t x, uint8_t y) {
        reportBuffer.analog_l_x = x;
        reportBuffer.analog_l_y = y;
    }

    void setRightAnalog(uint8_t x, uint8_t y) {
        reportBuffer.analog_r_x = x;
        reportBuffer.analog_r_y = y;
    }

    void setTouchPos1(uint16_t x, uint16_t y) {
        uint8_t pointTmp = DS4_TOUCH_GET_ID(reportBuffer.frames[0].pos1);
        uint8_t touch = DS4_TOUCH_GET_STATE(reportBuffer.frames[0].pos1);
        // If updating coordinates, do not bump the point id, otherwise bump it
        if (touch) {
            reportBuffer.frames[0].pos1 = DS4_TOUCH_POS_PACK(1, pointTmp, x, y);
        } else {
            reportBuffer.frames[0].pos1 = DS4_TOUCH_POS_PACK(1, pointCtr, x, y);
        }
        reportBuffer.frames[0].seq++;
    }

    void setTouchPos2(uint16_t x, uint16_t y) {
        uint8_t pointTmp = DS4_TOUCH_GET_ID(reportBuffer.frames[0].pos2);
        uint8_t touch = DS4_TOUCH_GET_STATE(reportBuffer.frames[0].pos2);
        // If updating coordinates, do not bump the point id, otherwise bump it
        if (touch) {
            reportBuffer.frames[0].pos2 = DS4_TOUCH_POS_PACK(1, pointTmp, x, y);
        } else {
            reportBuffer.frames[0].pos2 = DS4_TOUCH_POS_PACK(1, pointCtr, x, y);
        }
        reportBuffer.frames[0].seq++;
    }

    void releaseTouchPos1(void) {
        pointCtr += DS4_TOUCH_GET_STATE(reportBuffer.frames[0].pos1);
        DS4_TOUCH_RELEASE(reportBuffer.frames[0].pos1);
        reportBuffer.frames[0].seq++;
    }

    void releaseTouchPos2(void) {
        pointCtr += DS4_TOUCH_GET_STATE(reportBuffer.frames[0].pos2);
        DS4_TOUCH_RELEASE(reportBuffer.frames[0].pos2);
        reportBuffer.frames[0].seq++;
    }

    void releaseTouchAll(void) {
        pointCtr += DS4_TOUCH_GET_STATE(reportBuffer.frames[0].pos1);
        DS4_TOUCH_RELEASE(reportBuffer.frames[0].pos1);
        pointCtr += DS4_TOUCH_GET_STATE(reportBuffer.frames[0].pos2);
        DS4_TOUCH_RELEASE(reportBuffer.frames[0].pos2);
        pointCtr++;
        reportBuffer.frames[0].seq++;
    }

    // Run these in critical section to prevent race conditions
    // true if there's new challenge, false otherwise
    bool authChallengeAvailable(void);
    // returns the main buffer, clears new flag
    const ds4_auth_t *authGetChallenge(void) const;
    // true if the PS4 sent the challenge and starts to check if the response is
    // ready.
    bool authChallengeSent(void);
    // buffer the response
    ds4_auth_t *authGetResponseBuffer(void);
    // adds a buffered flag if called, and gets cleared
    // when PS4 asks for the response
    void authSetBufferedFlag(void);
    // true if PS4 asked and we can send the response, false otherwise
    bool authResponseAvailable(void);

private:
    ds4_report_t reportBuffer;
    ds4_feedback_t feedbackBuffer;
    uint8_t pointCtr;
};

extern usb_ds4_class DS4;

#endif // __cplusplus

#endif /* DS4_INTERFACE */
#endif /* __USB_DS4_H__ */
