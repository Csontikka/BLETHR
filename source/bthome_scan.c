/*
 * scan_bthome.c
 *
 *  Created on: 30 янв. 2025 г.
 *      Author: pvvx
 */

#include "tl_common.h"
#include "ble.h"
#include "scaning.h"
#include "bthome_adv.h"
#include "aes_ccm.h"

#define USE_EXT_BINDKEY		1

// BTHome v2
// bit[4:5]: 0 - 1, 1 - 0.1, 2 - 0.01, 3 - 0.001
// bit[6]: bool (on/off)
// bit[7]: signed
// 0x - unsigned 1
// 1x - unsigned 0.1
// 2x - unsigned 0.01
// 3x - unsigned 0.001
// 8x - signed 1
// 9x - signed 0.1
// Ax - signed 0.01
// Bx - signed 0.001

const u8 tblBTHome[] = {
//  0     1     2     3     4     5     6     7     8     9   	a     b     c     d     e     f
	0x01, 0x01, 0xA2, 0x21, 0x23, 0x23, 0x22, 0x22, 0xA2, 0x01, 0x33, 0x23, 0x32, 0x02, 0x02, 0x41, // 0x
	0x41, 0x41, 0x02, 0x02, 0x32, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, // 1x
	0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x01, 0x01, // 2x
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x02, 0x04, 0x12, // 3x
	0x02, 0x12, 0x33, 0x32, 0x22, 0x12, 0x11, 0x12, 0x02, 0x32, 0x12, 0x33, 0x34, 0x34, 0x34, 0x34, // 4x
	0x04, 0x32, 0x32, 0x0f, 0x0f // 5x
/*
	0xF0	device type id	uint16 (2 bytes)	F00100	1
	0xF1	firmware version	uint32 (4 bytes)	F100010204	4.2.1.0
	0xF2	firmware version	uint24 (3 bytes)	F1000106	6.1.0
*/
};


#if USE_EXT_BINDKEY
const u8 ccm_aad = 0x11;
#endif
//RAM u8 old_packet_id;

_attribute_ram_code_
__attribute__((optimize("-Os")))
void filter_bthome_ad(padv_bthome_t p, u8 * pmac) {
	int len = p->size;
//	u8 packet_id;
#if UART_PRINT_DEBUG_ENABLE
	//   0  1  2  3  4  5  6  7
	// {0E:16:D2:FC:40:00:45:01:4C:02:E8:07:03:AC:11}
	// u_array_printf((unsigned char*)p, len + 1);
#endif
	if(len > sizeof(padv_bthome_t)) {
		len -= sizeof(adv_bthome_t) - 2; // p->data len
		if(p->ver == BtHomeID_ver_encrypt && len > 9) {
#if USE_EXT_BINDKEY
			len -= 8;
			bthome_beacon_nonce_t bthome_nonce;
			//memcpy(bthome_nonce.mac, dev_MAC[n], sizeof(bthome_nonce.mac));
			bthome_nonce.mac[5] = pmac[0];
			bthome_nonce.mac[4] = pmac[1];
			bthome_nonce.mac[3] = pmac[2];
			bthome_nonce.mac[2] = pmac[3];
			bthome_nonce.mac[1] = pmac[4];
			bthome_nonce.mac[0] = pmac[5];
			u8 *pb = (u8 *)&bthome_nonce.uuid16;
			u8 *pmic = (u8 *)&p->UUID;
			// UUID16, ver
			*pb++ = *pmic++;
			*pb++ = *pmic++;
			*pb++ = *pmic;
			// count32
			pmic = &p->data[len];
//			packet_id = *pmic;
			*pb++ = *pmic++;
			*pb++ = *pmic++;
			*pb++ = *pmic++;
			*pb = *pmic++; // pm = &mic[4]
			if(aes_ccm_auth_decrypt((const unsigned char *)scan.cfg.bindkey,
					(u8 *)&bthome_nonce, sizeof(bthome_nonce),
					NULL, 0,
					p->data, len, // len crypt_data
					p->data, // decrypt data
					pmic, 4))  // &mic: &crypt_data[len + size (ext_cnt[3])]
#endif
				return;
		} else if(p->ver != BtHomeID_ver) {
			return;
		}
		padv_bthome_sruct_t ps = (padv_bthome_sruct_t)&p->data;
		while(len > 0) {
			if(ps->type < sizeof(tblBTHome)) {
				if(ps->type == BtHomeID_PacketId) { // in 0.01 C
					ext_measure.new_cnt = ps->data_ub[0];
				} else	if(ps->type == BtHomeID_temperature) { // in 0.01 C
					ext_measure.temperature = ps->data_is[0];
					ext_measure.update |= FLG_UPDATE_TEMP;
//					u_printf("t:%u\n", ext_measure.temperature);
//				} else if(ps->type == BtHomeID_temperature_01) {  // in 0.1 C
//					ext_measure.temperature = ps->data_is[0]*10;
//					ext_measure.update |= FLG_UPDATE_HUMI;
				} else if(ps->type == BtHomeID_humidity) { // in 0.01 %
					ext_measure.humidity = ps->data_us[0];
					ext_measure.update |= FLG_UPDATE_HUMI;
//					u_printf("h:%u\n", ext_measure.humidity);
//				} else if(ps->type == BtHomeID_humidity8) { // in 1 %
//					ext_measure.humidity = ps->data_ub[0]*100;
//					ext_measure.update |= FLG_UPDATE_HUMI;
				} else if(ps->type == BtHomeID_battery) { // Batt in %
					ext_measure.battery = ps->data_ub[0];
					ext_measure.update |= FLG_UPDATE_BAT;
				} else if(ps->type == BtHomeID_voltage) { // in 0.001V
					ext_measure.voltage = ps->data_us[0];
					ext_measure.update |= FLG_UPDATE_VBAT;
				}
			} else
				break;
			int size = (tblBTHome[ps->type] & 0x0f) + 1;
			if(size == 0x10)
				size = ps->data_ub[0] + 2;
			len -= size;
			ps = (padv_bthome_sruct_t)((u32)ps + size);
		}
	}
}
