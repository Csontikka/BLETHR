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

#define SCAN_SWEEP_MARGIN_MS	300   // added to the configured source period for the first sweep
#define SCAN_SWEEP_FULL_MS		10500 // covers the whole configurable source range, 3000..10000 ms
#define SCAN_SWEEPS				2     // sweeps of a search before the device parks

#define SCAN_PARK_SECS_FIRST	120  // first park after a failed search, 2 minutes
#define SCAN_PARK_SECS_MAX		3600 // longest park, 1 hour

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
	u32 park_until;		// utc_time_sec at which a parked device searches again, 0 = not parked
	u16 park_secs;		// current backoff in seconds, 0 = the device has not parked yet
	scan_cfg_t cfg;
} scan_wrk_t;

extern scan_wrk_t scan;

void scan_init(void);
int scanning_event_callback(u32 h, u8 *p, int n);
void scan_task(void);

#endif /* SCANING_H_ */
