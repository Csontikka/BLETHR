/*
 * scaning.h
 *
 *  Created on: 20.11.2021
 *      Author: pvvx
 */

#ifndef SCANING_H_
#define SCANING_H_

enum {
	FLG_UPDATE_NONE	=	0,
	FLG_UPDATE_BAT	=	1,
	FLG_UPDATE_VBAT	=	2,
	FLG_UPDATE_TEMP	=	4,
	FLG_UPDATE_HUMI	=	8,
	FLG_UPDATE_FLG	=	0x10,
	FLG_UPDATE_ALL  =   0x1f
} FLG_UPDATE_e;

typedef struct {
	s16 temperature;	// in 0.01 C
	u16 humidity;		// in 0.01 %
	u16 voltage;		// in 1 mV
	u8  battery;		// in %
	u8  new_cnt;		//
	u8  old_cnt;		//
	u8  update;			// FLG_UPDATE_e
} ext_measure_t;

extern ext_measure_t ext_measure;

enum {
	SCAN_STAGE_START = 0,	// старт сканирования (no LP)
	SCAN_STAGE_SYNC,		// сканирование в режиме синхронизации (no LP)
	SCAN_STAGE_LP,			// переход к сканированию в LP режиме
	SCAN_STAGE_WRK			// сканированию в LP режиме
} SCAN_STAGE_e;

#define SCAN_SYN_ADV		(ADV_INTERVAL_1_28_S)
#define SCAN_PARK_ADV		(ADV_INTERVAL_10_24S) // parked: beacon only, no scanning

#define SCAN_SWEEP_FULL_MS		10500 // covers the whole configurable source range, 3000..10000 ms
// Two, because one sweep of 10.5 s holds only a single beacon of a 10 s source, so a single
// lost packet parks a device whose source is right there. Measured: at a 10 s source both
// bench devices parked within minutes and climbed the backoff, with error counts in single
// figures, which is a search failing rather than a source missing. Both sweeps are the full
// length, so the second is a real second chance rather than a subset of the first.
#define SCAN_SWEEPS				2     // sweeps of a search before the device parks

#define SCAN_PARK_SECS_FIRST	120  // first park after a failed search, 2 minutes
// Syncs failed in a row before the device parks. Both kinds of failure count: a source whose
// period disagrees, which fails every time, and one whose next beacon never arrives, which on a
// marginal link fails now and then. Five rather than three because of the second kind: at the
// third of all windows this bench loses, five in a row is a quarter of a percent per attempt,
// while a source that has really gone fails every attempt and still parks inside a minute.
#define SCAN_SYNC_FAIL_MAX		5
#define SCAN_PARK_SECS_MAX		3600 // longest park, 1 hour
// Seconds the radio must have been off before the battery is measured. A cell under 7 mA of
// receive current reads far below its open circuit voltage, and this firmware can hold the
// radio on for 10.5 s at a stretch, so the reading taken in the first main loop pass after a
// sweep is the sagged one. That reading decides a two minute deep sleep, and after a reset the
// boot check runs before the radio starts, so a cold cell that is fine unloaded can put the
// device into a sleep and reboot loop that never advertises again.
#define BATT_SETTLE_SECS		3

#define SCAN_INT_DEFAULT	5000 // 5000 ms, 5 sec

#define SCAN_TIK_SHL		7
#define SCAN_TIM_TIK		(1<<SCAN_TIK_SHL) // 128
#define SCAN_INT_TIK		(CLOCK_16M_SYS_TIMER_CLK_1MS/SCAN_TIM_TIK) // 125  1/125 ms
#define SCAN_INT2US_SHR		(SCAN_TIK_SHL - 4) // CLOCK_16M_SYS_TIMER_CLK_1US =  1<<4

#define SCAN_ERR_CNT_MAX		5
#define SCAN_WINDOW_MAX_DEF		(20*SCAN_INT_TIK) 	// +-, в 8 us (1/125 ms)
#define SCAN_WINDOW_MIN_DEF		(10*SCAN_INT_TIK) 	// +-, в 8 us (1/125 ms)
#define SCAN_WINDOW_DEC			(8*SCAN_INT_TIK) 	// +-, в 8 us (1/125 ms)
#define SCAN_INTERVAL_MIN		(500*SCAN_INT_TIK) 	// +-, в 8 us (1/125 ms)

#define SCAN_WINDOW_MAX			(50*SCAN_INT_TIK) 	// +-, в 8 us (1/125 ms)
#define SCAN_WINDOW_MIN			(5*SCAN_INT_TIK) 	// +-, в 8 us (1/125 ms)


typedef struct {
	u32	interval;		// интервал (в 1/125 ms)
	u32 win_min;		// set in config, default = SCAN_WINDOW_MIN
	u32 win_max;		// set in config, default = SCAN_WINDOW_MAX
	u8 	MAC[6]; 		// [0] - lo, .. [6] - hi digits
	u8 	bindkey[16]; // for ext dev MAC
} scan_cfg_t;

typedef struct {
	u8	stage;			// стадия сканирования
	u8	err_count;		// счетчик ошибок приема
	u16	sum_cnt;		// счетчик сумм интервалов для вычисление среднего
	u32	start_tik;		// = 0 - сканирование отключено (разрешить sleep), !=0 - штамп времени старта сканирования (в 1/16 us)
	u32	cur_rx_tik; 	// время приема (в 1/16 us)
	u32	interval;		// текщий интервал в 8 us (в 1/125 ms)
	u32	sum_ints;		// сумма интервалов для вычисление среднего в 8 us (в 1/125 ms)
	u32 window;			// окно приема в 8 us (в 1/125 ms)
	u32 max_win;		// макс окно приема в 8 us (в 1/125 ms) до err
#if SCAN_DEBUG_ERR
	u16 all_err;
#endif
#if SCAN_DEBUG_TIM
	u16 rx_tim;		// ms from the start of the scan to the last reception, 0xffff = unknown
#endif
	u32 park_until;		// mono_sec at which a parked device searches again, 0 = not parked
	u32 radio_sec;		// mono_sec while a scan is running, so 'how long has the radio been off'
	u8  sync_fail;		// syncs failed in a row; a source heard but never agreed with
	u16 park_secs;		// current backoff in seconds, 0 = the device has not parked yet
	scan_cfg_t cfg;
} scan_wrk_t;

extern scan_wrk_t scan;

void scan_init(void);
int scanning_event_callback(u32 h, u8 *p, int n);
void scan_task(void);

#endif /* SCANING_H_ */
