/*
    Copyright 2016 fishpepper <AT> gmail.com

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    author: fishpepper <AT> gmail.com
*/

#ifndef HUBSAN_H_
#define HUBSAN_H_

#include <stdint.h>

#include "main.h"
#include "a7105.h"

// For code readability
enum {
    CHANNEL1 = 0,  // Aileron
    CHANNEL2,      // Elevator
    CHANNEL3,      // Throttle
    CHANNEL4,      // Rudder
    CHANNEL5,      // Leds
    CHANNEL6,      // Flip (or Altitude Hold on H501S, Stabilized mode on/off on H301)
    CHANNEL7,      // Still camera
    CHANNEL8,      // Video camera
    CHANNEL9,      // Headless
    CHANNEL10,     // RTH
    CHANNEL11,     // GPS HOLD
};

#define CHANNEL_LED         CHANNEL5
#define CHANNEL_FLIP        CHANNEL6
#define CHANNEL_ALT_HOLD    CHANNEL6
#define CHANNEL_STAB        CHANNEL6
#define CHANNEL_SNAPSHOT    CHANNEL7
#define CHANNEL_VIDEO       CHANNEL8
#define CHANNEL_HEADLESS    CHANNEL9
#define CHANNEL_RTH         CHANNEL10
#define CHANNEL_GPS_HOLD    CHANNEL11

enum{
    // flags going to packet[9] (H107)
    FLAG_VIDEO = 0x01,   // record video
    FLAG_FLIP  = 0x08,   // enable flips
    FLAG_LED   = 0x04    // enable LEDs
};

enum{
    // flags going to packet[9] (H107 Plus series)
    FLAG_HEADLESS = 0x08,  // headless mode
};

enum{
    // flags going to packet[13] (H107 Plus series)
    FLAG_SNAPSHOT  = 0x01,
    FLAG_FLIP_PLUS = 0x80,
};

#define VTX_STEP_SIZE "5"

enum {
    TELEM_ON = 0,
    TELEM_OFF,
};

#define ID_NORMAL 0x55201041  // H102D, H107/L/C/D, H301, H501S
#define ID_PLUS   0xAA201041  // H107P/C+/D+

enum {
    BIND_1,
    BIND_2,
    BIND_3,
    BIND_4,
    BIND_5,
    BIND_6,
    BIND_7,
    BIND_8,
    DATA_1,
    DATA_2,
    DATA_3,
    DATA_4,
    DATA_5,
};
#define WAIT_WRITE 0x80

static uint8_t hubsan_init(void);
static void hubsan_update_crc(void);
static void hubsan_build_bind_packet(uint8_t bind_state);
static void hubsan_build_packet(void);
static uint8_t hubsan_check_integrity(void);
static void hubsan_update_telemetry(void);
static void hubsan_enter_bindmode(void);

#endif  // HUBSAN_H_
