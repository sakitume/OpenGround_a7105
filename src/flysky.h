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

#ifndef FLYSKY_H_
#define FLYSKY_H_

#include <stdint.h>

#include "main.h"
#include "a7105.h"

#define BIND_COUNT 2500

#define PACKET_PERIOD_FLYSKY 1510UL
#define PACKET_PERIOD_CX20 3984UL

static const char * const flysky_opts[] = {
  "WLToys ext.",  _tr_noop("Off"), "V9x9", "V6x6", "V912", "CX20", NULL,
  "Freq Tune", "-300", "300", "655361", NULL, // big step 10, little step 1
  NULL
};
enum {
    PROTOOPTS_WLTOYS = 0,
    PROTOOPTS_FREQTUNE,
    LAST_PROTO_OPT,
};
ctassert(LAST_PROTO_OPT <= NUM_PROTO_OPTS, too_many_protocol_opts);

#define WLTOYS_EXT_OFF 0
#define WLTOYS_EXT_V9X9 1
#define WLTOYS_EXT_V6X6 2
#define WLTOYS_EXT_V912 3
#define WLTOYS_EXT_CX20 4

FLASHBYTETABLE A7105_regs[] = {
    0xFF, 0x42, 0x00, 0x14, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x01, 0x21, 0x05, 0x00, 0x50,
    0x9e, 0x4b, 0x00, 0x02, 0x16, 0x2b, 0x12, 0x00, 0x62, 0x80, 0x80, 0x00, 0x0a, 0x32, 0xc3, 0x0f,
    0x13, 0xc3, 0x00, 0xFF, 0x00, 0x00, 0x3b, 0x00, 0x17, 0x47, 0x80, 0x03, 0x01, 0x45, 0x18, 0x00,
    0x01, 0x0f, 0xFF,
};
FLASHBYTETABLE tx_channels[8][4] = {
    { 0x12, 0x34, 0x56, 0x78},
    { 0x18, 0x27, 0x36, 0x45},
    { 0x41, 0x82, 0x36, 0x57},
    { 0x84, 0x13, 0x65, 0x72},
    { 0x87, 0x64, 0x15, 0x32},
    { 0x76, 0x84, 0x13, 0x52},
    { 0x71, 0x62, 0x84, 0x35},
    { 0x71, 0x86, 0x43, 0x52}
};

// For code readability
enum {
    CHANNEL1 = 0,
    CHANNEL2,
    CHANNEL3,
    CHANNEL4,
    CHANNEL5,
    CHANNEL6,
    CHANNEL7,
    CHANNEL8,
    CHANNEL9,
    CHANNEL10,
    CHANNEL11,
    CHANNEL12,
};

enum {
    // flags going to byte 10
    FLAG_V9X9_VIDEO = 0x40,
    FLAG_V9X9_CAMERA= 0x80,
    // flags going to byte 12
    FLAG_V9X9_UNK   = 0x10, // undocumented ?
    FLAG_V9X9_LED   = 0x20,
};

enum {
    // flags going to byte 13
    FLAG_V6X6_HLESS1= 0x80,
    // flags going to byte 14
    FLAG_V6X6_VIDEO = 0x01,
    FLAG_V6X6_YCAL  = 0x02,
    FLAG_V6X6_XCAL  = 0x04,
    FLAG_V6X6_RTH   = 0x08,
    FLAG_V6X6_CAMERA= 0x10,
    FLAG_V6X6_HLESS2= 0x20,
    FLAG_V6X6_LED   = 0x40,
    FLAG_V6X6_FLIP  = 0x80,
};

enum {
    // flags going to byte 14
    FLAG_V912_TOPBTN= 0x40,
    FLAG_V912_BTMBTN= 0x80,
};

void flysky_init(void);
void flysky_init_timer(void);
void flysky_tx_set_enabled(uint32_t enabled);
void TIM3_IRQHandler(void);
void flysky_apply_extension_flags(void);
void flysky_build_packet(uint8_t init);


#endif  // FLYSKY_H_
