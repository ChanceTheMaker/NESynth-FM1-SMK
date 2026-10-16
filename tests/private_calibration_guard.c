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
int main(void){
    for(unsigned i=0;i<sizeof live;i++)live[i]=backup[i]=(uint8_t)(i*7);
    assert(np_private_factory_matches(read_factory,live,backup,sizeof backup)&&calls==64);
    for(unsigned i=0;i<sizeof live;i+=127){
        live[i]^=1;calls=0;assert(!np_private_factory_matches(read_factory,live,backup,sizeof backup));live[i]^=1;
    }
    live[8191]^=1;assert(!np_private_factory_matches(read_factory,live,backup,sizeof backup));live[8191]^=1;
    calls=0;fail_at=32;assert(!np_private_factory_matches(read_factory,live,backup,sizeof backup));
    calls=0;assert(!np_private_factory_matches(read_factory,live,backup,sizeof backup-1)&&!calls);
    assert(!np_private_factory_matches(0,live,backup,sizeof backup));
    assert(!np_private_factory_matches(read_factory,live,0,sizeof backup));
    puts("PASS: exact private factory match, mismatched device, failed reads and bounded region");
}
