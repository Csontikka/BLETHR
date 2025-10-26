#ifndef _BLE_H_
#define _BLE_H_

#include "app.h"
#include "stack/ble/ble.h"

#define DEF_RF_POWER	RF_POWER_P0p04dBm
#define MTU_DATA_SIZE ATT_MTU_SIZE

extern u8 mac_public[6];
extern u8 mac_random_static[6];

typedef struct
{
  /** Minimum value for the connection event (interval. 0x0006 - 0x0C80 * 1.25 ms) */
	u16 intervalMin;
  /** Maximum value for the connection event (interval. 0x0006 - 0x0C80 * 1.25 ms) */
	u16 intervalMax;
  /** Number of LL latency connection events (0x0000 - 0x03e8) */
	u16 latency;
  /** Connection Timeout (0x000A - 0x0C80 * 10 ms) */
	u16 timeout;
} gap_periConnectParams_t;

#define DEFAULT_CONN_PARAM CONN_INTERVAL_10MS, CONN_INTERVAL_30MS, 0, CONN_TIMEOUT_1S
extern gap_periConnectParams_t my_periConnParameters;
#define UPDATE_CONN_PARAM CONN_INTERVAL_20MS, CONN_INTERVAL_20MS, 49, CONN_TIMEOUT_4S
extern gap_periConnectParams_t my_updateConnParameters;

typedef struct __attribute__((packed)) _ad_struct_head_t {
	u8 size;
	u8 type;
} ad_struct_head_t, * pad_struct_head_t;

typedef struct __attribute__((packed)) _ad_uuid16_t {
	u8 size;
	u8 type;
	u16 uuid16;
    u8 data[1]; // 1 - for check min length
} ad_uuid16_t, * pad_uuid16_t;

typedef struct __attribute__((packed)) _ad_flag_t {
	u8 size;
	u8 type;
	u8 flg;
} ad_flag_t, * pad_flag_t;

#define ADV_BUFFER_SIZE		(31 - sizeof(ad_flag_t))

#define MY_ADV_INTERVAL_MIN	(ADV_INTERVAL_1_28_S)
#define MY_ADV_INTERVAL_MAX	(ADV_INTERVAL_1_28_S)

#define BLE_NAME "BLE_000000"
#define BLE_NAME_SIZE 10

typedef struct __attribute__((packed)) _adv_name_t {
	u8 size;
	u8 type;
	u8 name[BLE_NAME_SIZE];
} adv_name_t;
extern adv_name_t adv_name;

#if (BATT_SERVICE_ENABLE)
extern u8  battery_level;
extern u16 batteryValueInCCC;
#endif

#define SERVICE_UUID_SPP 		0xffe0
#define CHARACTERISTIC_UUID_SPP (SERVICE_UUID_SPP+1)
#define SPP_SERVICE_BUF_LEN		(ATT_MTU_SIZE-3) // = 20
extern	u16 sppDataCCC;
extern	u8 sppDataBuffer[SPP_SERVICE_BUF_LEN];


///////////////////////////////////// ATT  HANDLER define ///////////////////////////////////////
typedef enum
{
	ATT_H_START = 0,

	//// Gap ////
	/**********************************************************************************************/
	GenericAccess_PS_H, 					//UUID: 2800, 	VALUE: uuid 1800
	GenericAccess_DeviceName_CD_H,			//UUID: 2803, 	VALUE:  			Prop: Read | Notify
	GenericAccess_DeviceName_DP_H,			//UUID: 2A00,   VALUE: device name
	GenericAccess_Appearance_CD_H,			//UUID: 2803, 	VALUE:  			Prop: Read
	GenericAccess_Appearance_DP_H,			//UUID: 2A01,	VALUE: appearance
	CONN_PARAM_CD_H,						//UUID: 2803, 	VALUE:  			Prop: Read
	CONN_PARAM_DP_H,						//UUID: 2A04,   VALUE: connParameter

	//// Gatt ////
	/**********************************************************************************************/
	GenericAttribute_PS_H,					//UUID: 2800, 	VALUE: uuid 1801
	GenericAttribute_ServiceChanged_CD_H,	//UUID: 2803, 	VALUE:  			Prop: Indicate
	GenericAttribute_ServiceChanged_DP_H,   //UUID:	2A05,	VALUE: service change
	GenericAttribute_ServiceChanged_CCB_H,	//UUID: 2902,	VALUE: serviceChangeCCC

#if (DEVICE_INFO_SERVICE_ENABLE)
	//// device information ////
	/**********************************************************************************************/
	DeviceInformation_PS_H,				//UUID: 2800, 	VALUE: uuid 180A
	DeviceInformation_ModName_CD_H,		//UUID: 2803, 	VALUE: Prop: Read
	DeviceInformation_ModName_DP_H,		//UUID: 2A24,	VALUE: Model Number String
	DeviceInformation_SerialN_CD_H,		//UUID: 2803, 	VALUE: Prop: Read
	DeviceInformation_SerialN_DP_H,		//UUID: 2A25,	VALUE: Serial Number String
	DeviceInformation_FirmRev_CD_H,		//UUID: 2803, 	VALUE: Prop: Read
	DeviceInformation_FirmRev_DP_H,		//UUID: 2A26,	VALUE: Firmware Revision String
	DeviceInformation_HardRev_CD_H,		//UUID: 2803, 	VALUE: Prop: Read
	DeviceInformation_HardRev_DP_H,		//UUID: 2A27,	VALUE: Hardware Revision String
	DeviceInformation_SoftRev_CD_H,		//UUID: 2803, 	VALUE: Prop: Read
	DeviceInformation_SoftRev_DP_H,		//UUID: 2A28,	VALUE: Software Revision String
	DeviceInformation_ManName_CD_H,		//UUID: 2803, 	VALUE: Prop: Read
	DeviceInformation_ManName_DP_H,		//UUID: 2A29,	VALUE: Manufacturer Name String
#endif
#if (BATT_SERVICE_ENABLE)
	//// Battery service ////
	/**********************************************************************************************/
	BATT_PS_H, 							//UUID: 2800, 	VALUE: uuid 180f
	BATT_LEVEL_INPUT_CD_H,				//UUID: 2803, 	VALUE: Prop: Read | Notify
	BATT_LEVEL_INPUT_DP_H,				//UUID: 2A19 	VALUE: batVal
	BATT_LEVEL_INPUT_CCB_H,				//UUID: 2902, 	VALUE: batValCCC
#endif
	//// SPP Server2Client service ////
	/**********************************************************************************************/
	SPP_PS_H, 							//UUID: 2800, 	VALUE: FFE0
	SPP_Server2Client_CD_H,				//UUID: 2803, 	VALUE: Prop: Read | write_without_rsp | Notify
	SPP_Server2Client_DP_H,				//UUID: FFE1,   VALUE: SppDataBuffer
	SPP_Server2Client_CCB_H,			//UUID: 2902 	VALUE: SppDataCCC
	SPP_Server2Client_DESC_H,			//UUID: 2901, 	VALUE: my_SppName
#if (OTA_SERVICE_ENABLE)
	//// Telink OTA ////
	/**********************************************************************************************/
	OTA_PS_H, 							//UUID: 2800, 	VALUE: telink ota service uuid
	OTA_CMD_OUT_CD_H,					//UUID: 2803, 	VALUE: Prop: read | write_without_rsp
	OTA_CMD_OUT_DP_H,					//UUID: telink ota uuid,  VALUE: otaData
	OTA_CMD_OUT_DESC_H,					//UUID: 2901, 	VALUE: otaName
#endif
	ATT_END_H,

}ATT_HANDLE;

//extern attribute_t my_Attributes[ATT_END_H];

void init_ble();
void my_att_init();
void start_adv_scanning(void);
void stop_adv_scanning(void);

void ble_set_default_name(void);

void ev_adv_timeout(u8 e, u8 *p, int n);

#if (BATT_SERVICE_ENABLE)
inline void ble_send_battery(void) {
	bls_att_pushNotifyData(BATT_LEVEL_INPUT_DP_H, (u8 *) &battery_level, 1);
}
#endif

void send_debug(void *soo, int len);

#endif // _BLE_H_
