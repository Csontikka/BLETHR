/*
 * lcd.c
 *
 *  Created on: 10.03.2023
 *      Author: pvvx
 */
#include "tl_common.h"
#include "app_config.h"
#if (DEV_SERVICES & SERVICE_SCREEN)
#include "drivers.h"
#include "drivers/8258/gpio_8258.h"
#include "app.h"
#include "i2c.h"
#if (DEV_SERVICES & SERVICE_HARD_CLOCK)
#include "rtc.h"
#endif
#include "lcd.h"
#include "ble.h"
#include "bthome_adv.h"
#include "scaning.h"

RAM lcd_flg_t lcd_flg;
#if	(DEVICE_TYPE == DEVICE_ZTH03) || (DEVICE_TYPE == DEVICE_ZYZTH01)
RAM u8 display_buff[LCD_BUF_SIZE], display_cmp_buff[LCD_BUF_SIZE+1];
#else
RAM u8 display_buff[LCD_BUF_SIZE], display_cmp_buff[LCD_BUF_SIZE];
#endif

#if (!USE_EPD)
_attribute_ram_code_
void update_lcd(void){
//	if(cfg.flg.screen_off)
//		return;
#if	(DEVICE_TYPE == DEVICE_ZTH03) || (DEVICE_TYPE == DEVICE_ZYZTH01)
	if (memcmp(&display_cmp_buff[1], &display_buff, sizeof(display_buff))) {
		memcpy(&display_cmp_buff[1], &display_buff, sizeof(display_buff));
		send_to_lcd();
#else
	if (memcmp(&display_cmp_buff, &display_buff, sizeof(display_buff))) {
		send_to_lcd();
		memcpy(&display_cmp_buff, &display_buff, sizeof(display_buff));
#endif
		lcd_flg.b.send_notify = lcd_flg.b.notify_on; // set flag LCD for send notify
	}
}
#endif // !USE_EPD


_attribute_ram_code_
void send_task(void) {
	if(ext_measure.update & FLG_UPDATE_TEMP) {
		ext_measure.update &= ~FLG_UPDATE_TEMP;
		if(measured_data.temp != ext_measure.temperature || wrk.lcd_redraw) {
			measured_data.temp = ext_measure.temperature;
			if (cfg.flg.temp_F_or_C) {
				show_temp_symbol(TMP_SYM_F); // "°F"
				show_big_number_x10((s32)(((((s32)ext_measure.temperature * 9) + 25)/ 50) + 320)); // convert C to F
			} else {
				show_temp_symbol(TMP_SYM_C); // "°C"
				show_big_number_x10((ext_measure.temperature + 5)/10);
			}
#if SHOW_SMILEY
			show_smiley(LCD_SYM_SMILEY_NONE);
#endif
		}
		if(ext_measure.update & FLG_UPDATE_HUMI) {
			ext_measure.update &= ~FLG_UPDATE_HUMI;
			if(measured_data.humi != ext_measure.humidity || wrk.lcd_redraw) {
				measured_data.humi = ext_measure.humidity;
#if	(DEVICE_TYPE == DEVICE_CGG1) || (DEVICE_TYPE == DEVICE_CGDK2)
				show_small_number_x10((ext_measure.humidity+5)/10, 1);
#else
				show_small_number((ext_measure.humidity + 50)/100, 1);
#endif
#if SHOW_SMILEY
				show_smiley(LCD_SYM_SMILEY_NONE);
#endif
			}
			if(ext_measure.new_cnt != ext_measure.old_cnt) {
				ext_measure.old_cnt = ext_measure.new_cnt;
				bthome_data_beacon();
			}
		}
		wrk.lcd_redraw = 0; // a fresh packet has been drawn, the error screen is gone
	}
	show_ble_symbol(wrk.ble_connected);
	update_lcd();
}


#endif // (DEV_SERVICES & SERVICE_SCREEN)
