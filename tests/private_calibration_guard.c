#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "../experiments/bluetooth/private_calibration_guard.h"
static uint8_t live[NP_PRIVATE_FACTORY_BYTES],backup[NP_PRIVATE_FACTORY_BYTES];
static unsigned calls,fail_at;
static int read_factory(void *ctx,uint32_t off,uint8_t *dst,uint32_t n){
    assert(ctx==live&&off>=0xc0000u&&off<0xc2000u&&n==128u&&n<=0xc2000u-off);
    if(++calls==fail_at)return -1;
    memcpy(dst,live+off-0xc0000u,n);return 0;
}
static int check(np_factory_report *r){calls=0;return np_private_unit_matches(read_factory,live,backup,sizeof backup,r);}
int main(void){
    for(unsigned i=0;i<sizeof backup;i++)backup[i]=(uint8_t)(i*7);
    memcpy(backup+4096+548,"trimdat",8);
    for(unsigned k=0;k<37;k++)for(unsigned j=0;j<5;j++){
        unsigned at=4096+168+k*10+j*2,v=5+j*200;backup[at]=(uint8_t)v;backup[at+1]=(uint8_t)(v>>8);
    }
    memcpy(live,backup,sizeof live);np_factory_report r;
    assert(check(&r)&&calls==64&&r.full_match&&r.unit_match&&r.first_difference==UINT32_MAX);
    for(unsigned i=0;i<sizeof live;i++){
        live[i]^=1;int unit=i>=4096&&i<4096+556;
        assert(check(&r)==!unit&&!r.full_match&&r.differences==1&&r.first_difference==0xc0000+i);live[i]^=1;
    }
    fail_at=32;assert(!check(&r)&&r.read_failed&&!r.unit_match);fail_at=0;
    calls=0;assert(!np_private_unit_matches(read_factory,live,backup,sizeof backup-1,&r)&&!calls);
    assert(!np_private_unit_matches(0,live,backup,sizeof backup,&r));
    assert(!np_private_unit_matches(read_factory,live,0,sizeof backup,&r));
    backup[4096+548]=0;calls=0;assert(!check(&r)&&!calls);
    puts("PASS: exact validated usrtrim match, every-byte differences, unrelated sectors, failed reads and bounds");
}
