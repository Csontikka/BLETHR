/*
 * bthome_beacon.c
 *
 *  Created on: 17.10.23
 *      Author: pvvx
 */

#include "tl_common.h"
#include "app_config.h"
#include "ble.h"
#include "battery.h"
#include "app.h"
#include "bthome_adv.h"
#include "scaning.h"

RAM adv_buf_t adv_buf = {
		.flag.size = 2,
		.flag.type = GAP_ADTYPE_FLAGS,
			/*	Flags:
			 	bit0: LE Limited Discoverable Mode
				bit1: LE General Discoverable Mode
				bit2: BR/EDR Not Supported
				bit3: Simultaneous LE and BR/EDR to Same Device Capable (Controller)
				bit4: Simultaneous LE and BR/EDR to Same Device Capable (Host)
				bit5..7: Reserved
			 */
		.flag.flg = 0x06,
		.type = GAP_ADTYPE_SERVICE_DATA_UUID_16BIT, // 16-bit UUID
		.UUID = ADV_BTHOME_UUID16,
		.info = BtHomeID_ver,
		.p_id = BtHomeID_PacketId,
		.b_id = BtHomeID_battery,
#if (DEV_SERVICES & SERVICE_THS)
		.t_id = BtHomeID_temperature,
		.h_id = BtHomeID_humidity,
#endif
		.v_id = BtHomeID_voltage
#if SCAN_DEBUG_ERR
		, .c_id = BtHomeID_count16
#endif
};


_attribute_ram_code_
__attribute__((optimize("-Os")))
void bthome_data_beacon(void) {
	adv_buf_t * p = &adv_buf;
//	p->type = GAP_ADTYPE_SERVICE_DATA_UUID_16BIT; // 16-bit UUID
//	p->UUID = ADV_BTHOME_UUID16;
//	p->info = BtHomeID_ver;
//	p->p_id = BtHomeID_PacketId;
	p->pid++;
//	p->b_id = BtHomeID_battery;
	p->battery_level = ext_measure.battery; // measured_data.battery_level;
#if (DEV_SERVICES & SERVICE_THS)
//	p->t_id = BtHomeID_temperature;
	p->temperature = measured_data.temp; // x0.01 C
//	p->h_id = BtHomeID_humidity;
	p->humidity = measured_data.humi; // x0.01 %
#endif
//	p->v_id = BtHomeID_voltage;
#if USE_AVERAGE_BATTERY
	p->battery_mv = measured_data.average_battery_mv; // mV
#else
	p->battery_mv = measured_data.battery_mv; // mV
#endif
#if SCAN_DEBUG_ERR
//	p->c_id = BtHomeID_count16;
	p->count = scan.all_err;
#endif
	p->size = sizeof(adv_buf_t) - sizeof(ad_flag_t) - 1;
	bls_ll_setAdvData((u8 *)p, sizeof(adv_buf_t));
}
