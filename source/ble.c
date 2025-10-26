#include "tl_common.h"
#include "drivers.h"
#include "app_config.h"
#include "drivers/8258/gpio_8258.h"
#include "ble.h"
#include "vendor/common/blt_common.h"
#include "cmd_parser.h"
#include "app.h"
#include "lcd.h"
#include "scaning.h"
#include "bthome_adv.h"

void bls_set_advertise_prepare(void *p); // add ll_adv.h

RAM u8 blt_rxfifo_b[64 * 8] = { 0 };
RAM my_fifo_t blt_rxfifo = { 64, 8, 0, 0, blt_rxfifo_b, };
RAM u8 blt_txfifo_b[40 * 16] = { 0 };
RAM my_fifo_t blt_txfifo = { 40, 16, 0, 0, blt_txfifo_b, };

RAM u8 mac_public[6];
u8 mac_random_static[6];

RAM adv_name_t adv_name = {
		.size = BLE_NAME_SIZE + 1,
		.type = GAP_ADTYPE_LOCAL_NAME_COMPLETE, // Complete local name
		.name = BLE_NAME
};

#if	(OTA_SERVICE_ENABLE)
void app_enter_ota_mode(void) {
	// chk_ota_clear(); //
#ifdef NO_CLR_OTA_AREA
	bls_ota_clearNewFwDataArea();
#endif
	wrk.ota_is_working = 1;
	bls_ota_setTimeout(45 * 1000000); // set OTA timeout  45 seconds
	wrk.ble_connected &= ~ BIT(CONNECTED_FLG_PAR_UPDATE);
	bls_pm_setManualLatency(0);
	SHOW_OTA_SCREEN();
}
#endif // OTA_SERVICE_ENABLE

void ble_connect_callback(u8 e, u8 *p, int n) {
	wrk.ble_connected = BIT(CONNECTED_FLG_ENABLE);
	blc_ll_setScanEnable(BLC_SCAN_DISABLE, DUP_FILTER_DISABLE); // отсановить сканирование
	wrk.scan_enable = 0;
	scan.start_tik = 0; // разрешить sleep
	bls_l2cap_requestConnParamUpdate (
			my_updateConnParameters.intervalMin,
			my_updateConnParameters.intervalMax,
			my_updateConnParameters.latency,
			my_updateConnParameters.timeout
			);
	bls_l2cap_setMinimalUpdateReqSendingTime_after_connCreate(1000);
}

int app_conn_param_update_response(u8 id, u16  result) {
	if(result == CONN_PARAM_UPDATE_ACCEPT)
		wrk.ble_connected |= BIT(CONNECTED_FLG_PAR_UPDATE);
	else if(result == CONN_PARAM_UPDATE_REJECT) {
		// bls_l2cap_requestConnParamUpdate(160, 160, 4, 300); // (200 ms, 200 ms, 1 s, 3 s)
		bls_l2cap_requestConnParamUpdate (
				my_updateConnParameters.intervalMin,
				my_updateConnParameters.intervalMax,
				my_updateConnParameters.latency,
				my_updateConnParameters.timeout
				);
	}
	return 0;
}

/*
 * bls_app_registerEventCallback (BLT_EV_FLAG_ADV_DURATION_TIMEOUT, &ev_adv_timeout);
 * blt_event_callback_t(): */
_attribute_ram_code_ void ev_adv_timeout(u8 e, u8 *p, int n) {
	(void) e; (void) p; (void) n;
	bls_ll_setAdvParam(MY_ADV_INTERVAL_MIN, MY_ADV_INTERVAL_MAX,
			ADV_TYPE_CONNECTABLE_UNDIRECTED, OWN_ADDRESS_PUBLIC, 0, NULL,
			BLT_ENABLE_ADV_ALL, ADV_FP_NONE);
	bls_ll_setAdvEnable(1);
}

void ble_disconnect_callback(u8 e, u8 *p, int n) {
	if(wrk.ble_connected & BIT(CONNECTED_FLG_RESET_OF_DISCONNECT)) // reset device on disconnect?
		start_reboot();
	bls_pm_setManualLatency(0); // ?
	wrk.ble_connected = 0;
	wrk.ota_is_working = 0;
	bls_ll_setAdvData((u8 *)&adv_buf, 3);
	ev_adv_timeout(0,0,0);
	scan_init();
}

const char* hex_ascii = { "0123456789ABCDEF" };

__attribute__((optimize("-Os")))
void ble_set_default_name(void) {
	//Set the BLE Name to the last three MACs the first ones are always the same
	u8 *p = adv_name.name;
	*p++ = 'S';
	*p++ = 'T';
	*p++ = 'H';
	*p++ = '_';
	*p++ = hex_ascii[mac_public[2] >> 4];
	*p++ = hex_ascii[mac_public[2] & 0x0f];
	*p++ = hex_ascii[mac_public[1] >> 4];
	*p++ = hex_ascii[mac_public[1] & 0x0f];
	*p++ = hex_ascii[mac_public[0] >> 4];
	*p = hex_ascii[mac_public[0] & 0x0f];
}

#if (DEV_SERVICES & SERVICE_PINCODE)
int app_host_event_callback(u32 h, u8 *para, int n) {
	(void) para; (void) n;
	u8 event = (u8)h;
	if (event == GAP_EVT_SMP_TK_DISPALY) { // PK_Resp_Dsply_Init_Input
			//u32 *pinCode = (u32*) para;
			u32 * p = (u32 *)&smp_param_own.paring_tk[0];
			memset(p, 0, sizeof(smp_param_own.paring_tk));
			p[0] = wrk.pincode;
	}
	return 0;
}
#endif

__attribute__((optimize("-Os")))
void init_ble(void) {
	////////////////// BLE stack initialization //////////////////////

	blc_initMacAddress(CFG_ADR_MAC, mac_public, mac_random_static);

	/// if bls_ll_setAdvParam( OWN_ADDRESS_RANDOM ) ->  blc_ll_setRandomAddr(mac_random_static);
	ble_set_default_name();
	////// Controller Initialization  //////////
	// blc_ll_initBasicMCU(); // in user_init_normal()
	blc_ll_initStandby_module(mac_public); //must
	blc_ll_initAdvertising_module(mac_public); // adv module: 		 must for BLE slave,
	blc_ll_initConnection_module(); // connection module  must for BLE slave/master
	blc_ll_initSlaveRole_module(); // slave module: 	 must for BLE slave,
	blc_ll_initPowerManagement_module(); //pm module:      	 optional
	////// Host Initialization  //////////
	blc_gap_peripheral_init();
	my_att_init(); //gatt initialization
	blc_l2cap_register_handler(blc_l2cap_packet_receive);
	//Smp Initialization may involve flash write/erase(when one sector stores too much information,
	//   is about to exceed the sector threshold, this sector must be erased, and all useful information
	//   should re_stored) , so it must be done after battery check
#if (DEV_SERVICES & SERVICE_PINCODE)
	if(wrk.pincode) {
		//set security level: "LE_Security_Mode_1_Level_3"
		blc_smp_setSecurityLevel(Authenticated_Paring_with_Encryption); //if not set, default is : LE_Security_Mode_1_Level_2(Unauthenticated_Paring_with_Encryption)
		blc_smp_setParingMethods(LE_Secure_Connection);
		blc_smp_enableAuthMITM(1);
		//blc_smp_setBondingMode(Bondable_Mode);	// if not set, default is : Bondable_Mode
		blc_smp_setIoCapability(IO_CAPABILITY_DISPLAY_ONLY);	// if not set, default is : IO_CAPABILITY_NO_INPUT_NO_OUTPUT
		//Smp Initialization may involve flash write/erase(when one sector stores too much information,
		//   is about to exceed the sector threshold, this sector must be erased, and all useful information
		//   should re_stored) , so it must be done after battery check
		//Notice:if user set smp parameters: it should be called after usr smp settings
		blc_smp_peripheral_init();
		// Hid device on android7.0/7.1 or later version
		// New paring: send security_request immediately after connection complete
		// reConnect:  send security_request 1000mS after connection complete. If master start paring or encryption before 1000mS timeout, slave do not send security_request.
		//host(GAP/SMP/GATT/ATT) event process: register host event callback and set event mask
		blc_smp_configSecurityRequestSending(SecReq_IMM_SEND, SecReq_PEND_SEND, 1000); //if not set, default is:  send "security request" immediately after link layer connection established(regardless of new connection or reconnection )
		blc_gap_registerHostEventHandler(app_host_event_callback);
		blc_gap_setEventMask(GAP_EVT_MASK_SMP_TK_DISPALY);
	} else
#endif
		blc_smp_setSecurityLevel(No_Security);

	///////////////////// USER application initialization ///////////////////
	bls_ll_setScanRspData((u8 *) &adv_name, adv_name.size + 1);
	rf_set_power_level_index(cfg.rf_tx_power);
	// bls_app_registerEventCallback(BLT_EV_FLAG_SUSPEND_EXIT, &user_set_rf_power);
	bls_app_registerEventCallback(BLT_EV_FLAG_CONNECT, &ble_connect_callback);
	bls_app_registerEventCallback(BLT_EV_FLAG_TERMINATE, &ble_disconnect_callback);

	///////////////////// Power Management initialization///////////////////
	bls_pm_setSuspendMask(SUSPEND_DISABLE);
	blc_pm_setDeepsleepRetentionThreshold(50, 30);
	blc_pm_setDeepsleepRetentionEarlyWakeupTiming(240);
	blc_pm_setDeepsleepRetentionType(DEEPSLEEP_MODE_RET_SRAM_LOW32K);

#if	(OTA_SERVICE_ENABLE)
	bls_ota_clearNewFwDataArea();
	bls_ota_registerStartCmdCb(app_enter_ota_mode);
#endif // OTA_SERVICE_ENABLE
	blc_l2cap_registerConnUpdateRspCb(app_conn_param_update_response);
#if (MTU_DATA_SIZE > ATT_MTU_SIZE)
	blc_att_setRxMtuSize(MTU_DATA_SIZE); 	// If not set RX MTU size, default is: 23 bytes, max 247?
#endif // MTU_DATA_SIZE
	bls_ll_setAdvData((u8 *)&adv_buf, 3);
	// bls_set_advertise_prepare(app_advertise_prepare_handler);
	ev_adv_timeout(0,0,0);
}

_attribute_ram_code_
void start_adv_scanning(void) {
	scan.start_tik = clock_time() | 1;
	//scan setting
	blc_ll_initScanning_module(mac_public);
	//bluetooth low energy(LE) event
	blc_hci_le_setEventMask_cmd(HCI_LE_EVT_MASK_ADVERTISING_REPORT | HCI_LE_EVT_MASK_SCAN_REQUEST_RECEIVED);
	blc_hci_registerControllerEventHandler(scanning_event_callback); //controller hci event to host all processed in this func
	//set scan parameter and scan enable
	blc_ll_setScanParameter(SCAN_TYPE_PASSIVE, SCAN_INTERVAL_100MS, SCAN_INTERVAL_100MS,
							  OWN_ADDRESS_PUBLIC, SCAN_FP_ALLOW_ADV_ANY);
	blc_ll_setScanEnable(BLC_SCAN_ENABLE, DUP_FILTER_DISABLE);
	blc_ll_addScanningInAdvState();  //add scan in adv state
}
