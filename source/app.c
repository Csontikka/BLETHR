#include "tl_common.h"
#include "drivers.h"
#include "stack/ble/ble.h"
#include "vendor/common/blt_common.h"
#include "cmd_parser.h"
#include "battery.h"
#include "ble.h"
#include "app.h"
#include "scaning.h"
#include "flash_eep.h"
#include "bthome_adv.h"
#if (DEV_SERVICES & SERVICE_SCREEN)
#include "lcd.h"
#include "i2c.h"
#endif

void app_enter_ota_mode(void);

RAM measured_data_t measured_data;
RAM wrk_data_t wrk;
RAM dev_cfg_t cfg;

const dev_cfg_t def_cfg = {
		.rf_tx_power = DEF_RF_POWER,
		.win_min = SCAN_WINDOW_MIN_DEF,
		.win_max = SCAN_WINDOW_MAX_DEF
};

#define measurement_step_time 30 // sec


#if (DEV_SERVICES & SERVICE_BINDKEY)
RAM u8 bindkey[16];
#endif

#if USE_AVERAGE_BATTERY
//--- check battery
#define BAT_AVERAGE_COUNT	32
RAM struct {
	u32	buf_sum;
	u8	count;
} bat_average;
#endif


void test_config(void) {
	if (cfg.rf_tx_power & BIT(7)) {
		if (cfg.rf_tx_power < RF_POWER_N25p18dBm)
			cfg.rf_tx_power = RF_POWER_N25p18dBm;
		else if (cfg.rf_tx_power > RF_POWER_P3p01dBm)
			cfg.rf_tx_power = RF_POWER_P3p01dBm;
	} else {
		if (cfg.rf_tx_power < RF_POWER_P3p23dBm)
			cfg.rf_tx_power = RF_POWER_P3p23dBm;
		else if (cfg.rf_tx_power > RF_POWER_P10p46dBm)
			cfg.rf_tx_power = RF_POWER_P10p46dBm;
	}

	if(cfg.scan_interval) {
		if(cfg.scan_interval < 3000)
			cfg.scan_interval = 3000;
		else if(cfg.scan_interval > 10000)
			cfg.scan_interval = 10000;
	}

	if(cfg.win_min < SCAN_WINDOW_MIN)
		cfg.win_min = SCAN_WINDOW_MIN;
	else if(cfg.win_min > SCAN_WINDOW_MAX)
		cfg.win_min = SCAN_WINDOW_MAX;

	if(cfg.win_max < SCAN_WINDOW_MIN)
		cfg.win_max = SCAN_WINDOW_MIN;
	else if(cfg.win_max > SCAN_WINDOW_MAX)
		cfg.win_max = SCAN_WINDOW_MAX;

	if(cfg.win_max < cfg.win_min)
		cfg.win_max = cfg.win_min;
}

//--- check battery
_attribute_ram_code_
u8 check_battery(u16 bmv) {
	measured_data.battery_mv = get_battery_mv();
	if (measured_data.battery_mv < bmv) {
#if USE_SENSOR_SHTC3
		send_i2c_word(0x70 << 1, 0x98b0); // SHTC3 go SLEEP: Sleep command of the sensor
#endif // USE_SENSOR_SHTC3
#if (DEVICE_TYPE ==	DEVICE_LYWSD03MMC) || (DEVICE_TYPE == DEVICE_CGDK2) || (DEVICE_TYPE == DEVICE_MJWSD05MMC) || (DEVICE_TYPE == DEVICE_MHO_C122) || (DEVICE_TYPE == DEVICE_MJWSD05MMC_EN)
		// Set sleep power < 1 uA
		send_i2c_byte(0x3E << 1, 0xEA); // BU9792AFUV reset
#elif (DEVICE_TYPE == DEVICE_ZTH03) || (DEVICE_TYPE == DEVICE_LKTMZL02)
extern int soft_i2c_send_byte(u8 addr, u8 b);
		soft_i2c_send_byte(0x3E << 1, 0xD0);
#endif
		cpu_sleep_wakeup(DEEPSLEEP_MODE, PM_WAKEUP_TIMER,
			clock_time() + 120 * CLOCK_16M_SYS_TIMER_CLK_1S); // go deep-sleep 2 minutes
	}
#if USE_AVERAGE_BATTERY
	bat_average.buf_sum += measured_data.battery_mv;
	bat_average.count++;
	measured_data.average_battery_mv = bat_average.buf_sum / bat_average.count;
	measured_data.battery_level = get_battery_level(measured_data.average_battery_mv);
	if(bat_average.count >= BAT_AVERAGE_COUNT) {
		bat_average.buf_sum >>= 1;
		bat_average.count >>= 1;
	}
#else
	measured_data.battery_level = get_battery_level(measured_data.battery_mv);
#endif
#if (BATT_SERVICE_ENABLE)
	battery_level = measured_data.battery_level; // the value behind GATT 0x2A19
#endif
	return measured_data.battery_level;
}

//------------------ user_init_normal -------------------
void user_init_normal(void) {
//this will get executed one time after power up
	blc_ll_initBasicMCU(); //must
	adc_power_on_sar_adc(0); // - 0.4 mA
	lpc_power_down();
	check_battery(MIN_VBAT_MV); // 2.2V
	flash_unlock();
	random_generator_init(); //must
	// Returns false and erases the four configuration sectors at 0x7C000..0x7FFFF when
	// the stored version is missing or below the first argument. Two rules follow, and
	// breaking either of them costs the user the source MAC, the interval, the bind key
	// and the clock correction: SW_VERSION must stay at 0x10 or above, so the version
	// this build stores is not rejected by a later one, and the minimum must stay at
	// 0x1010, so this firmware does not wipe a device coming from a stock 1.x release.
	if(flash_supported_eep_ver(0x1010, 0x1000+SW_VERSION)) {
		if(flash_read_cfg(&cfg, EEP_ID_DEV_CFG, sizeof(cfg)) != sizeof(cfg))
			memcpy(&cfg, &def_cfg, sizeof(cfg));
#if USE_TIME_ADJUST
		if(flash_read_cfg(&wrk.utc_time_tick_step, EEP_ID_TIMAD, sizeof(wrk.utc_time_tick_step)) != sizeof(wrk.utc_time_tick_step))
			wrk.utc_time_tick_step = CLOCK_16M_SYS_TIMER_CLK_1S;
#endif
		flash_read_cfg(&scan.cfg.MAC, EEP_ID_EMAC, sizeof(scan.cfg.MAC));
		flash_read_cfg(&scan.cfg.bindkey, EEP_ID_EBKEY, sizeof(scan.cfg.bindkey));
#if (DEV_SERVICES & SERVICE_PINCODE)
		if(flash_read_cfg(&wrk.pincode, EEP_ID_PINCD, sizeof(wrk.pincode)) != sizeof(wrk.pincode))
			wrk.pincode = 0;
#endif
	} else {
		memcpy(&cfg, &def_cfg, sizeof(cfg));
#if USE_TIME_ADJUST
		wrk.utc_time_tick_step = CLOCK_16M_SYS_TIMER_CLK_1S;
#endif
#if (DEV_SERVICES & SERVICE_PINCODE)
		wrk.pincode = 0;
#endif
	}
	test_config();
	init_ble();
#if USE_SENSOR_SHTC3
	send_i2c_word(0x70 << 1, 0x98b0); // SHTC3 go SLEEP: Sleep command of the sensor
#endif // USE_SENSOR_SHTC3
#if (DEV_SERVICES & SERVICE_SCREEN)
	init_lcd();
	show_err_screen(0);
#endif
//	bls_set_advertise_prepare(app_advertise_prepare_handler); // TODO: not work if EXTENDED_ADVERTISING
	scan_init();
//	bls_app_registerEventCallback(BLT_EV_FLAG_SUSPEND_EXIT, &suspend_exit_cb);
//	bls_app_registerEventCallback(BLT_EV_FLAG_SUSPEND_ENTER, &suspend_enter_cb);
//	bls_pm_setSuspendMask(SUSPEND_ADV | SUSPEND_CONN | DEEPSLEEP_RETENTION_ADV );
}

//------------------ user_init_deepRetn -------------------
_attribute_ram_code_
void user_init_deepRetn(void) {
//after sleep this will get executed
	blc_ll_initBasicMCU();
	rf_set_power_level_index(cfg.rf_tx_power);
	blc_ll_recoverDeepRetention();
	if(!wrk.ble_connected && wrk.scan_enable) {
		start_adv_scanning();
	}
#if (OTA_SERVICE_ENABLE)
	bls_ota_registerStartCmdCb(app_enter_ota_mode);
#endif
}

#if (DEV_SERVICES & SERVICE_SCREEN) == 0
_attribute_ram_code_
void send_task(void) {
	if(ext_measure.update & FLG_UPDATE_TEMP) {
		ext_measure.update &= ~FLG_UPDATE_TEMP;
		if(measured_data.temp != ext_measure.temperature) {
			measured_data.temp = ext_measure.temperature;
		}
		if(ext_measure.update & FLG_UPDATE_HUMI) {
			ext_measure.update &= ~FLG_UPDATE_HUMI;
			if(measured_data.humi != ext_measure.humidity) {
				measured_data.humi = ext_measure.humidity;
			}
			if(ext_measure.new_cnt != ext_measure.old_cnt) {
				ext_measure.old_cnt = ext_measure.new_cnt;
				bthome_data_beacon();
			}
		}
	}
}
#endif

//----------------------- main_loop()
_attribute_ram_code_ void main_loop(void) {
	blt_sdk_main_loop();
	while(clock_time() -  wrk.utc_time_sec_tick > wrk.utc_time_tick_step) {
		wrk.utc_time_sec_tick += wrk.utc_time_tick_step;
		wrk.utc_time_sec++; // + 1 sec
		wrk.mono_sec++;
	}
#if	(OTA_SERVICE_ENABLE)
	if (!wrk.ota_is_working) {
#endif
		if (wrk.ble_connected && wrk.mono_sec - wrk.conn_sec >= CONN_MAX_SECS) {
			bls_ll_terminateConnection(HCI_ERR_REMOTE_USER_TERM_CONN); // a client left it open
			wrk.conn_sec = wrk.mono_sec; // do not ask again while the link tears down
		}
#if	(BATT_SERVICE_ENABLE)
		if(wrk.send_measure) {
			if (batteryValueInCCC && (blc_ll_getCurrentState() & BLS_LINK_STATE_CONN))
					ble_send_battery();
			wrk.send_measure = 0;
		}
#endif
		// Not while the radio is receiving, and not in the seconds after it stops either. A cell
		// under receive current reads far below its open circuit voltage, and a sweep here can
		// hold the radio on for 10.5 s, so the first pass after one sees the sagged reading. That
		// reading decides a two minute deep sleep, and the boot check after the reset runs before
		// the radio starts, so a cold cell that is fine unloaded can end up in a sleep and reboot
		// loop that never advertises again. Waiting a few seconds costs nothing: the measurement
		// is on a 30 s tick and the radio is off almost all of the time in low power mode.
		if (!scan.start_tik
		&& wrk.mono_sec - scan.radio_sec >= BATT_SETTLE_SECS
		&& wrk.mono_sec - wrk.tim_measure >= measurement_step_time) {
#if	(BATT_SERVICE_ENABLE)
			wrk.send_measure = 1;
#endif
			wrk.tim_measure = wrk.mono_sec;
#if (DEV_SERVICES & SERVICE_SCREEN)
			show_battery_symbol(check_battery(END_VBAT_MV) <= 5);
#endif
			if (!wrk.scan_enable && scan.park_until)
				bthome_parked_beacon(); // parked: keep our own reading current
		}
		scan_task();
		if (scan.start_tik) { // 	if (blts.scan_en & 1) // (scan.start_tik)
			// Pushed forward while one of the LONG scans is open, so elsewhere
			// 'mono_sec - radio_sec' is how long it has been since the radio was on for a
			// stretch. Only the search and the wait for a sync qualify, both of which can run
			// for ten seconds. A low power window is ten to thirty milliseconds and barely
			// moves the cell, which matters: at the fastest source those windows come every
			// three seconds, and counting them here would block the battery measurement for
			// good instead of delaying it.
			if (scan.stage <= SCAN_STAGE_SYNC)
				scan.radio_sec = wrk.mono_sec;
			bls_pm_setSuspendMask(SUSPEND_DISABLE);
		}
		else {
			send_task();
			bls_pm_setSuspendMask(SUSPEND_ADV | SUSPEND_CONN | DEEPSLEEP_RETENTION_ADV | DEEPSLEEP_RETENTION_CONN );
		}
#if	(OTA_SERVICE_ENABLE)
	} else { // OTA
		if ((wrk.ble_connected & BIT(CONNECTED_FLG_PAR_UPDATE)) == 0)
			bls_pm_setManualLatency(0);
		bls_pm_setSuspendMask(SUSPEND_ADV | SUSPEND_CONN);
	}
#endif
}
