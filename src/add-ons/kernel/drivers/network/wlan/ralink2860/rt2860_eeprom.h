
/*-
 * Copyright (c) 2009-2010 Alexander Egorenkov <egorenar@gmail.com>
 * Copyright (c) 2009 Damien Bergamini <damien.bergamini@free.fr>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _RT2860_EEPROM_H_
#define _RT2860_EEPROM_H_

#define RT2860_EEPROM_VERSION					0x0002
#define RT2860_EEPROM_ADDRESS01					0x0004
#define RT2860_EEPROM_ADDRESS23					0x0006
#define RT2860_EEPROM_ADDRESS45					0x0008
#define RT2860_EEPROM_POWERSAVE_LEVEL			0x0022
#define RT2860_EEPROM_ANTENNA					0x0034
#define RT2860_EEPROM_NIC_CONFIG				0x0036
#define RT2860_EEPROM_COUNTRY					0x0038
#define RT2860_EEPROM_RF_FREQ_OFF				0x003a
#define RT2860_EEPROM_LED1_OFF					0x003c
#define RT2860_EEPROM_LED2_OFF					0x003e
#define RT2860_EEPROM_LED3_OFF					0x0040
#define RT2860_EEPROM_LNA_GAIN					0x0044
#define RT2860_EEPROM_RSSI_OFF_2GHZ_BASE		0x0046
#define RT2860_EEPROM_RSSI_OFF_5GHZ_BASE		0x0046
#define RT2860_EEPROM_TXPOW_RATE_DELTA			0x0050
#define RT2860_EEPROM_TXPOW1_2GHZ_BASE			0x0052
#define RT2860_EEPROM_TXPOW2_2GHZ_BASE			0x0060
#define RT2860_EEPROM_TSSI_2GHZ_BASE			0x006e
#define RT2860_EEPROM_TXPOW1_5GHZ_BASE			0x0078
#define RT2860_EEPROM_TXPOW2_5GHZ_BASE			0x00a6
#define RT2860_EEPROM_TSSI_5GHZ_BASE			0x00d4
#define RT2860_EEPROM_TXPOW_RATE_BASE			0x00de
#define RT2860_EEPROM_BBP_BASE					0x00f0

#define RT2860_EEPROM_RF_2820					1 /* 2.4GHz 2T3R */
#define RT2860_EEPROM_RF_2850					2 /* 2.4/5GHz 2T3R */
#define RT2860_EEPROM_RF_2720					3 /* 2.4GHz 1T2R */
#define RT2860_EEPROM_RF_2750					4 /* 2.4G/5GHz 1T2R */

/*
 * RT2860_EEPROM_NIC_CONFIG flags
 */
#define RT2860_EEPROM_EXT_LNA_5GHZ				(1 << 3)
#define RT2860_EEPROM_EXT_LNA_2GHZ				(1 << 2)
#define RT2860_EEPROM_TX_AGC_CNTL				(1 << 1)
#define RT2860_EEPROM_HW_RADIO_CNTL				(1 << 0)

#define RT2860_EEPROM_LED_POLARITY				(1 << 7)
#define RT2860_EEPROM_LED_MODE_MASK				0x7f

#define RT2860_EEPROM_LED_CNTL_DEFAULT			0x01
#define RT2860_EEPROM_LED1_OFF_DEFAULT			0x5555
#define RT2860_EEPROM_LED2_OFF_DEFAULT			0x2221
#define RT2860_EEPROM_LED3_OFF_DEFAULT			0xa9f8

#define RT2860_EEPROM_RSSI_OFF_MIN				-10
#define RT2860_EEPROM_RSSI_OFF_MAX				10

#define RT2860_EEPROM_TXPOW_2GHZ_MIN			0
#define RT2860_EEPROM_TXPOW_2GHZ_MAX			31
#define RT2860_EEPROM_TXPOW_2GHZ_DEFAULT		5
#define RT2860_EEPROM_TXPOW_5GHZ_MIN			-7
#define RT2860_EEPROM_TXPOW_5GHZ_MAX			15
#define RT2860_EEPROM_TXPOW_5GHZ_DEFAULT		5

#endif /* #ifndef _RT2860_EEPROM_H_ */
