/*
 * scaning.c
 *
 *  Created on: 20.11.2021
 *      Author: pvvx
 */
#include "tl_common.h"
#include "ble.h"
#include "lcd.h"
#include "scaning.h"
#include "bthome_adv.h"

#define SCAN_DEBUG	0 // UART_PRINT_DEBUG_ENABLE

u8 prev_advs[32];

RAM scan_wrk_t scan;

RAM  ext_measure_t ext_measure;

void scan_init(void) {

	//cfg.scan_interval
	scan.cfg.interval = (u32)cfg.scan_interval * 125;
	scan.cfg.win_min = (u32)cfg.win_min;
	scan.cfg.win_max = (u32)cfg.win_max;
	wrk.scan_enable = cfg.scan_interval != 0;
	scan.err_count = 0;
	scan.park_until = 0;
	scan.stage = SCAN_STAGE_START;
	blta.adv_interval = SCAN_SYN_ADV*625*CLOCK_16M_SYS_TIMER_CLK_1US;
}

_attribute_ram_code_
static void set_adv_time(u32 tt) {
	bls_ll_setAdvParam(tt, tt, // sleep x sec
		ADV_TYPE_CONNECTABLE_UNDIRECTED, OWN_ADDRESS_PUBLIC, 0, NULL,
		BLT_ENABLE_ADV_ALL, ADV_FP_NONE);
	rf_set_power_level_index(cfg.rf_tx_power);
	bls_ll_setAdvEnable(1);
}
//////////////////////////////////////////////////////////
// scan event call back
//////////////////////////////////////////////////////////
_attribute_ram_code_
__attribute__((optimize("-Os")))
int scanning_event_callback(u32 h, u8 *p, int n) {
	if (h & HCI_FLAG_EVENT_BT_STD) { // ble controller hci event
		if ((h & 0xff) == HCI_EVT_LE_META) {
			//----- hci le event: le adv report event -----
			if (p[0] == HCI_SUB_EVT_LE_ADVERTISING_REPORT) { // ADV packet
				//after controller is set to scan state, it will report all the adv packet it received by this event
				event_adv_report_t *pa = (event_adv_report_t *) p;
				if(memcmp(scan.cfg.MAC, pa->mac, sizeof(scan.cfg.MAC)) == 0) {
					u32 tt = clock_time();
					u32 delta = (tt - scan.cur_rx_tik) >> SCAN_TIK_SHL; // получить период приема
					if(delta < SCAN_INTERVAL_MIN) // откинуть дубли по 3-м каналам или другие быстрые передачи
						return 0;
					blc_ll_setScanEnable(BLC_SCAN_DISABLE, DUP_FILTER_DISABLE); // отсановить сканирование
					scan.cur_rx_tik = tt; // timestamp приема нового сообщения
					if(scan.stage) {
						if(scan.stage == SCAN_STAGE_SYNC) {
						//------- сканирование в режиме синхронизации
							if(delta - 100*SCAN_INT_TIK < scan.cfg.interval
							&& delta + 100*SCAN_INT_TIK > scan.cfg.interval) {
								scan.interval = delta;
								scan.sum_ints = delta;
								scan.sum_cnt = 1;
								scan.err_count = 0;
								scan.stage = SCAN_STAGE_LP; // вкл сканирование в LP режиме
								scan.park_secs = 0; // the source is back, start the backoff over
								scan.window = scan.cfg.win_max; // SCAN_WINDOW_MAX;
								scan.max_win = scan.cfg.win_max<<1; // SCAN_WINDOW_MAX*2;

							} else {
								scan_init();
#if SCAN_DEBUG
								u_printf("se: %u <> %u %u\n",
										scan.interval << SCAN_INT2US_SHR,
										delta << SCAN_INT2US_SHR,
										(scan.cur_rx_tik - scan.start_tik)>>4);
#endif
							}
						} else {
						//------- сканирование в LP режиме
							if(scan.err_count && scan.err_count <= SCAN_ERR_CNT_MAX)
								tt = delta / (scan.err_count + 1);
							else
								tt = delta;
							if(tt - scan.cfg.win_max < scan.interval // SCAN_WINDOW_MAX
								&& tt + scan.cfg.win_max > scan.interval) {
								delta = tt;
								if(scan.sum_cnt >= 128) {
									scan.sum_ints >>= 1;
									scan.sum_cnt >>= 1;
								}
								scan.sum_ints += delta;
								scan.sum_cnt++;
								if(scan.sum_cnt > 16) {
									if(scan.window > scan.cfg.win_min) // SCAN_WINDOW_MIN)
										scan.window -= SCAN_INT_TIK/2;
								}
								scan.interval = scan.sum_ints / scan.sum_cnt;
								// уточнять интервал ?
								scan.cfg.interval = scan.interval;
								scan.stage = SCAN_STAGE_WRK;
							}
						}
						// производить пробуждение за x мс до scan.interval, перевод в интервал маяка 0.625 ms
						set_adv_time((scan.interval - scan.window - SCAN_WINDOW_DEC) / ((SCAN_INT_TIK*1000)/ADV_INTERVAL_1S));
						scan.err_count = 0; // сброс счета ошибок приема
						//scan.max_win = SCAN_WINDOW_MAX+SCAN_WINDOW_MIN;
						scan.max_win = scan.cfg.win_max + scan.cfg.win_min;
#if SCAN_DEBUG
						u_printf("%u %u %u %u\n",
								scan.interval << SCAN_INT2US_SHR,
								delta << SCAN_INT2US_SHR,
								scan.window << SCAN_INT2US_SHR,
								(scan.cur_rx_tik - scan.start_tik)>>4);
#endif
					} else {
						//------- сканирование в режиме поиска
						set_adv_time((scan.cfg.interval - 100*SCAN_INT_TIK) / ((SCAN_INT_TIK*1000)/ADV_INTERVAL_1S));
						scan.err_count = 0; // сброс счета ошибок приема
						scan.stage = SCAN_STAGE_SYNC; // вкл старт сканирования в режиме синхронизации
						//scan.max_win = SCAN_WINDOW_MAX+SCAN_WINDOW_MIN;
						scan.max_win = scan.cfg.win_max + scan.cfg.win_min;
					}
					scan.start_tik = 0; // разрешить sleep
					//------ decode adv
					u32 adlen = pa->len;
					u8 rssi = pa->data[adlen];
					if (adlen && adlen < 32 && rssi != 0) { // rssi != 0
						u32 i = 0;
						while(adlen) {
							pad_uuid16_t pd = (pad_uuid16_t) &pa->data[i];
							u32 len = pd->size + 1;
							if(len <= adlen) {
								if(len >= sizeof(ad_uuid16_t) && pd->type == GAP_ADTYPE_SERVICE_DATA_UUID_16BIT) {
								   if(memcmp(prev_advs, pd, len)) {
									   memcpy(prev_advs, pd, len);
									   if((pd->uuid16) == ADV_BTHOME_UUID16) { // GATT Service: BTHome v2
										   filter_bthome_ad((adv_bthome_t *)prev_advs, pa->mac);
#if 0
								   	   } else if((pd->uuid16) == 0x181A) { // GATT Service 0x181A Environmental Sensing, ATC custom FW
								   		   filter_custom_ad((adv_custom_t *)pd, pa->mac);
								   	   } else if((pd->uuid16) == 0xfe95) { // GATT Service: Xiaomi Inc.
								   		   filter_xiaomi_ad((adv_xiaomi_t *)pd, pa->mac);
								   	   } else if((pd->uuid16) == 0xfdcd) { // GATT Service: Qingping Technology (Beijing) Co., Ltd.
								   		   filter_qingping_ad((adv_qingping_t *)pd, pa->mac);
#endif
								   	   }
								   }
								}
							} else
								break;
							adlen -= len;
							i += len;
						}
					}
				}
			}
#if SCAN_DEBUG
			else if (p[0] == HCI_SUB_EVT_LE_SCAN_REQUEST_RECEIVED) {
				//event_adv_report_t *pa = (event_adv_report_t *) p;
				u_array_printf((unsigned char*)p, 20);
			}
			else
				u_printf("evle:%X\n", p[0]);
#endif

		}
#if SCAN_DEBUG
		else
			u_printf("ev:%X\n", h);
#endif
	}
	return 0;
}

//////////////////////////////////////////////////////////
// scan task
//////////////////////////////////////////////////////////
_attribute_ram_code_
__attribute__((optimize("-Os")))
void scan_task(void) {
	if(!wrk.ble_connected && !wrk.scan_enable && scan.park_until
	&& wrk.utc_time_sec >= scan.park_until) {
		//------- the backoff has run out, search for the source again
		scan_init(); // clears park_until and returns to SCAN_STAGE_START
		if(wrk.scan_enable)
			start_adv_scanning();
	}
	if(!wrk.ble_connected && wrk.scan_enable) {
		u32 tt = clock_time();
		if(scan.stage > SCAN_STAGE_SYNC) {
			//----------- сканирование в LP режиме
			if (scan.start_tik) {
				tt = (tt - scan.start_tik) >> SCAN_TIK_SHL;
				if(tt > scan.max_win) {
					if(scan.err_count < SCAN_ERR_CNT_MAX) {
						//scan.max_win = 100*SCAN_INT_TIK;
						scan.max_win += scan.cfg.win_max;
						if(scan.window < scan.cfg.win_max)
							scan.window += SCAN_INT_TIK;
						blta.adv_interval -= 3*CLOCK_16M_SYS_TIMER_CLK_1MS;
						blc_ll_setScanEnable(BLC_SCAN_DISABLE, DUP_FILTER_DISABLE); // отсановить сканирование
						scan.err_count++; // счет ошибок приема
						scan.start_tik = 0;
#if SCAN_DEBUG
						u_printf("ts: %u, e:%u\n", tt << SCAN_INT2US_SHR, scan.err_count);
#endif
					} else {
						scan_init();
#if (DEV_SERVICES & SERVICE_SCREEN)
						wrk.lcd_redraw = 1;
						SHOW_FLG_ERR();
#endif
					}
#if SCAN_DEBUG_ERR
					scan.all_err++;
#endif
				}
			}
		} else if(scan.stage == SCAN_STAGE_SYNC) {
			//------- сканирование в режиме синхронизации
			tt = (tt - scan.cur_rx_tik) >> SCAN_TIK_SHL;
			if(tt > scan.cfg.interval + 100*SCAN_INT_TIK) {
				blc_ll_setScanEnable(BLC_SCAN_DISABLE, DUP_FILTER_DISABLE); // отсановить сканирование
#if (DEV_SERVICES & SERVICE_SCREEN)
				wrk.lcd_redraw = 1;
				SHOW_FLG_ERR();
#endif
				scan.err_count++;
				scan.start_tik = 0;
				scan_init();
#if SCAN_DEBUG
				u_printf("ss: %u, e:%u\n", tt << SCAN_INT2US_SHR, scan.err_count);
#endif
#if SCAN_DEBUG_ERR
				scan.all_err++;
#endif
			}
		} else { // SCAN_STAGE_START
			//------- сканирование в режиме поиска
			if (scan.start_tik) {
				// one continuous sweep, long enough to contain a beacon of the source:
				// the first uses the configured period, the second the whole legal range
				u32 sweep = scan.err_count ?
					(u32)SCAN_SWEEP_FULL_MS * CLOCK_16M_SYS_TIMER_CLK_1MS :
					(scan.cfg.interval << SCAN_TIK_SHL)
						+ SCAN_SWEEP_MARGIN_MS * CLOCK_16M_SYS_TIMER_CLK_1MS;
				tt = tt - scan.start_tik;
				if(tt > sweep) {
					blc_ll_setScanEnable(BLC_SCAN_DISABLE, DUP_FILTER_DISABLE); // отсановить сканирование
#if (DEV_SERVICES & SERVICE_SCREEN)
					wrk.lcd_redraw = 1;
					SHOW_FLG_ERR();
#endif
					scan.err_count++;
					scan.start_tik = 0;
					if(scan.err_count >= SCAN_SWEEPS) {
						// the source was not heard in a full sweep of its beacon period.
						// park: keep beaconing, stop scanning, and search again later
						if(scan.park_secs == 0)
							scan.park_secs = SCAN_PARK_SECS_FIRST;
						else if(scan.park_secs < SCAN_PARK_SECS_MAX) {
							scan.park_secs <<= 2;
							if(scan.park_secs > SCAN_PARK_SECS_MAX)
								scan.park_secs = SCAN_PARK_SECS_MAX;
						}
						scan.park_until = wrk.utc_time_sec + scan.park_secs;
						wrk.scan_enable = 0;
						// stop broadcasting the source fields: they are no longer current.
						// this device keeps reporting its own state
						bthome_parked_beacon();
						set_adv_time(SCAN_PARK_ADV);
#if (DEV_SERVICES & SERVICE_SCREEN)
						wrk.lcd_redraw = 1;
						show_scan_off();
#endif
					}
#if (DEV_SERVICES & SERVICE_SCREEN)
					else {
						wrk.lcd_redraw = 1;
						show_err_screen(scan.err_count);
					}
#endif
#if SCAN_DEBUG
					u_printf("st: %u, e:%u\n", tt >> 4, scan.err_count);
#endif
#if SCAN_DEBUG_ERR
					scan.all_err++;
#endif
				}
			}
		}
	}
}

