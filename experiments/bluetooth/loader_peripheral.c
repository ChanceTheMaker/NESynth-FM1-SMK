/* SPDX-License-Identifier: GPL-3.0-only */
/* Included in the generated Hortator BLE unit in place of ble_central.c.
 * Startup/scheduler integration adapted from DEADACTIVE's Hortator 7e341597.
 * ATT/advertising API follows JieLi's peripheral SDK and Felucca diagnostics.
 * Cooperative CPU0 only. Callback work is bounded and never writes flash. */
#include "diagnostic_gatt.h"
extern void np_loader_connected(void);
extern int np_loader_write(const uint8_t *,unsigned);
extern unsigned np_loader_read(uint16_t,uint8_t *,uint16_t);
extern void att_server_init(const uint8_t *,uint16_t (*)(uint16_t,uint16_t,uint16_t,uint8_t *,uint16_t),int (*)(uint16_t,uint16_t,uint16_t,uint16_t,uint8_t *,uint16_t));
extern void att_server_register_packet_handler(btstack_packet_handler_t);
extern void sm_set_request_security(int);
#ifdef NP_LOADER_FM1
extern void bt_max_pwr_set(uint8_t,uint8_t,uint8_t,uint8_t);
#endif
static np_diag_gatt np_gatt;
static uint32_t np_state,np_error,np_deadline;
static uint8_t np_advertise,np_tune_pending;
static uint32_t np_tune_due;
/* AC79 ble_api.h: conn_update_param_t, BLE_CMD_REQ_CONN_PARAM_UPDATE=11.
 * 15-30 ms, zero peripheral latency; refusal leaves the link usable. */
static const uint16_t np_fast_connection[4]={12,24,0,400};
/* ble_mac is shared with the SDK syscfg_read/get_chip_id callbacks. */
static uint8_t np_att_ram[BLE_SDK_ATT_CTRL_BLOCK_SIZE+247u+512u] __attribute__((aligned(4)));
/* State: 0 off, 1 starting, 2 advertising pending, 3 advertising,
 * 4 connected, 5 failed. Exported for USB diagnostics later. */
uint32_t np_ble_diag_state(void){return np_state;}
uint32_t np_ble_diag_error(void){return np_error;}
uint32_t np_ble_diag_writes(void){return np_gatt.writes;}
static void np_tune_after_discovery(void){
    if(!np_tune_pending){np_tune_pending=1;np_tune_due=core_ms()+100;}
}
static uint16_t np_read(uint16_t h,uint16_t a,uint16_t o,uint8_t *p,uint16_t n){if(!np_gatt.connected||h!=np_gatt.handle)return 0;if(a==6){np_tune_after_discovery();return np_loader_read(o,p,n);}return np_diag_read(&np_gatt,h,a,o,p,n);}
static int np_write(uint16_t h,uint16_t a,uint16_t t,uint16_t o,uint8_t *p,uint16_t n){if(!np_gatt.connected||h!=np_gatt.handle||a!=6)return 3;if(t)return 6;if(o)return 7;np_tune_after_discovery();return np_loader_write(p,n);}
static void np_packet_impl(uint8_t type,uint16_t channel,uint8_t *p,uint16_t n,int controller){
    (void)channel;
    int event=np_diag_event(&np_gatt,type,p,n,controller);
    if(event==1){
        np_loader_connected();
        int result=ble_op_att_send_init(np_gatt.handle,np_att_ram,sizeof np_att_ram,247);
        np_advertise=0;
        if(result){np_error=(uint32_t)result;np_state=5;ble_op_disconnect(np_gatt.handle);}else {np_state=4;np_tune_pending=0;}
    }else if(event==2){
        np_tune_pending=0;
        ble_op_att_send_init(0,0,0,0);
        if(np_state!=5){np_advertise=1;np_state=2;}
    }else if(event==3&&np_state!=5){
        np_advertise=1;np_state=2;
    }
}
static void np_packet(uint8_t t,uint16_t c,uint8_t *p,uint16_t n){np_packet_impl(t,c,p,n,0);}
static void np_controller_packet(uint8_t t,uint16_t c,uint8_t *p,uint16_t n){np_packet_impl(t,c,p,n,1);}
void ble_profile_init(void){
    ble_stage(6);le_device_db_init();ble_stack_gatt_role(0);sm_init();
    sm_set_io_capabilities(3);sm_set_authentication_requirements(0);
    sm_set_encryption_key_size_range(7,16);sm_set_request_security(0);
    att_server_init(np_diag_db,np_read,np_write);
    att_server_register_packet_handler(np_packet);hci_event_callback_set(np_controller_packet);
    le_l2cap_register_packet_handler(np_controller_packet);ble_vendor_set_default_att_mtu(247);
}
static void ble_on_event(int from,struct bt_event *e){
    if(from==BLE_SDK_BT_EVENT_FROM_CON&&e&&e->event==BLE_SDK_BT_STATUS_INIT_OK&&np_state==1){
        ble_stage(7);np_state=2;np_advertise=1;
    }
}
void np_ble_diag_start(void){
    if(np_state)return;
    if(!ble_rf_trim_load()){np_error=1;np_state=5;return;}
    uint32_t x=fm1_ticks()^0x4e53424cu;
    for(unsigned i=0;i<6;i++){x=x*1664525u+1013904223u;ble_mac[i]=(uint8_t)(x>>24);}
    ble_mac[0]|=0xc0;ble_mac[5]|=0xc0; /* non-persistent local diagnostic identity */
    ble_os_init(ble_heap,BLE_HEAP_BYTES);ble_delay_calibrate();ble_os_trace=&ble_diag.task;
    ble_stage(1);core_ble_stack_window();ble_stage(2);
    np_state=1;np_deadline=core_ms()+10000;ble_on=1;
#ifdef NP_LOADER_FM1
    /* The SDK normally does this in cfg_file_parse(), which this port skips.
     * Use the FM-1 peripheral reference's BLE level (SDK range 0..11). */
    bt_max_pwr_set(8,6,6,11);
#endif
    int result=le_controller_set_mac(ble_mac);
    if(result){np_error=(uint32_t)result;np_state=5;return;}
    ble_stage(3);ble_os_after_run=ble_irq_check;
    if(task_create(ble_app_core,0,"app_core")){np_error=3;np_state=5;}
}
void np_ble_diag_service(void){
    if(!ble_on)return;
    ble_os_service(5000);ble_diag.heap_high=ble_os_heap_high();ble_diag.alloc_fails=ble_os_alloc_fails();
    if(np_state==1&&(int32_t)(core_ms()-np_deadline)>=0){np_error=2;np_state=5;}
    if(np_tune_pending==1&&np_gatt.connected&&(int32_t)(core_ms()-np_tune_due)>=0){
        np_tune_pending=2;
        (void)ble_user_cmd_prepare(11,2,(int)np_gatt.handle,(void *)np_fast_connection);
        /* ble_api.h: BLE_CMD_SET_DATA_LENGTH=20; 251 octets at 1M PHY. */
        (void)ble_user_cmd_prepare(20,3,(int)np_gatt.handle,251,2120);
    }
    if(!np_advertise||np_gatt.connected||np_state==5)return;
    np_advertise=0;
    int result=ble_user_cmd_prepare(2,3,160,0,7);
    if(!result)result=ble_user_cmd_prepare(3,2,(int)np_diag_adv_size,(void *)np_diag_adv);
    if(!result)result=ble_user_cmd_prepare(4,2,(int)np_diag_rsp_size,(void *)np_diag_rsp);
    if(!result)result=ble_user_cmd_prepare(1,1,1);
    if(result){np_error=(uint32_t)result;np_state=5;}else np_state=3;
}
