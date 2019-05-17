/*
    Copyright 2016 fishpepper <AT> gmail.com

    This program is free software: you can redistribute it and/ or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http:// www.gnu.org/ licenses/>.

    author: sanper <AT> gmail.com
*/

#include "main.h"
#include <string.h>
#include <stdio.h>
#include "hubsan.h"
#include "debug.h"
#include "timeout.h"
#include "led.h"
#include "delay.h"
#include "wdt.h"
#include "a7105.h"
#include "io.h"
#include "storage.h"
#include "adc.h"
#include "telemetry.h"

#include <libopencm3/stm32/timer.h>

static uint8_t packet[16];
static uint8_t channel;
static uint8_t option;
static int16_t vtx_freq;
static const uint8_t allowed_ch[] = {0x14, 0x1e, 0x28, 0x32,
                                     0x3c, 0x46, 0x50, 0x5a,
                                     0x64, 0x6e, 0x78, 0x82};
static uint32_t sessionid;
static const uint32_t txid = 0xdb042679;
static uint8_t state;
static uint8_t packet_count;
static uint8_t bind_count;
static uint32_t id_data;

static uint8_t hubsan_init(void) {
    a7105_init();
    return 0;
}

static void hubsan_update_crc(void) {
    uint8_t i;
    uint8_t sum = 0;
    for (i = 0; i < 15; i++)
        sum += packet[i];
    packet[15] = (256 - (sum % 256)) & 0xFF;
}

static void hubsan_build_bind_packet(uint8_t bind_state) {
    static uint8_t handshake_counter;

    if ( state < BIND_7 )
        handshake_counter = 0;
    memset(packet, 16, 0);
    packet[0] = bind_state;
    packet[1] = channel;
    packet[2] = (sessionid >> 24) & 0xFF;
    packet[3] = (sessionid >> 16) & 0xFF;
    packet[4] = (sessionid >> 8) & 0xFF;
    packet[5] = (sessionid >> 0) & 0xFF;
    if (id_data == ID_NORMAL) {
        packet[6] = 0x08;
        packet[7] = 0xe4;
        packet[8] = 0xea;
        packet[9] = 0x9e;
        packet[10] = 0x50;
        packet[11] = (txid >> 24) & 0xFF;
        packet[12] = (txid >> 16) & 0xFF;
        packet[13] = (txid >> 8) & 0xFF;
        packet[14] = (txid >> 0) & 0xFF;
    } else {
        if (state > BIND_3) {
            packet[7] = 0x62;
            packet[8] = 0x16;
        }
        if (state == BIND_7)
            packet[2] = handshake_counter++;
    }
    hubsan_update_crc();
}

static void hubsan_build_packet(void) {
    static uint8_t vtx_freq = 0;

    memset(packet, 16, 0);
    if ( vtx_freq != option || packet_count == 100 ) {
        vtx_freq = option;
        packet[0] = 0x40;
        packet[1] = (vtx_freq > 0xF2) ? 0x17 : 0x16;
        packet[2] = vtx_freq + 0x0D;
        packet[3] = 0x82;
        packet_count++;
    } else {
        packet[0] = 0x20;
    }
}

static uint8_t hubsan_check_integrity(void) {
    uint8_t i;
    uint8_t sum = 0;

    for ( i = 0; i < 15; i++)
        sum += packet[i];
    return packet[15] == ((256 - (sum % 256)) & 0xFF);
}

static void hubsan_update_telemetry(void) {}

static void hubsan_enter_bindmode(void) {}
