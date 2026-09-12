#ifndef _APP_CONFIG_H_
#define _APP_CONFIG_H_

#define SW_VERSION 0x12 // BCD format (0x34 -> '3.4')
// Revision of this fork on top of SW_VERSION, reported as the trailing letter of the
// Software Revision String ("V1.2+a"). Bump it on every release from here. It is kept
// apart from SW_VERSION on purpose: SW_VERSION is also the stored configuration
// version, and changing that risks erasing the user's settings, see app.c.
#define FORK_REV 'h' // 'a'..'z'

#define SCAN_DEBUG_ERR		1
// Temporary diagnostic: carry the time from the start of a scan to the reception in the
// advertisement, in milliseconds. It answers whether a long search sweep really listens
// for its whole length: hits spread across the whole sweep mean it does, hits clustered
// at the very start mean the stack stops scanning on its own. Turn off for release.
#define SCAN_DEBUG_TIM		0
#define UART_PRINT_DEBUG_ENABLE 0

#if UART_PRINT_DEBUG_ENABLE
#define PRINT_BAUD_RATE 1500000 // real ~1000000
#define DEBUG_INFO_TX_PIN	GPIO_PA7 // SWS
#define PA7_DATA_OUT		1
#define PA7_OUTPUT_ENABLE	1
#define PULL_WAKEUP_SRC_PA7 PM_PIN_PULLUP_1M
#define PA7_FUNC		AS_GPIO
#endif // UART_PRINT_DEBUG_ENABLE

#define DEVICE_INFO_SERVICE_ENABLE 	1 // = 1 enable Device Information Characteristics
#define BATT_SERVICE_ENABLE			1
#define OTA_SERVICE_ENABLE			1

// Left off deliberately, and reviewed. Nothing in this firmware has a reachable hang:
// the I2C and UART waits all terminate on their own, the master runs without clock
// stretching, and the flash driver feeds the watchdog itself. Nothing in the SDK sleep
// path touches it either, and whether the counter stops while the part is suspended
// cannot be settled from the headers, so turning this on risks a device that does
// nothing but reboot, in a place nobody can reach. Parking and the backoff already
// recover from the failure that actually happens, which is a source that goes quiet.
// Revisit only on evidence of a real hang in the field, when there is something to
// aim at, and then in a release of its own with a long soak test of the parked state.
#define MODULE_WATCHDOG_ENABLE		0	// WDT not use
#define WATCHDOG_INIT_TIMEOUT		250  // ms
#define BLT_SOFTWARE_TIMER_ENABLE	1

#define USE_TIME_ADJUST		1 // = 1 time correction enabled

#define BLE_SECURITY_ENABLE 0
#define BLE_HOST_SMP_ENABLE BLE_SECURITY_ENABLE

//------------------------------
// DevID:
#ifndef DEVICE_CGG1_ver
#define DEVICE_CGG1_ver		0 // =2022 - CGG1-M version 2022, or = 0 - CGG1-M version 2020,2021
#endif

#define DEVICE_MHO_C401		1	// E-Ink display MHO-C401 2020
#if DEVICE_CGG1_ver == 0
#define DEVICE_CGG1 		2  // E-Ink display Old CGG1-M "Qingping Temp & RH Monitor"
#else
#define DEVICE_CGG1 		7  // E-Ink display New CGG1-M "Qingping Temp & RH Monitor"
#endif
#define DEVICE_CGDK2 		6  // LCD display "Qingping Temp & RH Monitor Lite"
#define DEVICE_MHO_C401N	8	// E-Ink display MHO-C401 2022
#define DEVICE_MJWSD05MMC	9  // LCD (ch) display MJWSD05MMC
#define DEVICE_LYWSD03MMC	10	// LCD display LYWSD03MMC
// HW_VER_LYWSD03MMC_B14 = 0
// HW_VER_LYWSD03MMC_B19 = 3
// HW_VER_LYWSD03MMC_B16 = 4
// HW_VER_LYWSD03MMC_B17 = 5
// HW_VER_LYWSD03MMC_B15 = 10
#define DEVICE_MHO_C122		11	// LCD display MHO_C122
#define DEVICE_MJWSD05MMC_EN	12  // LCD (en) display MJWSD05MMC
//---
#define DEVICE_TB03F  		16	// DIY, TB-03F-Kit module + INA226 or MY18B20
#define DEVICE_TS0201   	17	// ZigBee TS0201, analog: IH-K009
#define DEVICE_TNK01		18	// DIY, PB-03F module, Water tank controller
//#define DEVICE_THB2		19	// PHY62x2 BLE https://github.com/pvvx/THB2
//#define DEVICE_BTH01		20	// PHY62x2 BLE https://github.com/pvvx/THB2
//#define DEVICE_TH05		21	// PHY62x2 BLE LCD https://github.com/pvvx/THB2
#define DEVICE_TH03Z   		22	// ZigBee TH03Z
//#define BOARD_THB1		23 // THB1 https://github.com/pvvx/THB2
//#define BOARD_TH05D		24 // TH05_V1.3 https://github.com/pvvx/THB2
//#define BOARD_TH05F		25 // TH05Y_V1.2 https://github.com/pvvx/THB2
//#define BOARD_THB3		26 // https://github.com/pvvx/THB2
#define DEVICE_ZTH01   		27	// ZigBee ZTH01
#define DEVICE_ZTH02   		28	// ZigBee ZTH02
#define DEVICE_PLM1 		29  // Tuya BLE Plant monitor ECF-SGS01-A rev1.3 (BT3L Tuya module)
#define DEVICE_ZTH03 		30  // Tuya TH03 Zigbee LCD
#define DEVICE_LKTMZL02		31  // Tuya LKTMZL02 Zigbee LCD 2xAAA
//#define DEVICE_KEY2		32  // KEY2 https://github.com/pvvx/THB2
#define DEVICE_ZTH05Z		33  // Tuya ZTH05ZTUv12 Zigbee LCD, AHT30, CR2032
//#define DEVICE_THB2X		34  // PHY62x2 BLE, https://github.com/pvvx/THB2/discussions/82
#define DEVICE_CB3S			35  // development is not completed! TS0041_TZ3000_fa9mlvja, Tuya ZigBee "Smart Button"
#define DEVICE_HS09			36  // development is not completed! TS0201_TZ3000_1twfmkcc: Tuya ZigBee "Smart Humidity Sensor"
#define DEVICE_ZYZTH02		37  // Tuya ZY-ZTH02 Zigbee, 2 x AAA, SHT30/CHT832x
#define DEVICE_ZYZTH01		38  // development is not completed! Tuya ZY-ZTH02Pro Zigbee LCD, 2 x AAA, SHT30/CHT832x

#ifndef DEVICE_TYPE
#define DEVICE_TYPE			DEVICE_LYWSD03MMC
#endif

//------------------------------------
// Supported services by the device (bits)
#define SERVICE_OTA			0x00000001	// OTA all enable!
//#define SERVICE_OTA_EXT	0x00000002	// Compatible BigOTA/ZigbeeOTA
#define SERVICE_PINCODE 	0x00000004	// support pin-code
//#define SERVICE_BINDKEY 	0x00000008	// support encryption beacon (bindkey)
//#define SERVICE_HISTORY 	0x00000010	// flash logger enable
#define SERVICE_SCREEN		0x00000020	// screen
//#define SERVICE_LE_LR		0x00000040	// support extension advertise + LE Long Range
#define SERVICE_THS			0x00000080	// T & H sensor
//#define SERVICE_RDS		0x00000100	// wake up when the reed switch + pulse counter
//#define SERVICE_KEY		0x00000200	// key "connect"
//#define SERVICE_OUTS		0x00000400	// GPIO output
//#define SERVICE_INS		0x00000800	// GPIO input
#define SERVICE_TIME_ADJUST 0x00001000	// time correction enabled
//#define SERVICE_HARD_CLOCK	0x00002000	// RTC enabled
//#define SERVICE_TH_TRG	0x00004000	// use TH trigger out
//#define SERVICE_LED		0x00008000	// use led
//#define SERVICE_MI_KEYS	0x00010000	// use mi keys
//#define SERVICE_PRESSURE	0x00020000	// pressure sensor
//#define SERVICE_18B20		0x00040000	// use sensor(s) MY18B20
//#define SERVICE_IUS		0x00080000	// use I and U sensor (INA226)
//#define SERVICE_PLM		0x00100000	// use PWM-RH and NTC
//#define SERVICE_BUTTON	0x00200000	// брелок-кнопка
//#define SERVICE_FINDMY	0x00400000	// FindMy
#define SERVICE_EXTENDED	0x80000000	// Scan device

#if DEVICE_TYPE == DEVICE_LYWSD03MMC

#define DEV_MODEL_STR "LYWSD03MMC"
#define DEV_SERVICES ( SERVICE_OTA\
		| SERVICE_SCREEN \
		| SERVICE_THS \
		| SERVICE_TIME_ADJUST \
		| SERVICE_PINCODE \
		| SERVICE_EXTENDED \
)
#define USE_SENSOR_SHTC3	1

#define SHL_ADC_VBAT		1  // "B0P" in adc.h
#define GPIO_VBAT			GPIO_PB0 // missing pin on case TLSR8251F512ET24
#define PB0_INPUT_ENABLE	1
#define PB0_DATA_OUT		1
#define PB0_OUTPUT_ENABLE	1
#define PB0_FUNC			AS_GPIO

#define I2C_MAX_SPEED 		700000 // 700 kHz
#define I2C_SCL 			GPIO_PC2
#define I2C_SDA 			GPIO_PC3
#define I2C_GROUP 			I2C_GPIO_GROUP_C2C3
#define PULL_WAKEUP_SRC_PC2	PM_PIN_PULLUP_10K
#define PULL_WAKEUP_SRC_PC3	PM_PIN_PULLUP_10K

#define GPIO_TRG			GPIO_PC4	// Trigger1, output, pcb mark "P9"
#define PC4_INPUT_ENABLE	1
#define PC4_DATA_OUT		0
#define PC4_OUTPUT_ENABLE	0
#define PC4_FUNC			AS_GPIO
#define PULL_WAKEUP_SRC_PC4	PM_PIN_PULLDOWN_100K

#define GPIO_KEY2			GPIO_PA5	// key "Connect", input, pcb mark "P8"
#define PA5_INPUT_ENABLE	1
#define PA5_DATA_OUT		0
#define PA5_OUTPUT_ENABLE	0
#define PA5_FUNC			AS_GPIO
#define PULL_WAKEUP_SRC_PA5 PM_PIN_PULLUP_1M

#define GPIO_LCD_URX 		UART_TX_PD7  // UART RX LCD old B1.6
#define GPIO_LCD_UTX 		UART_RX_PB7  // UART TX LCD old B1.6
#define GPIO_LCD_CLK 		GPIO_PD7  //(not change!) SPI-CLK LCD new B1.6
#define GPIO_LCD_SDI 		GPIO_PB7  //(not change!) SPI-SDI LCD new B1.6

#define PD7_INPUT_ENABLE	1
#define PD7_DATA_OUT		1
#define PD7_OUTPUT_ENABLE	1
#define PD7_FUNC			AS_GPIO
#define PULL_WAKEUP_SRC_PD7	PM_PIN_PULLUP_1M // UART TX (B1.6)

#define PB7_INPUT_ENABLE	1
#define PB7_DATA_OUT		1
#define PB7_OUTPUT_ENABLE	0
#define PB7_FUNC			AS_GPIO
#define PULL_WAKEUP_SRC_PB7	PM_PIN_PULLUP_1M // SPI CLK (B1.6 new), UART RX


#define PULL_WAKEUP_SRC_PB6 PM_PIN_PULLUP_10K // LCD on low temp needs this, its an unknown pin going to the LCD controller chip

#elif DEVICE_TYPE == DEVICE_LKTMZL02
// TLSR8258
// GPIO_PA0 - free (Reed Switch, input)
// GPIO_PA1 - free
// GPIO_PA7 - SWS, (debug TX)
// GPIO_PB1 - free
// GPIO_PB4 - LED - "1" On
// GPIO_PB5 - free, (TRG)
// GPIO_PB6 - free
// GPIO_PB7 - free
// GPIO_PC0 - KEY
// GPIO_PC1 - SDA1, used I2C LCD
// GPIO_PC2 - SDA, used I2C Sensor
// GPIO_PC3 - SCL, used I2C Sensor
// GPIO_PC4 - SCL1, used I2C LCD
// GPIO_PD2 - free
// GPIO_PD3 - free
// GPIO_PD4 - free
// GPIO_PD7 - free
#define DEV_MODEL_STR "LKTMZL02"
#define DEV_SERVICES ( SERVICE_OTA\
		| SERVICE_SCREEN \
		| SERVICE_THS \
		| SERVICE_TIME_ADJUST \
		| SERVICE_PINCODE \
		| SERVICE_EXTENDED \
)

#define ZIGBEE_TUYA_OTA 	1

#define USE_EPD				0 // min update time ms

#define USE_SENSOR_CHT8305		0
#define USE_SENSOR_CHT8215		0
#define USE_SENSOR_AHT20_30		1
#define USE_SENSOR_SHT4X		0
#define USE_SENSOR_SHTC3		0
#define USE_SENSOR_SHT30		0

#define SHL_ADC_VBAT		1  // "B0P" in adc.h
#define GPIO_VBAT			GPIO_PB0 // missing pin on case TLSR825x
#define PB0_INPUT_ENABLE	1
#define PB0_DATA_OUT		1
#define PB0_OUTPUT_ENABLE	1
#define PB0_FUNC			AS_GPIO

// I2C Sensor
#define I2C_MAX_SPEED 		700000 // 700 kHz
#define I2C_SCL 			GPIO_PC2
#define I2C_SDA 			GPIO_PC3
#define I2C_GROUP 			I2C_GPIO_GROUP_C2C3
#define PULL_WAKEUP_SRC_PC2	PM_PIN_PULLUP_10K
#define PULL_WAKEUP_SRC_PC3	PM_PIN_PULLUP_10K

// I2C LCD
#define I2C_SCL_LCD			GPIO_PC4
#define I2C_SDA_LCD			GPIO_PC1
#define PC1_INPUT_ENABLE	1
#define PC1_DATA_OUT		0
#define PC1_OUTPUT_ENABLE	0
#define PC1_FUNC			AS_GPIO
#define PC4_INPUT_ENABLE	1
#define PC4_DATA_OUT		0
#define PC4_OUTPUT_ENABLE	0
#define PC4_FUNC			AS_GPIO
#define PULL_WAKEUP_SRC_PC1	PM_PIN_PULLUP_10K
#define PULL_WAKEUP_SRC_PC4	PM_PIN_PULLUP_10K

#define GPIO_TRG			GPIO_PB5
#define PB5_INPUT_ENABLE	1
#define PB5_DATA_OUT		0
#define PB5_OUTPUT_ENABLE	0
#define PB5_FUNC			AS_GPIO
#define PULL_WAKEUP_SRC_PB5	PM_PIN_PULLDOWN_100K

#define GPIO_LED			GPIO_PB4
#define PB4_INPUT_ENABLE	1
#define PB4_DATA_OUT		1
#define PB4_OUTPUT_ENABLE	0
#define PB4_FUNC			AS_GPIO
#define PULL_WAKEUP_SRC_PB4	PM_PIN_PULLDOWN_100K

#elif DEVICE_TYPE == DEVICE_ZYZTH01

#define DEV_MODEL_STR "ZY-ZTH02Pro"
#define DEV_SERVICES ( SERVICE_OTA\
		| SERVICE_SCREEN \
		| SERVICE_THS \
		| SERVICE_TIME_ADJUST \
		| SERVICE_PINCODE \
		| SERVICE_EXTENDED \
)

#define USE_SENSOR_CHT8305		1
#define USE_SENSOR_CHT8215		0
#define USE_SENSOR_AHT20_30		1
#define USE_SENSOR_SHT4X		1
#define USE_SENSOR_SHTC3		1
#define USE_SENSOR_SHT30		1

#define SHL_ADC_VBAT		1  // "B0P" in adc.h
#define GPIO_VBAT			GPIO_PB0 // missing pin on case TLSR8251F512ET24
#define PB0_INPUT_ENABLE	1
#define PB0_DATA_OUT		1
#define PB0_OUTPUT_ENABLE	1
#define PB0_FUNC			AS_GPIO

#define I2C_MAX_SPEED 		200000 // 200 kHz
#define I2C_SCL 			GPIO_PC3
#define PC3_INPUT_ENABLE	1
#define PC3_DATA_OUT		0
#define PC3_OUTPUT_ENABLE	0
//#define PULL_WAKEUP_SRC_PC3	PM_PIN_PULLUP_10K
#define I2C_SDA 			GPIO_PD2
#define PD2_INPUT_ENABLE	1
#define PD2_DATA_OUT		0
#define PD2_OUTPUT_ENABLE	0
//#define PULL_WAKEUP_SRC_PD2	PM_PIN_PULLUP_10K

#define GPIO_KEY2			GPIO_PB4
#define PB4_INPUT_ENABLE	1
#define PB4_DATA_OUT		0
#define PB4_OUTPUT_ENABLE	0
#define PB4_FUNC			AS_GPIO
//#define PULL_WAKEUP_SRC_PB4	PM_PIN_PULLUP_1M

#define GPIO_LED			GPIO_PC2
#define PC2_INPUT_ENABLE	1
#define PC2_DATA_OUT		1
#define PC2_OUTPUT_ENABLE	0
#define PC2_FUNC			AS_GPIO
#define PULL_WAKEUP_SRC_PC2	PM_PIN_PULLDOWN_100K

#define GPIO_TRG			GPIO_PA1	// mark "TXD2"
#define PA1_INPUT_ENABLE	1
#define PA1_DATA_OUT		0
#define PA1_OUTPUT_ENABLE	0
#define PA1_FUNC			AS_GPIO
#define PULL_WAKEUP_SRC_PA1	PM_PIN_PULLDOWN_100K

#define RDS1_PULLUP			PM_PIN_PULLUP_1M
#define GPIO_RDS1 			GPIO_PA0	// mark "RXD2",  Reed Switch
#define PA0_INPUT_ENABLE	1
#define PA0_DATA_OUT		0
#define PA0_OUTPUT_ENABLE	0
#define PA0_FUNC			AS_GPIO
#define PULL_WAKEUP_SRC_PA0 RDS1_PULLUP

#elif DEVICE_TYPE == DEVICE_TB03F

#define DEV_MODEL_STR "TB0-3F"
#define DEV_SERVICES ( SERVICE_OTA\
		| SERVICE_THS \
		| SERVICE_TIME_ADJUST \
		| SERVICE_PINCODE \
		| SERVICE_EXTENDED \
)

#define SHL_ADC_VBAT		1  // "B0P" in adc.h
#define GPIO_VBAT			GPIO_PB0 // missing pin on case TLSR8251F512ET24
#define PB0_INPUT_ENABLE	1
#define PB0_DATA_OUT		1
#define PB0_OUTPUT_ENABLE	1
#define PB0_FUNC			AS_GPIO


// I2C Sensor
#define I2C_MAX_SPEED 		700000 // 700 kHz
#define I2C_SCL 			GPIO_PC1
#define I2C_SDA 			GPIO_PC0
#define I2C_GROUP 			I2C_GPIO_GROUP_C0C1
#define PULL_WAKEUP_SRC_PC0	PM_PIN_PULLUP_10K
#define PULL_WAKEUP_SRC_PC1	PM_PIN_PULLUP_10K

// PC2,3,4 - LED1,2,3
#define GPIO_OUT_TH		GPIO_PC2
#define PC2_DATA_OUT		0
#define PC2_OUTPUT_ENABLE	1
#define PC2_FUNC			AS_GPIO

#define GPIO_OUT_RDS	GPIO_PC3
#define PC3_DATA_OUT		0
#define PC3_OUTPUT_ENABLE	1
#define PC3_FUNC			AS_GPIO

#define GPIO_OUT_LM		GPIO_PC4
#define PC4_DATA_OUT		0
#define PC4_OUTPUT_ENABLE	1
#define PC4_FUNC			AS_GPIO

#endif

#define USE_AVERAGE_BATTERY	1

#define CLOCK_SYS_CLOCK_HZ  	16000000 // 16000000, 24000000, 32000000, 48000000
enum{
	CLOCK_SYS_CLOCK_1S = CLOCK_SYS_CLOCK_HZ,
	CLOCK_SYS_CLOCK_1MS = (CLOCK_SYS_CLOCK_1S / 1000),
	CLOCK_SYS_CLOCK_1US = (CLOCK_SYS_CLOCK_1S / 1000000),
};

#define pm_wait_ms(t) cpu_stall_wakeup_by_timer0(t*CLOCK_SYS_CLOCK_1MS);
#define pm_wait_us(t) cpu_stall_wakeup_by_timer0(t*CLOCK_SYS_CLOCK_1US);

#define RAM _attribute_data_retention_ // short version, this is needed to keep the values in ram after sleep

#include "vendor/common/default_config.h"

#endif //_APP_CONFIG_H_
