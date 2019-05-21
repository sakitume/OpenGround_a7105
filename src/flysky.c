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

    author: fishpepper <AT> gmail.com
*/

#include "main.h"
#include <string.h>
#include <stdio.h>
#include "flysky.h"
#include "debug.h"
#include "timeout.h"
#include "led.h"
#include "delay.h"
#include "wdt.h"
#include "a7105.h"
#include "io.h"
#include "clocksource.h"
#include "storage.h"
#include "adc.h"
#include "telemetry.h"

#include <libopencm3/stm32/timer.h>


static uint32_t id;
static uint8_t packet[21];
static uint16_t counter;
static uint16_t packet_period;
static uint8_t hopping_frequency[16];
static uint8_t hopping_frequency_no;
static uint8_t tx_power;
static int16_t freq_offset;

void flysky_init(void) {
    // uint8_t i;
    debug("flysky: init\n"); debug_flush();

    telemetry_init();

    a7105_init();

    frsky_link_quality = 0;
    frsky_diversity_count = 0;
    frsky_packet_received = 0;
    frsky_packet_sent = 0;
    frsky_bind_packet_received = 0;

    frsky_rssi = 100;

    // check if spi is working properly
    if (!a7105_check_transceiver()) {
        // no a7105 detected - abort
        debug("frsky: no a7105 detected. abort\n");
        debug_flush();
        return;
    }

    // init frsky registersttings for cc2500
    frsky_configure();

    // show info:
    debug("frsky: using txid 0x"); debug_flush();
    debug_put_hex8(storage.frsky_txid[0]);
    debug_put_hex8(storage.frsky_txid[1]);
    debug_put_newline();

    // init txid matching
    frsky_configure_address();

    // tune cc2500 pll and save the values to ram
    frsky_calib_pll();

    // initialise 9ms timer isr
    frsky_tx_enabled = 0;
    frsky_init_timer();

    frsky_tx_set_enabled(1);

    debug("frsky: init done\n"); debug_flush();
}

static int flysky_init()
{
    int i;
    uint8_t if_calibration1;
    uint8_t vco_calibration0;
    uint8_t vco_calibration1;
    uint8_t reg;

    A7105_WriteID(0x5475c52a);
    for (i = 0; i < 0x33; i++)
    {
        reg = pgm_read_byte(&A7105_regs[i]);
        if(Model.proto_opts[PROTOOPTS_WLTOYS] == WLTOYS_EXT_CX20) {
            if(i==0x0E) reg=0x01;
            if(i==0x1F) reg=0x1F;
            if(i==0x20) reg=0x1E;
        }
        if( reg != 0xFF)
            A7105_WriteReg(i, reg);
    }
    A7105_Strobe(A7105_STANDBY);

    //IF Filter Bank Calibration
    A7105_WriteReg(0x02, 1);
    A7105_ReadReg(0x02);
    u32 ms = CLOCK_getms();
    CLOCK_ResetWatchdog();
    while(CLOCK_getms()  - ms < 500) {
        if(! A7105_ReadReg(0x02))
            break;
    }
    if (CLOCK_getms() - ms >= 500)
        return 0;
    if_calibration1 = A7105_ReadReg(0x22);
    if(if_calibration1 & A7105_MASK_FBCF) {
        //Calibration failed...what do we do?
        return 0;
    }

    //VCO Current Calibration
    A7105_WriteReg(0x24, 0x13); //Recomended calibration from A7105 Datasheet

    //VCO Bank Calibration
    A7105_WriteReg(0x26, 0x3b); //Recomended limits from A7105 Datasheet

    //VCO Bank Calibrate channel 0?
    //Set Channel
    A7105_WriteReg(0x0f, 0); //Should we choose a different channel?
    //VCO Calibration
    A7105_WriteReg(0x02, 2);
    ms = CLOCK_getms();
    CLOCK_ResetWatchdog();
    while(CLOCK_getms()  - ms < 500) {
        if(! A7105_ReadReg(0x02))
            break;
    }
    if (CLOCK_getms() - ms >= 500)
        return 0;
    vco_calibration0 = A7105_ReadReg(0x25);
    if (vco_calibration0 & A7105_MASK_VBCF) {
        //Calibration failed...what do we do?
        return 0;
    }

    //Calibrate channel 0xa0?
    //Set Channel
    A7105_WriteReg(0x0f, 0xa0); //Should we choose a different channel?
    //VCO Calibration
    A7105_WriteReg(0x02, 2);
    ms = CLOCK_getms();
    CLOCK_ResetWatchdog();
    while(CLOCK_getms()  - ms < 500) {
        if(! A7105_ReadReg(A7105_02_CALC))
            break;
    }
    if (CLOCK_getms() - ms >= 500)
        return 0;
    vco_calibration1 = A7105_ReadReg(0x25);
    if (vco_calibration1 & A7105_MASK_VBCF) {
        //Calibration failed...what do we do?
        return 0;
    }

    //Reset VCO Band calibration
    A7105_WriteReg(0x25, 0x08);

    A7105_SetTxRxMode(TX_EN);
    A7105_SetPower(Model.tx_power);

    A7105_Strobe(A7105_STANDBY);
    return 1;
}

void flysky_init_timer(void) {
    // TIM3 clock enable
    rcc_periph_clock_enable(RCC_TIM3);

    // init timer3 for 9ms
    timer_reset(TIM3);

    // enable the TIM3 gloabal Interrupt
    nvic_enable_irq(NVIC_TIM3_IRQ);
    nvic_set_priority(NVIC_TIM3_IRQ, NVIC_PRIO_FRSKY);

    // compute prescaler value
    // we want one ISR every 9ms
    // setting TIM_Period to 9000 will reuqire
    // a prescaler so that one timer tick is 1us (1MHz)
    uint16_t prescaler = (uint16_t) (rcc_timer_frequency  / 1000000) - 1;

    // time base as calculated above
    timer_set_prescaler(TIM3, prescaler);
    timer_set_mode(TIM3, TIM_CR1_CKD_CK_INT, TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);
    // timer should count with 1MHz thus 9000 ticks = 9ms
    timer_set_period(TIM3, 9000-1);

    // DO NOT ENABLE INT yet!

    // enable timer
    timer_enable_counter(TIM3);
}

void flysky_tx_set_enabled(uint32_t enabled) {
    // TIM Interrupts enable? -> tx active
    if (enabled) {
        frsky_frame_counter = 0;
        frsky_state = 0;
        // enable ISR
        timer_enable_irq(TIM3, TIM_DIER_UIE);
    } else {
        // stop ISR
        timer_disable_irq(TIM3, TIM_DIER_UIE);
        // make sure last packet was sent
        delay_ms(20);
    }
}

void TIM3_IRQHandler(void) {
    if (timer_get_flag(TIM3, TIM_SR_UIF)) {
        // clear flag (NOTE: this should never be done at the end of the ISR)
        timer_clear_flag(TIM3, TIM_SR_UIF);

        // when will there be the next isr?
        switch (frsky_state) {
            default:
            case (0) :
                // process any incoming telemetry data
                frsky_receive_packet();
                // hop to next channel
                frsky_increment_channel(1);
                // send data
                frsky_send_packet();
                frsky_frame_counter++;
                // next hop in 9ms
                timer_set_period(TIM3, 9000-1);
                frsky_state = 1;
                break;

            case (1) :
                // hop to next channel
                frsky_increment_channel(1);
                // send data
                frsky_send_packet();
                frsky_frame_counter++;
                // next hop in 9ms
                timer_set_period(TIM3, 9000-1);
                frsky_state = 2;
                break;

            case (2) :
                // hop to next channel
                frsky_increment_channel(1);
                // send data
                frsky_send_packet();
                frsky_frame_counter++;
                // after this tx we expect rx data,
                // TX is finished after ~7.2ms -> next isr in 7.5ms
                timer_set_period(TIM3, 7500-1);
                frsky_state = 3;
                break;

            case (3) :
                // prepare for data receiption, hop to next channel
                frsky_increment_channel(1);
                // enable LNA
                //cc2500_enter_rxmode();
                // the next hop will now in 1.3ms (after freq stabilised)
                timer_set_period(TIM3, 1300-1-0*200);
                frsky_state = 4;
                break;

            case (4) :
                // now go to RX mode
                // go to idle
                //cc2500_strobe(RFST_SRX);
                // increment framecounter
                frsky_frame_counter++;
                // the next hop will now in 9.2ms
                // 2*9 = 18 = 7.5 + 1.3 + 9.2
                timer_set_period(TIM3, 9200-1+0*200);
                frsky_state = 0;
                break;

            case (0x80) :
                // bind mode, set timeout to 9ms
                timer_set_period(TIM3, 9000);

                // get bind packet index
                frsky_frame_counter++;
                if ((frsky_frame_counter * 5) > FRSKY_HOPTABLE_SIZE) {
                    frsky_frame_counter = 0;
                }

                // send bind packet
                frsky_send_bindpacket(frsky_frame_counter);
                frsky_state = 0x80;
                break;
        }
    }
}


static void flysky_apply_extension_flags(void)
{
    const uint8_t V912_X17_SEQ[10] = { 0x14, 0x31, 0x40, 0x49, 0x49,    // sometime first byte is 0x15 ?
                                  0x49, 0x49, 0x49, 0x49, 0x49, }; 
    static uint8_t seq_counter;
    switch(Model.proto_opts[PROTOOPTS_WLTOYS]) {
        case WLTOYS_EXT_V9X9:
            if(Channels[CHANNEL5] > 0)
                packet[12] |= FLAG_V9X9_LED;
            if(Channels[CHANNEL6] > 0)
                packet[10] |= FLAG_V9X9_VIDEO;
            if(Channels[CHANNEL7] > 0)
                packet[10] |= FLAG_V9X9_CAMERA;
            if(Channels[CHANNEL8] > 0)
                packet[12] |= FLAG_V9X9_UNK;
            break;
            
        case WLTOYS_EXT_V6X6:
            packet[13] = 0x03; // 3 = 100% rate (0=40%, 1=60%, 2=80%)
            packet[14] = 0x00;
            if(Channels[CHANNEL5] > 0) 
                packet[14] |= FLAG_V6X6_LED;
            if(Channels[CHANNEL6] > 0) 
                packet[14] |= FLAG_V6X6_FLIP;
            if(Channels[CHANNEL7] > 0) 
                packet[14] |= FLAG_V6X6_CAMERA;
            if(Channels[CHANNEL8] > 0) 
                packet[14] |= FLAG_V6X6_VIDEO;
            if(Model.num_channels >= 9 && Channels[CHANNEL9] > 0) { 
                packet[13] |= FLAG_V6X6_HLESS1;
                packet[14] |= FLAG_V6X6_HLESS2;
            }
            if(Model.num_channels >= 10 && Channels[CHANNEL10] > 0) 
                packet[14] |= FLAG_V6X6_RTH;
            if(Model.num_channels >= 11 && Channels[CHANNEL11] > 0) 
                packet[14] |= FLAG_V6X6_XCAL;
            if(Model.num_channels >= 12 && Channels[CHANNEL12] > 0) 
                packet[14] |= FLAG_V6X6_YCAL;
            packet[15] = 0x10; // unknown
            packet[16] = 0x10; // unknown
            packet[17] = 0xAA; // unknown
            packet[18] = 0xAA; // unknown
            packet[19] = 0x60; // unknown, changes at irregular interval in stock TX
            packet[20] = 0x02; // unknown
            break;
            
        case WLTOYS_EXT_V912:
            seq_counter++;
            if( seq_counter > 9)
                seq_counter = 0;
            packet[12] |= 0x20; // bit 6 is always set ?
            packet[13] = 0x00;  // unknown
            packet[14] = 0x00;
            if(Channels[CHANNEL5] > 0)
                packet[14] |= FLAG_V912_BTMBTN;
            if(Channels[CHANNEL6] > 0)
                packet[14] |= FLAG_V912_TOPBTN;
            packet[15] = 0x27; // [15] and [16] apparently hold an analog channel with a value lower than 1000
            packet[16] = 0x03; // maybe it's there for a pitch channel for a CP copter ?
            packet[17] = V912_X17_SEQ[seq_counter]; // not sure what [17] & [18] are for
            if(seq_counter == 0)                    // V912 Rx does not even read those bytes... [17-20]
                packet[18] = 0x02;
            else
                packet[18] = 0x00;
            packet[19] = 0x00; // unknown
            packet[20] = 0x00; // unknown
            break;
            
        case WLTOYS_EXT_CX20:
            packet[19] = 00; // unknown
            packet[20] = (hopping_frequency_no<<4)|0x0A;
            break;
            
        default:
            break; 
    }
}

static void flysky_build_packet(uint8_t init)
{
    int i;
    //-100% =~ 0x03e8
    //+100% =~ 0x07ca
    //Calculate:
    //Center = 0x5d9
    //1 %    = 5
    packet[0] = init ? 0xaa : 0x55; 
    packet[1] = (id >>  0) & 0xff;
    packet[2] = (id >>  8) & 0xff;
    packet[3] = (id >> 16) & 0xff;
    packet[4] = (id >> 24) & 0xff;
    for (i = 0; i < 8; i++) {
        if (i > Model.num_channels) {
            packet[5 + i*2] = 0;
            packet[6 + i*2] = 0;
            continue;
        }
        s32 value = (s32)Channels[i] * 0x1f1 / CHAN_MAX_VALUE + 0x5d9;
        if(Model.proto_opts[PROTOOPTS_WLTOYS] == WLTOYS_EXT_CX20 && i == 1) // reverse elevator
            value = 3000 - value;
        if (value < 0)
            value = 0;
        packet[5 + i*2] = value & 0xff;
        packet[6 + i*2] = (value >> 8) & 0xff;
    }
    flysky_apply_extension_flags();
}

MODULE_CALLTYPE
static uint16_t flysky_cb()
{
    if (counter) {
        flysky_build_packet(1);
        A7105_WriteData(packet, 21, 1);
        counter--;
        if (! counter)
            PROTOCOL_SetBindState(0);
    } else {
        flysky_build_packet(0);
        A7105_WriteData(packet, 21, hopping_frequency[hopping_frequency_no & 0x0F]);
        //Keep transmit power updated
        if(tx_power != Model.tx_power) {
            A7105_SetPower(Model.tx_power);
            tx_power = Model.tx_power;
        }
        // keep frequency tuning updated
        if(freq_offset != Model.proto_opts[PROTOOPTS_FREQTUNE]) {
                freq_offset = Model.proto_opts[PROTOOPTS_FREQTUNE];
                A7105_AdjustLOBaseFreq(freq_offset);
        }
    }
    hopping_frequency_no++;
    return packet_period;
}

static void initialize(uint8_t bind) {
    uint8_t chanrow;
    uint8_t chanoffset;
    uint8_t temp;

    CLOCK_StopTimer();
    if(Model.proto_opts[PROTOOPTS_WLTOYS] == WLTOYS_EXT_CX20) {
        packet_period = PACKET_PERIOD_CX20;
    } else {
        packet_period = PACKET_PERIOD_FLYSKY;
    }
    while(1) {
        A7105_Reset();
        CLOCK_ResetWatchdog();
        if (flysky_init())
            break;
    }
    if (Model.fixed_id) {
        id = Model.fixed_id;
    } else {
        id = (Crc(&Model, sizeof(Model)) + Crc(&Transmitter, sizeof(Transmitter))) % 999999;
    }
    if ((id & 0xf0) > 0x90) // limit offset to 9 as higher values don't work with some RX (ie V912)
        id = id - 0x70;
    chanrow = id % 16;
    chanoffset = (id & 0xff) / 16;
    
    for(uint8_t i=0;i<16;i++)
    {
        temp=pgm_read_byte(&tx_channels[chanrow>>1][i>>2]);
        if(i&0x02)
            temp&=0x0F;
        else
            temp>>=4;
        temp*=0x0A;
        if(i&0x01)
            temp+=0x50;
        if(Model.proto_opts[PROTOOPTS_WLTOYS] == WLTOYS_EXT_CX20)
        {
            if(temp==0x0A)
                temp+=0x37;
            if(temp==0xA0)
            {
                if (chanoffset<4)
                    temp=0x37;
                else if (chanoffset<9)
                    temp=0x2D;
                else
                    temp=0x29;
            }
        }
        hopping_frequency[((chanrow&1)?15-i:i)]=temp-chanoffset;
    }
    
    tx_power = Model.tx_power;
    A7105_SetPower(Model.tx_power);
    freq_offset = Model.proto_opts[PROTOOPTS_FREQTUNE];
    A7105_AdjustLOBaseFreq(freq_offset);
    hopping_frequency_no = 0;
    if (bind || ! Model.fixed_id) {
        counter = BIND_COUNT;
        PROTOCOL_SetBindState(2500 * packet_period / 1000); //msec
    } else {
        counter = 0;
    }
    
    CLOCK_StartTimer(2400, flysky_cb);
}

const void *FLYSKY_Cmds(enum ProtoCmds cmd)
{
    switch(cmd) {
        case PROTOCMD_INIT:  initialize(0); return 0;
        case PROTOCMD_DEINIT:
        case PROTOCMD_RESET:
            CLOCK_StopTimer();
            return (void *)(A7105_Reset() ? 1L : -1L);
        case PROTOCMD_CHECK_AUTOBIND: return Model.fixed_id ? 0 : (void *)1L;
        case PROTOCMD_BIND:  initialize(1); return 0;
        case PROTOCMD_NUMCHAN: return (void *)12L;
        case PROTOCMD_DEFAULT_NUMCHAN: return (void *)8L;
        case PROTOCMD_CURRENT_ID: return (void *)((unsigned long)id);
        case PROTOCMD_GETOPTIONS:
            return flysky_opts;
        case PROTOCMD_TELEMETRYSTATE: return (void *)(long)PROTO_TELEM_UNSUPPORTED;
        default: break;
    }
    return 0;
}
