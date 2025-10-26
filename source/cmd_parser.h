/*
 * cmd_parser.h
 *
 *  Created on: 13.11.2021
 *      Author: pvvx
 */

#ifndef CMD_PARSER_H_
#define CMD_PARSER_H_

enum {
	CMD_ID_DEV_ID   = 0x00, // Get Info
	CMD_ID_DEV_MAC	= 0x10, // Get/Set MAC [+RandMAC], [size][mac[6][randmac[2]]]
//	CMD_ID_BKEY		= 0x18, // Get/Set beacon bindkey in EEP
	CMD_ID_UTC_TIME = 0x23, // Get/set utc time
	CMD_ID_TADJUST  = 0x24, // Get/set adjust time clock delta (in 1/16 us for 1 sec)
	CMD_ID_CFG      = 0x55,	// Get/set device config
	CMD_ID_EMAC     = 0x58, // Get/set ext MAC
	CMD_ID_EBKEY 	= 0x5C, // Get/set ext beacon bindkey in EEP (for MAC)
	CMD_ID_PINCODE  = 0x70, // Set new PinCode 0..999999
	CMD_ID_MTU		= 0x71, // Request Mtu Size Exchange (23..255)
	CMD_ID_REBOOT	= 0x72, // Set Reboot on disconnect
} CMD_ID_KEYS;

// CMD_ID_DEV_ID
typedef struct _dev_id_t{
	u8 pid;				// packet identifier = CMD_ID_DEVID
	u8 revision;		// protocol version/revision
	u16 hw_version;		// hardware version
	u16 sw_version;		// software version (BCD)
	u16 dev_spec_data;	// device-specific data (bit0..3: sensor_type)
	u32 services;		// supported services by the device
} dev_id_t, * pdev_id_t;

#endif /* CMD_PARSER_H_ */
