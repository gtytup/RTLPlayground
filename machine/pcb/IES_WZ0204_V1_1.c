/* IES-WZ0204-V1.1, white-label industrial 4x2.5G + 2xSFP+ board (RTL8371B
 * family). Measured on the single unit, see doc/devices/IES-WZ0204-V1.1.md.
 *
 * Copper ports 1-4 follow the KP-9000-6XH-X2 (2M-PCB43-V1.1) layout, the two
 * SFP+ cages are wired the other way round: physical port 5 is on SDS1, physical
 * port 6 on SDS0. Of the copper ports only 2 and 3 are confirmed, and only the
 * left cage has been exercised - no module has been fitted to the right one.
 *
 * flash_sio = 1 is not a precaution: the fitted Boya BY25D40 (JEDEC
 * 0x68 0x40 0x13) offers 0x03/0x0B/0x3B reads but no 0xBB dual-I/O read, so the
 * SoC cannot fetch code in DIO mode at all.
 *
 * The VLAN slide switch pulls GPIO31 to ground when engaged. That pin is UART0
 * TX and this board brings no serial console out, so reading it as a plain GPIO
 * costs nothing - see GPIO31_UART0_TX in rtl837x_pins.c. */
#include "machine_defs.h"

__code const struct machine machine = {
	MACHINE_COMMON_SETTING,
	.flash_sio = 1,
	.n_sfp = 2,
	.log_to_phys_port = {0, 0, 0, 6, 1, 2, 3, 4, 5},
	.phys_to_log_port = {4, 5, 6, 7, 8, 3, 0, 0, 0},
	.is_sfp = {0, 0, 0, 2, 0, 0, 0, 0, 1},
	/* Left SFP cage (physical port 5, logical 8) = SDS1. Only this cage has
	 * been verified: detect pin, LOS pin and I2C bus all measured with a 10G
	 * and a 1G module. */
	.sfp_port[0].pin_detect = GPIO30_ACL_BIT3_EN,
	.sfp_port[0].pin_los = GPIO37,
	.sfp_port[0].pin_tx_disable = GPIO_NA,
	.sfp_port[0].sds = 1,
	.sfp_port[0].i2c = I2CBUS( GPIO39_I2C_SDA4, GPIO40_I2C_SCL3_MDC1 ),
	/* Right SFP cage (physical port 6, logical 3) = SDS0. Inherited from the
	 * sibling boards; never exercised on this one. */
	.sfp_port[1].pin_detect = GPIO50_I2C_SCL2_UART1_TX,
	.sfp_port[1].pin_los = GPIO10_LED10,
	.sfp_port[1].pin_tx_disable = GPIO_NA,
	.sfp_port[1].sds = 0,
	.sfp_port[1].i2c = I2CBUS( GPIO41_I2C_SDA3_MDIO1, GPIO40_I2C_SCL3_MDC1 ),
	.reset_pin = GPIO_NA,
	/* .pin_mode_vlan = GPIO31_UART0_TX comes from MACHINE_PIN_MODE_VLAN in this
	 * target's header, see *Mode switch* in doc/devices/IES-WZ0204-V1.1.md. */
	/* RTL837X_REG_LED_GLB_IO_EN bit 6 is set by IES_WZ0204_V1_1_init.c, as on
	 * the other boards of the family: without it the solid LED conditions do
	 * not drive the LEDs at all. */
	.high_leds = { .mux = LED_27 | LED_29, .enable = LED_28_SYS | LED_29 },
	.port_led_set = { 0, 0, 0, 0, 0, 0, 0, 0, 0},
	/* The ASIC default pad->LED-ID mux already matches the board - every LED's
	 * pad is assigned to its own port - so the copper ports need nothing. The
	 * one change is the two SFP cages' upper LEDs (pads 9 and 24): they sit on
	 * index 0, the shared link/activity entry, while the stock firmware shows
	 * link on the upper LED and activity on the lower one. Pointing them at
	 * index 1 (no ACT bit) gives a solid upper LED. */
	.led_mux_custom = 1,
	.led_mux = {
		0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f,
		0x0f, 0x0d, 0x0d, 0x0e, 0x10, 0x11, 0x12, 0x14,
		0x15, 0x16, 0x18, 0x19, 0x1a, 0x1c, 0x1d, 0x1e,
		0x21, 0x21, 0x22, 0x23,
	},
	/* Index 2 is used by the two SFP cages' lower LEDs only (pads 11 and 26),
	 * so it can carry the SFP's own conditions. The inherited value, 1G | LINK,
	 * never matches a 10G link - which is precisely why both lower LEDs stayed
	 * dark with a 10G module fitted. */
	.led_sets = { { LEDS_2G5 | LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT | LEDS_10G,
			LEDS_2G5 | LEDS_LINK | LEDS_10G,
			LEDS_2G5 | LEDS_1G | LEDS_10G | LEDS_LINK | LEDS_ACT,
			0 },
		    },
};
