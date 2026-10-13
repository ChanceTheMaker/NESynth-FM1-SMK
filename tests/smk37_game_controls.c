#include <assert.h>
#include <stdio.h>
#include "smk37_game_controls.h"
#include "../firmware/smk37_game_pads.h"
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
    assert(gp_smk37_keys(0,1u<<1)==GP_A&&gp_smk37_keys(0,1u<<4)==GP_B);
    assert(gp_smk37_keys(1u<<6,1u<<3)==0);
    const unsigned pads[]={8,9,10,1,14,15},bits[]={GP_LEFT,GP_DOWN,GP_RIGHT,GP_UP,GP_B,GP_A};
    for(unsigned i=0;i<6;i++)assert(gp_smk37_pads(1u<<pads[i])==bits[i]);
    assert(gp_smk37_pad_lights()==0xc702);
    assert(gp_smk37_panel(1u<<16)==GP_START&&gp_smk37_panel(1u<<17)==GP_SELECT);
    gb_smk37_pads_t s={0};
    gb_smk37_pad_sample(&s,8,500);gb_smk37_pad_sample(&s,8,0);
    assert(!s.held&&gb_smk37_pads_take(&s)==(1u<<8)); /* short tap survives */
    assert(gb_smk37_pads_take(&s)==0);
    for(unsigned i=0;i<4;i++)gb_smk37_pad_sample(&s,15,600);
    assert(gb_smk37_pads_take(&s)==(1u<<15)&&gb_smk37_pads_take(&s)==(1u<<15));
    gb_smk37_pad_sample(&s,15,0);assert(gb_smk37_pads_take(&s)==0);
    puts("PASS: SMK37 white/black/high keys, pad rows, lighting mask, PLAY/REC, short taps and held/released pads.");
}
