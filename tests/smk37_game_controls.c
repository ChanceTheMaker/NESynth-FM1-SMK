#include <assert.h>
#include <stdio.h>
#include "smk37_game_controls.h"
#include "../firmware/smk37_game_pads.h"
#include "../firmware/smk37_volume.h"
int main(void){
    assert(gp_smk37_keys(1u<<5,0)==GP_LEFT);
    assert(gp_smk37_keys(1u<<9,0)==GP_DOWN);
    assert(gp_smk37_keys(1u<<12,0)==GP_RIGHT);
    assert(gp_smk37_keys(1u<<7,0)==GP_LEFT);
    assert(gp_smk37_keys(1u<<11,0)==GP_RIGHT);
    assert(gp_smk37_keys((1u<<5)|(1u<<7),0)==GP_LEFT);
    assert(gp_smk37_keys((1u<<11)|(1u<<12),0)==GP_RIGHT);
    assert(gp_smk37_keys((1u<<7)|(1u<<9),0)==(GP_LEFT|GP_DOWN));
    assert(gp_smk37_keys((1u<<11)|(1u<<9),0)==(GP_RIGHT|GP_DOWN));
    assert(gp_smk37_keys(1u<<8,0)==GP_UP&&gp_smk37_keys(1u<<10,0)==GP_UP);
    assert(gp_smk37_keys(0,1u<<3)==GP_B&&gp_smk37_keys(0,1u<<4)==GP_A);
    assert(gp_smk37_keys(1u<<6,1u<<1)==0);
    const unsigned pads[]={8,9,10,1,14,15},bits[]={GP_LEFT,GP_DOWN,GP_RIGHT,GP_UP,GP_B,GP_A};
    for(unsigned i=0;i<6;i++)assert(gp_smk37_pads(1u<<pads[i])==bits[i]);
    assert(gp_smk37_pad_lights()==0xc702);
    assert(gp_smk37_panel(1u<<16)==GP_SELECT&&gp_smk37_panel(1u<<17)==GP_START);
    gb_smk37_pads_t s={0};
    gb_smk37_pad_sample(&s,8,500);gb_smk37_pad_sample(&s,8,0);
    assert(!s.held&&gb_smk37_pads_take(&s)==(1u<<8)); /* short tap survives */
    assert(gb_smk37_pads_take(&s)==0);
    for(unsigned i=0;i<4;i++)gb_smk37_pad_sample(&s,15,600);
    assert(gb_smk37_pads_take(&s)==(1u<<15)&&gb_smk37_pads_take(&s)==(1u<<15));
    gb_smk37_pad_sample(&s,15,0);assert(gb_smk37_pads_take(&s)==0);
    uint32_t previous=0;
    for(unsigned raw=0;raw<=1023;raw++){
        uint32_t gain=smk_volume_sample(64,21,raw);
        assert(gain>=previous&&gain<=256);previous=gain;
        if(raw<=8)assert(gain==0);
        if(raw>=1015)assert(gain==256);
    }
    for(unsigned input=0;input<24;input++)if(input!=21)
        assert(smk_volume_sample(64,input,1023)==64); /* Pads/wheels/other faders cannot change volume. */
    assert(smk_volume_sample(64,21,65535)==256);
    int16_t pcm[512];int32_t output[514];
    for(unsigned i=0;i<512;i++)pcm[i]=(i&1)?-32768:32767;
    output[0]=123;output[513]=456;
    smk_volume_render(output+1,pcm,0,256);
    assert(output[0]==123&&output[513]==456);
    for(unsigned i=0;i<256;i++){
        assert(output[1+i*2]==32767*(int32_t)(i+1));
        assert(output[2+i*2]==-32768*(int32_t)(i+1));
    }
    smk_volume_render(output+1,pcm,256,0);
    assert(output[511]==0&&output[512]==0);
    smk_volume_render(output+1,pcm,0,0);
    for(unsigned i=1;i<=512;i++)assert(output[i]==0);
    smk_volume_render(output+1,pcm,64,64);
    assert(output[1]==32767*64&&output[2]==-32768*64);
    puts("PASS: Fader 1 isolation, monotonic volume, mute/full endpoints, stereo ramp, signed 24-bit bounds and startup level.");
    puts("PASS: SMK37 white/black/high keys, pad rows, lighting mask, PLAY/REC, short taps and held/released pads.");
}
