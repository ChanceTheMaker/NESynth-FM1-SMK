#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "../experiments/bluetooth/diagnostic_gatt.h"
int main(void){
    np_diag_gatt s={0};uint8_t bytes[18],echo[]={1,2,3,4,5,6,7,8};
    assert(np_diag_write(&s,0,6,0,0,echo,8)==3);
    np_diag_connect(&s,0); /* handle zero is valid */
    assert(np_diag_read(&s,0,3,0,0,0)==strlen(NP_BLE_NAME));
    assert(np_diag_write(&s,1,6,0,0,echo,8)==3);
    assert(np_diag_write(&s,0,3,0,0,echo,8)==3);
    assert(np_diag_write(&s,0,6,1,0,echo,8)==6);
    assert(np_diag_write(&s,0,6,0,1,echo,8)==7);
    for(unsigned i=0;i<8;i++)assert(np_diag_write(&s,0,6,0,0,echo,i)==13);
    assert(np_diag_write(&s,0,6,0,0,0,8)==13&&!s.writes);
    assert(np_diag_write(&s,0,6,0,0,echo,8)==0);
    memset(bytes,0xa5,sizeof bytes);assert(np_diag_read(&s,0,6,0,bytes+1,16)==16);
    assert(bytes[0]==0xa5&&bytes[17]==0xa5&&!memcmp(bytes+1,"NPB1",4)&&bytes[5]==1&&!memcmp(bytes+9,echo,8));
    assert(np_diag_read(&s,0,6,16,bytes,1)==0);
    assert(np_diag_read(&s,0,6,15,bytes,18)==1&&bytes[0]==8);
    np_diag_disconnect(&s,1);assert(s.connected);np_diag_disconnect(&s,0);assert(!s.connected&&!s.writes);
    np_diag_connect(&s,17);assert(!s.writes&&!s.echo[0]);
    assert(np_diag_adv_size<=31&&np_diag_rsp_size<=31&&np_diag_rsp[0]+1u==np_diag_rsp_size);
    np_diag_disconnect(&s,17);
    uint8_t connect[33]={0x3e,19,1,0,0,0},disconnect[6]={5,4,0,0,0,19};
    for(unsigned n=0;n<20;n++)assert(!np_diag_event(&s,4,connect,n,1)&&!s.connected);
    assert(!np_diag_event(&s,4,connect,20,0)&&!s.connected);
    assert(np_diag_event(&s,4,connect,20,1)==1&&s.connected&&s.handle==0);
    assert(np_diag_write(&s,0,6,0,0,echo,8)==0);
    assert(np_diag_event(&s,4,connect,21,1)==0&&s.writes==1);
    assert(!np_diag_event(&s,4,disconnect,5,0)&&s.connected);
    assert(np_diag_event(&s,4,disconnect,5,1)==2&&!s.connected);
    connect[1]=31;connect[2]=10;assert(np_diag_event(&s,4,connect,32,1)==1);
    disconnect[2]=1;assert(!np_diag_event(&s,4,disconnect,6,1)&&s.connected);
    disconnect[2]=0;assert(np_diag_event(&s,4,disconnect,6,1)==2);
    connect[3]=1;assert(np_diag_event(&s,4,connect,33,1)==3&&!s.connected);
    puts("PASS: diagnostic ATT bounds, write validation, echo, disconnect and advertising sizes");
}
