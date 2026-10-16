#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include "../experiments/bluetooth/diagnostic_gatt.h"
typedef void (*btstack_packet_handler_t)(uint8_t,uint16_t,uint8_t *,uint16_t);
struct bt_event {uint8_t event,args[7];uint32_t value;};
#define BLE_SDK_ATT_CTRL_BLOCK_SIZE 188
#define BLE_SDK_BT_EVENT_FROM_CON 3
#define BLE_SDK_BT_STATUS_INIT_OK 3
#define BLE_HEAP_BYTES 13824u
static uint8_t ble_heap[BLE_HEAP_BYTES],ble_on,ble_mac[6];
static struct {uint32_t task,heap_high,alloc_fails;} ble_diag;
static uint32_t *ble_os_trace;
static void (*ble_os_after_run)(int);
static unsigned rf_ok,os_inits,tasks,commands,last_cmd,now,fail_cmd,stage;
static btstack_packet_handler_t controller_cb,att_cb;
static int ble_rf_trim_load(void){return rf_ok;}
static uint32_t fm1_ticks(void){return 123456;}
static uint32_t core_ms(void){return now;}
#ifdef NP_LOADER_FM1
static unsigned power_inits;
void bt_max_pwr_set(uint8_t a,uint8_t b,uint8_t c,uint8_t d){assert(a==8&&b==6&&c==6&&d==11&&!tasks);power_inits++;}
#endif
static void ble_os_init(uint8_t *p,unsigned n){assert(rf_ok&&p==ble_heap&&n==BLE_HEAP_BYTES);os_inits++;}
static void ble_delay_calibrate(void){}
static void ble_stage(unsigned n){stage=n;}
static void core_ble_stack_window(void){}
static int le_controller_set_mac(void *p){assert(p==ble_mac);assert((ble_mac[0]&0xc0)==0xc0&&(ble_mac[5]&0xc0)==0xc0);
#ifdef NP_LOADER_FM1
assert(power_inits==1);
#endif
return 0;}
static void ble_irq_check(int t){(void)t;}
static void ble_app_core(void *p){(void)p;}
static int task_create(void (*f)(void *),void *p,const char *name){assert(f==ble_app_core&&!p&&name);tasks++;return 0;}
static void ble_os_service(unsigned n){assert(n==5000);}
static unsigned ble_os_heap_high(void){return 1024;}
static unsigned ble_os_alloc_fails(void){return 0;}
static void le_device_db_init(void){}
static void ble_stack_gatt_role(uint8_t r){assert(r==0);}
static void sm_init(void){}
static void sm_set_io_capabilities(int n){assert(n==3);}
static void sm_set_authentication_requirements(uint8_t n){assert(!n);}
static void sm_set_encryption_key_size_range(uint8_t a,uint8_t b){assert(a==7&&b==16);}
void sm_set_request_security(int n){assert(!n);}
void att_server_init(const uint8_t *db,uint16_t (*read)(uint16_t,uint16_t,uint16_t,uint8_t *,uint16_t),int (*write)(uint16_t,uint16_t,uint16_t,uint16_t,uint8_t *,uint16_t)){assert(db==np_diag_db&&read&&write);}
void att_server_register_packet_handler(btstack_packet_handler_t cb){att_cb=cb;}
static void hci_event_callback_set(btstack_packet_handler_t cb){controller_cb=cb;}
static void le_l2cap_register_packet_handler(btstack_packet_handler_t cb){assert(cb==controller_cb);}
static uint16_t ble_vendor_set_default_att_mtu(uint16_t n){assert(n==247);return n;}
static int ble_user_cmd_prepare(int cmd,int argc,...){
    assert((cmd==1&&argc==1)||(cmd==2&&argc==3)||((cmd==3||cmd==4)&&argc==2)||(cmd==7&&argc==4)||(cmd==5&&argc==1)||(cmd==11&&argc==2)||(cmd==20&&argc==3));
    if(cmd==20){va_list a;va_start(a,argc);assert(va_arg(a,int)==0);assert(va_arg(a,int)==251);assert(va_arg(a,int)==2120);va_end(a);}
    if(cmd==11){va_list a;va_start(a,argc);assert(va_arg(a,int)==0);const uint16_t *p=va_arg(a,void *);assert(p[0]==12&&p[1]==24&&p[2]==0&&p[3]==400);va_end(a);}
    commands++;last_cmd=(unsigned)cmd;return (unsigned)cmd==fail_cmd?-100:0;
}
#define ble_op_att_send_init(h,p,n,m) ble_user_cmd_prepare(7,4,(int)(h),(p),(int)(n),(int)(m))
#define ble_op_disconnect(h) ble_user_cmd_prepare(5,1,(int)(h))
void np_loader_connected(void){}
int np_loader_write(const uint8_t *p,unsigned n){(void)p;return n==1?0:13;}
unsigned np_loader_read(uint16_t off,uint8_t *p,uint16_t n){(void)off;(void)p;(void)n;return 20;}
#include "../experiments/bluetooth/loader_peripheral.c"
int main(void){
    np_ble_diag_service();assert(!commands&&!os_inits);
    np_ble_diag_start();assert(np_state==5&&np_error==1&&!tasks&&!os_inits);
    np_state=np_error=0;rf_ok=1;np_ble_diag_start();assert(np_state==1&&tasks==1&&os_inits==1&&stage==3);
    np_ble_diag_start();assert(tasks==1);ble_profile_init();assert(stage==6&&att_cb&&controller_cb);
    struct bt_event e={3,{0},0};ble_on_event(3,&e);assert(np_state==2&&!commands);
    np_ble_diag_service();assert(np_state==3&&commands==4&&last_cmd==1);
    np_ble_diag_service();assert(commands==4);
    uint8_t packet[21]={0x3e,19,1,0,0,0};controller_cb(4,0,packet,20);assert(np_state==4&&commands==5&&last_cmd==7);
    np_gatt.writes=9;controller_cb(4,0,packet,20);assert(commands==5&&np_gatt.writes==9);
    now=5000;np_ble_diag_service();assert(commands==5); /* Never tune during service discovery. */
    (void)np_read(0,3,0,0,0);np_ble_diag_service();assert(commands==5); /* GAP name is not application readiness. */
    assert(np_read(0,6,0,0,0)==20);
    now=5099;np_ble_diag_service();assert(commands==5);
    now=5100;fail_cmd=11;np_ble_diag_service();assert(commands==7&&last_cmd==20&&np_state==4);
    np_ble_diag_service();assert(commands==7); /* optional tuning refusal is not fatal or retried */
    uint8_t disconnected[5]={5,4,0,0,0};controller_cb(4,0,disconnected,5);assert(np_state==2&&commands==8&&!np_tune_pending);
    fail_cmd=0;np_ble_diag_service();assert(np_state==3&&commands==12);
    controller_cb(4,0,packet,20);assert(np_state==4&&commands==13);
    controller_cb(4,0,disconnected,5);assert(commands==14);
    now=2000;np_ble_diag_service();assert(np_state==3&&commands==18&&last_cmd==1); /* no stale request after disconnect */
    puts("PASS: loader connection tuning is deferred, bounded, disconnect-safe and nonfatal when refused; MTU247 and handle zero accepted");
}
