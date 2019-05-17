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
    along with this program.  If not, see <http:// www.gnu.org/licenses/>.

    author: santipc <AT> gmail.com
*/

#include "a7105.h"
#include "spi.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

#include "debug.h"
#include "timeout.h"
#include <string.h>

#include <libopencm3/stm32/timer.h>

static volatile uint8_t a7105_frame_counter;
static volatile uint32_t a7105_state;

void a7105_init(void) {
    debug("a7105: init\n"); debug_flush();
    a7105_init_gpio();
    spi_init();

    debug("a7105: init done\n"); debug_flush();
}

void a7105_init_gpio(void) {
    // set high:
    gpio_set(POWERDOWN_GPIO, POWERDOWN_PIN);

    // set powerdown trigger pin as output
    gpio_mode_setup(POWERDOWN_GPIO, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, POWERDOWN_PIN);
    gpio_set_output_options(POWERDOWN_GPIO, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, POWERDOWN_PIN);
    // PA/ LNA:
    // periph clock enable for port
    rcc_periph_clock_enable(GPIO_RCC(A7105_LNA_GPIO));
    rcc_periph_clock_enable(GPIO_RCC(A7105_PA_GPIO));

    // CTX:
    // set all gpio directions to output
    gpio_mode_setup(A7105_LNA_GPIO, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, A7105_LNA_PIN);
    gpio_set_output_options(A7105_LNA_GPIO, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, A7105_LNA_PIN);

    // CRX:
    gpio_mode_setup(A7105_PA_GPIO, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, A7105_PA_PIN);
    gpio_set_output_options(A7105_PA_GPIO, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, A7105_PA_PIN);
/*
    cc2500_enter_rxmode();

    // enable clock for GDO1
    rcc_periph_clock_enable(GPIO_RCC(CC2500_GDO1_GPIO));

    // setup gdo1
    gpio_mode_setup(CC2500_GDO1_GPIO, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, CC2500_GDO1_PIN);
    gpio_set_output_options(CC2500_GDO1_GPIO, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, CC2500_GDO1_PIN);

    // periph clock enable for port
    rcc_periph_clock_enable(GPIO_RCC(CC2500_GDO2_GPIO));

    // configure GDO2 pins as Input
    gpio_mode_setup(CC2500_GDO2_GPIO, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, CC2500_GDO2_PIN);
    gpio_set_output_options(CC2500_GDO2_GPIO, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, CC2500_GDO2_PIN);
*/
}

void a7105_write_reg(uint8_t address, uint8_t data) {
    spi_csn_lo();
    spi_tx(address);
    spi_tx(data);
    spi_csn_hi();
}

uint8_t a7105_read_reg(uint8_t address) {
    uint8_t data;
    spi_csn_lo();
    spi_tx(0x40 | address);
    /* Wait for tx completion before spi shutdown */
    data = spi_rx();
    spi_csn_hi();
    return data;
}

void a7105_write_data(uint8_t *dpbuffer, uint8_t len, uint8_t channel) {
    int i;
    spi_csn_lo();
    spi_tx(A7105_RST_WRPTR);
    spi_tx(A7105_05_FIFO_DATA);
    for (i = 0; i < len; i++)
        spi_tx(dpbuffer[i]);
    spi_csn_hi();

    a7105_write_reg(A7105_0F_PLL_I, channel);
    a7105_strobe(A7105_TX);
}

void a7105_read_data(uint8_t *dpbuffer, uint8_t len) {
    int i;
    a7105_strobe(A7105_RST_RDPTR);
    for (i = 0; i < len; i++)
        dpbuffer[i] = a7105_read_reg(A7105_05_FIFO_DATA);
    return;
}

/*
 * 1 - Tx else Rx
 */
void a7105_set_tx_rx_mode(enum TXRX_State mode) {
    if (mode == TX_EN) {
        a7105_write_reg(A7105_0B_GPIO1_PIN1, 0x33);
        a7105_write_reg(A7105_0C_GPIO2_PIN_II, 0x31);
    } else if (mode == RX_EN) {
        a7105_write_reg(A7105_0B_GPIO1_PIN1, 0x31);
        a7105_write_reg(A7105_0C_GPIO2_PIN_II, 0x33);
    } else {
        // The A7105 seems to some with a cross-wired power-amp (A7700)
        // On the XL7105-D03, TX_EN -> RXSW and RX_EN -> TXSW
        // This means that sleep mode is wired as RX_EN = 1 and TX_EN = 1
        // If there are other amps in use, we'll need to fix this
        a7105_write_reg(A7105_0B_GPIO1_PIN1, 0x33);
        a7105_write_reg(A7105_0C_GPIO2_PIN_II, 0x33);
    }
}

int a7105_reset() {
    uint8_t result;

    // Issue full reset command to A7105.
    a7105_write_reg(A7105_00_MODE, 0x00);
    // Configure 4-wire SPI (GIO1 to MISO)
    a7105_write_reg(A7105_0B_GPIO1_PIN1, 0x19);
    delay_us(1000);
    result = a7105_read_reg(A7105_10_PLL_II);
    a7105_strobe(A7105_STANDBY);
    debug("a7105: transceiver result: 0x");
    debug_put_hex8(result);
    debug_put_newline();
    debug_flush();

    return result == 0x9e;
}

void a7105_write_id(uint32_t id) {
    spi_csn_lo();
    spi_tx(A7105_06_ID_DATA);
    spi_tx((id >> 24) & 0xFF);
    spi_tx((id >> 16) & 0xFF);
    spi_tx((id >> 8) & 0xFF);
    spi_tx((id >> 0) & 0xFF);
    spi_csn_hi();
}

void a7105_set_power(int power) {
    /*
    Power amp is ~+16dBm so:
    TXPOWER_100uW  = -23dBm == PAC=0 TBG=0
    TXPOWER_300uW  = -20dBm == PAC=0 TBG=1
    TXPOWER_1mW    = -16dBm == PAC=0 TBG=2
    TXPOWER_3mW    = -11dBm == PAC=0 TBG=4
    TXPOWER_10mW   = -6dBm  == PAC=1 TBG=5
    TXPOWER_30mW   = 0dBm   == PAC=2 TBG=7
    TXPOWER_100mW  = 1dBm   == PAC=3 TBG=7
    TXPOWER_150mW  = 1dBm   == PAC=3 TBG=7
    */
    uint8_t pac, tbg;
    switch (power) {
        case 0: pac = 0; tbg = 0; break;
        case 1: pac = 0; tbg = 1; break;
        case 2: pac = 0; tbg = 2; break;
        case 3: pac = 0; tbg = 4; break;
        case 4: pac = 1; tbg = 5; break;
        case 5: pac = 2; tbg = 7; break;
        case 6: pac = 3; tbg = 7; break;
        case 7: pac = 3; tbg = 7; break;
        default: pac = 0; tbg = 0; break;
    }
    a7105_write_reg(A7105_28_TX_TEST, (pac << 3) | tbg);
}

void a7105_strobe(enum A7105_State state) {
    spi_csn_lo();
    spi_tx(state);
    spi_csn_hi();
}

uint8_t a7105_check_transceiver(void) {
    debug("a7105: checking transceiver\n"); debug_flush();

    if (a7105_reset()) {
        debug("a7105: got valid initialization response!\n");
        debug_flush();
        return 1;
    }

    debug("a7105: got INVALID initialization response?!\n");
    debug_flush();
    return 0;
}

