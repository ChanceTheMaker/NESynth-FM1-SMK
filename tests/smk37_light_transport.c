#include <assert.h>
#include <stdint.h>
#include <stdio.h>
static uint32_t regs[0x52000/4];
static unsigned resets,barriers,polls,complete_after;
static void reset_gap(void){
    assert(!(regs[0x11d00/4]&1));
    assert(!(regs[0x50080/4]&(1u<<10))&&!(regs[0x50088/4]&(1u<<10)));
    assert(regs[0x50080/4]&(1u<<8));
    resets++;
}
static void barrier(void){assert(resets==1);barriers++;}
static void poll(void){
    assert(resets==1&&barriers==1);
    assert(regs[0x51020/4]==0x20010&&regs[0x11d04/4]==8&&regs[0x11d10/4]==420);
    assert(regs[0x50088/4]&(1u<<9));
    if(++polls==complete_after)regs[0x11d00/4]|=0x8000;
}
#define SMK37_LIGHT_MAX_LEVEL 255u
#define SMK37_LIGHT_REG(a) regs[(a)/4]
#define SMK37_LIGHT_RESET() reset_gap()
#define SMK37_LIGHT_DMA_BARRIER() barrier()
#define SMK37_LIGHT_POLL() poll()
#include "../vendor/felucca/firmware/hal/smk37_lights_hw.h"
static unsigned decode(const uint8_t *p){unsigned v=0;for(unsigned i=0;i<24;i++){assert(p[i]==0xc0||p[i]==0xf0);v=(v<<1)|(p[i]==0xf0);}return v;}
int main(void){
    uint8_t data[SMK37_LIGHT_BYTES];uint32_t rgb[16];
    for(unsigned level=0;level<256;level++)for(unsigned held=0;held<16;held++){
        for(unsigned p=0;p<16;p++)rgb[p]=p==held?0:p>=14?(level<<16)|level:((level*2/3)<<8)|level;
        smk37_light_encode_rgb(data,rgb);
        for(unsigned p=0;p<16;p++){unsigned c=rgb[p],wire=((c>>8)&255)<<16|((c>>16)&255)<<8|(c&255);assert(decode(data+36+24*smk37_pad_light_order[p])==wire);}
    }
    for(unsigned failed=0;failed<2;failed++){
        regs[0x11d00/4]=0x4021;regs[0x11d04/4]=4;regs[0x51020/4]=0x20000;
        regs[0x50080/4]=0x1400;regs[0x50088/4]=0x1600;regs[0x51000/4]=0xabcdef;
        resets=barriers=polls=0;complete_after=failed?0:3;
        assert(smk37_lights_send(data,7)==!failed);
        assert(resets==2&&barriers==1&&polls==(failed?7:3));
        assert(regs[0x11d00/4]==0x4021&&regs[0x11d04/4]==4&&regs[0x51020/4]==0x20000);
        assert(regs[0x50080/4]==0x1500&&regs[0x50088/4]==0x1600&&regs[0x51000/4]==0xabcdef);
    }
    puts("PASS: all brightness/held-pad wire colors, reset framing, DMA publication, LCD restore and timeout cleanup");
}
