/*
 * app.h
 *
 *  Created on: 19.12.2020
 *      Author: pvvx
 */

#ifndef MAIN_H_
#define MAIN_H_

#define EEP_ID_TIMAD	(0x0ADA) // EEP ID time adjust
#define EEP_ID_PINCD	(0xC0DE) // EEP ID pincode
#define EEP_ID_DEV_CFG	(0xDECF)
#define EEP_ID_BKEY 	(0xBEAC) // EEP ID bkey
#define EEP_ID_EMAC     (0xEACC) // EEP ID ext MAC
#define EEP_ID_EBKEY 	(0xEBCC) // EEP ID ext bindkey

typedef struct __attribute__((packed)) {
	struct __attribute__((packed)) {
		u32 temp_F_or_C	: 1;
	} flg;
	u8 rf_tx_power; 	// RF_POWER_N25p18dBm .. RF_POWER_P3p01dBm (130..191)
	u16	scan_interval;	// интервал в ms
	u16 win_min;		// set in config, default = SCAN_WINDOW_MIN
	u16 win_max;		// set in config, default = SCAN_WINDOW_MAX	u32
//	u8 	MAC[6]; 		// [0] - lo, .. [6] - hi digits
} dev_cfg_t;

extern dev_cfg_t cfg;

typedef struct _measured_data_t {
// start send part (MEASURED_MSG_SIZE)
	u16		battery_mv; // mV
	s16		temp; // x 0.01 C
	s16		humi; // x 0.01 %
	u16 	average_battery_mv; // mV
	u8 		battery_level;
} measured_data_t;  // save max 18 bytes

extern measured_data_t measured_data;

//====================================================================

// bits: wrk.ble_connected
enum {
	CONNECTED_FLG_ENABLE = 0,
	CONNECTED_FLG_PAR_UPDATE = 1,
	CONNECTED_FLG_BONDING = 2,
	CONNECTED_FLG_RESET_OF_DISCONNECT = 7
} CONNECTED_FLG_BITS_e;

typedef struct {
	u32 utc_time_sec;	// clock in sec (= 0 1970-01-01 00:00:00)
	u32 utc_time_sec_tick; //
	u32 utc_time_tick_step; // adjust time clock (in 1/16 us for 1 sec)
#if (DEV_SERVICES & SERVICE_TIME_ADJUST)
	u32 utc_set_time_sec;
#endif
#if (DEV_SERVICES & SERVICE_PINCODE)
	u32 pincode;
#endif
#if	(BATT_SERVICE_ENABLE)
	u32 tim_measure; // timer measurements >= 10 sec
	u8 send_measure;   // flag, measure complete
#endif
	u8 scan_enable;
	u8 ota_is_working;
	u8 ble_connected;	// BIT(CONNECTED_FLG_BITS_e)
	u8 lcd_redraw;		// flag: an error screen overwrote the display, repaint it on the next update
} wrk_data_t;

extern wrk_data_t wrk;

#if (DEV_SERVICES & SERVICE_BINDKEY)
u8 bindkey[16];
#endif

void test_config(void);

//---- blt_common.c
void blc_newMacAddress(int flash_addr, u8 *mac_pub, u8 *mac_rand);
void SwapMacAddress(u8 *mac_out, u8 *mac_in);
void flash_erase_mac_sector(u32 faddr);

#endif /* MAIN_H_ */
