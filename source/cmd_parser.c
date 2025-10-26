/*
 * cmd_parser.c
 *
 *  Created on: 13.11.2021
 *      Author: pvvx
 */
#include "tl_common.h"
#include "app.h"
#include "ble.h"
#include "stack/ble/ble.h"
#include "vendor/common/blt_common.h"
#include "cmd_parser.h"
#include "scaning.h"
#include "flash_eep.h"

#define _flash_read(faddr,len,pbuf) flash_read_page(FLASH_BASE_ADDR + (u32)faddr, len, (u8 *)pbuf)

__attribute__((optimize("-Os")))
int onSppReceiveData(void * p) {
	rf_packet_att_data_t *req = (rf_packet_att_data_t*) p;
	u32 len = req->l2cap - 3;
	if(len) {
		u8 cmd = req->dat[0];
		len--;
		sppDataBuffer[0] = cmd;
		sppDataBuffer[1] = 0; // no err?
		u32 olen = 1;
		if (cmd == CMD_ID_DEV_ID) { // Get DEV_ID
			pdev_id_t p = (pdev_id_t) sppDataBuffer;
			// p->pid = CMD_ID_DEV_ID;
			//p->revision = 0;
			p->hw_version = 0x100 + DEVICE_TYPE;
			p->sw_version = SW_VERSION;
			p->dev_spec_data = 0;
			p->services = DEV_SERVICES;
			olen = sizeof(dev_id_t);
		} else if (cmd == CMD_ID_UTC_TIME) { // Get/set utc time
			if (len) {
				if (len > sizeof(wrk.utc_time_sec))
					len = sizeof(wrk.utc_time_sec);
				memcpy(&wrk.utc_time_sec, &req->dat[1], len);
#if (DEV_SERVICES & SERVICE_TIME_ADJUST)
				wrk.utc_set_time_sec = wrk.utc_time_sec;
#endif
#if (DEV_SERVICES & SERVICE_HARD_CLOCK)
				rtc_set_utime(utc_time_sec);
#endif
			}
			memcpy(&sppDataBuffer[1], &wrk.utc_time_sec, sizeof(wrk.utc_time_sec));
#if (DEV_SERVICES & SERVICE_TIME_ADJUST)
			memcpy(&sppDataBuffer[sizeof(wrk.utc_time_sec) + 1], &wrk.utc_set_time_sec, sizeof(wrk.utc_set_time_sec));
			olen = sizeof(wrk.utc_time_sec) + sizeof(wrk.utc_set_time_sec) + 1;
#else
			olen = sizeof(wrk.utc_time_sec) + 1;
#endif
#if USE_TIME_ADJUST
		} else if (cmd == CMD_ID_TADJUST) { // Get/set adjust time clock delta (in 1/16 us for 1 sec)
			if(len > 2) {
				s16 delta = req->dat[1] | (req->dat[2] << 8);
				wrk.utc_time_tick_step = CLOCK_16M_SYS_TIMER_CLK_1S + delta;
				flash_write_cfg(&wrk.utc_time_tick_step, EEP_ID_TIMAD, sizeof(wrk.utc_time_tick_step));
			}
			memcpy(&sppDataBuffer[1], &wrk.utc_time_tick_step, sizeof(wrk.utc_time_tick_step));
			olen = sizeof(wrk.utc_time_tick_step);
#endif
#if (DEV_SERVICES & SERVICE_BINDKEY)
		} else if (cmd == CMD_ID_BKEY) { // Get/set beacon bindkey
			if (len == sizeof(bindkey) + 1) {
				memcpy(bindkey, &req->dat[1], sizeof(bindkey));
				flash_write_cfg(&bindkey, EEP_ID_BKEY, sizeof(bindkey));
				// bindkey_init();
			}
			if (flash_read_cfg(&bindkey, EEP_ID_BKEY, sizeof(bindkey)) == sizeof(bindkey)) {
				memcpy(&sppDataBuffer[1], bindkey, sizeof(bindkey));
				olen = sizeof(bindkey) + 1;
			} else { // No bindkey in EEP!
				sppDataBuffer[1] = 0xff;
				olen = 2;
			}
#endif
#if (DEV_SERVICES & SERVICE_PINCODE)
		} else if (cmd == CMD_ID_PINCODE && len > 4) { // Set new pinCode 0..999999
			u32 old_pincode = wrk.pincode;
			u32 new_pincode = req->dat[1] | (req->dat[2]<<8) | (req->dat[3]<<16) | (req->dat[4]<<24);
			if (wrk.pincode != new_pincode) {
				wrk.pincode = new_pincode;
				if (flash_write_cfg(&wrk.pincode, EEP_ID_PINCD, sizeof(wrk.pincode))) {
					if ((wrk.pincode != 0) ^ (old_pincode != 0)) {
						bls_smp_eraseAllParingInformation();
						wrk.ble_connected |= BIT(CONNECTED_FLG_RESET_OF_DISCONNECT); // reset device on disconnect
					}
					sppDataBuffer[1] = 1;
				} else	sppDataBuffer[1] = 3;
			} //else send_buf[1] = 0;
			olen = 2;
#endif
		} else if (cmd == CMD_ID_EBKEY) { // Get/set beacon bindkey1
			u8 * pk = scan.cfg.bindkey;
			if(len == 16) {
				memcpy(pk, &req->dat[1], 16);
				flash_write_cfg(pk, EEP_ID_EBKEY, 16);
			}
			if(flash_read_cfg(pk, EEP_ID_EBKEY, 16) == 16) {
				memcpy(&sppDataBuffer[1], pk, 16);
				olen = 16;
			} else { // No bindkey1 in EEP!
				sppDataBuffer[1] = 0xff;
			}
		} else if (cmd == CMD_ID_EMAC) { // Get/set MAC1
			if(len == sizeof(scan.cfg.MAC)) {
				memcpy(scan.cfg.MAC, &req->dat[1], sizeof(scan.cfg.MAC));
				flash_write_cfg(&scan.cfg.MAC, EEP_ID_EMAC, sizeof(scan.cfg.MAC));
			}
			memcpy(&sppDataBuffer[1], scan.cfg.MAC, sizeof(scan.cfg.MAC));
			olen = sizeof(scan.cfg.MAC);
		} else if (cmd == CMD_ID_CFG) { // Get/set device config
			if(len) {
				if(len > sizeof(cfg))
					len = sizeof(cfg);
				memcpy(&cfg, &req->dat[1], len);
				test_config();
				flash_write_cfg(&cfg, EEP_ID_DEV_CFG, sizeof(cfg));
			}
			memcpy(&sppDataBuffer[1], &cfg, sizeof(cfg));
			olen = sizeof(cfg);

		} else if (cmd == CMD_ID_DEV_MAC) { // Get/Set mac
			if (len && req->dat[1] == 0) { // default MAC
				flash_erase_mac_sector(CFG_ADR_MAC);
				blc_initMacAddress(CFG_ADR_MAC, mac_public, mac_random_static);
				wrk.ble_connected |= BIT(CONNECTED_FLG_RESET_OF_DISCONNECT); // reset device on disconnect
			} else if (len == sizeof(mac_public)+1 && req->dat[1] == sizeof(mac_public)) {
				if (memcmp(mac_public, &req->dat[2], sizeof(mac_public))) {
					memcpy(mac_public, &req->dat[2], sizeof(mac_public));
					mac_random_static[0] = mac_public[0];
					mac_random_static[1] = mac_public[1];
					mac_random_static[2] = mac_public[2];
					generateRandomNum(2, &mac_random_static[3]);
					mac_random_static[5] = 0xC0; 			//for random static
					blc_newMacAddress(CFG_ADR_MAC, mac_public, mac_random_static);
					wrk.ble_connected |= BIT(CONNECTED_FLG_RESET_OF_DISCONNECT); // reset device on disconnect
				}
			} else	if (len == sizeof(mac_public)+1+2 && req->dat[1] == sizeof(mac_public)+2) {
				if (memcmp(mac_public, &req->dat[2], sizeof(mac_public))
						|| mac_random_static[3] != req->dat[2+6]
						|| mac_random_static[4] != req->dat[2+7] ) {
					memcpy(mac_public, &req->dat[2], sizeof(mac_public));
					mac_random_static[0] = mac_public[0];
					mac_random_static[1] = mac_public[1];
					mac_random_static[2] = mac_public[2];
					mac_random_static[3] = req->dat[2+6];
					mac_random_static[4] = req->dat[2+7];
					mac_random_static[5] = 0xC0; 			//for random static
					blc_newMacAddress(CFG_ADR_MAC, mac_public, mac_random_static);
					wrk.ble_connected |= BIT(CONNECTED_FLG_RESET_OF_DISCONNECT); // reset device on disconnect
				}
			}
			sppDataBuffer[1] = 8;
			_flash_read(CFG_ADR_MAC, 8, &sppDataBuffer[2]); // MAC[6] + mac_random[2]
			olen = 8 + 2;
		} else if (cmd == CMD_ID_MTU && len > 1) { // Request Mtu Size Exchange
			if(req->dat[1] > ATT_MTU_SIZE)
				sppDataBuffer[1] = blc_att_requestMtuSizeExchange(BLS_CONN_HANDLE, req->dat[1]);
			else
				sppDataBuffer[1] = 0xff;
			olen = 2;
		} else if (cmd == CMD_ID_REBOOT) { // Set Reboot on disconnect
			wrk.ble_connected |= BIT(CONNECTED_FLG_RESET_OF_DISCONNECT); // reset device on disconnect
			olen = 2;
		}
		if(olen)
			bls_att_pushNotifyData(SPP_Server2Client_DP_H, sppDataBuffer, olen + 1);
	}
	return 0;
}


