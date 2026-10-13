/* Regression cases for replayed audio buffers and notes that never decay. */
#include "InfoNES_pAPU.h"
extern BYTE wave_buffers[5][735],ApuCtrl,ApuCtrlNew;
extern int cur_event;
extern unsigned int ApuSamplesPerSync;
extern ApuEvent_t ApuEventQueue[];
extern void ApuRenderingWave1(),ApuRenderingWave2(),ApuRenderingWave3(),ApuRenderingWave5();
static bool wave_sounds(unsigned c){for(unsigned i=0;i<ApuSamplesPerSync;i++)if(wave_buffers[c][i])return true;return false;}
static void apu_write(unsigned reg,BYTE value){pAPUSoundRegs[reg](0x4000+reg,value);}
static void test_apu_release(){
    for(unsigned c=0;c<2;c++){
        InfoNES_pAPUInit();ApuCtrl=(BYTE)(1<<c);
        apu_write(c*4,0xbf);apu_write(c*4+2,0x40);apu_write(c*4+3,8);
        auto render=c?ApuRenderingWave2:ApuRenderingWave1;
        render();cur_event=0;assert(wave_sounds(c));
        apu_write(c*4+2,0);apu_write(c*4+3,0);render();cur_event=0;
        assert(!wave_sounds(c)); // Entire buffer clears at a muted timer.
        apu_write(c*4+2,0x40);ApuEventQueue[0].time=10000;render();cur_event=0;
        assert(wave_sounds(c)); // A later write survives an initially muted channel.
        // Constant-volume zero means silence, even when the envelope is nonzero.
        apu_write(c*4,0xb0);render();cur_event=0;assert(!wave_sounds(c));
        // One-shot envelope reaches zero while a long length counter remains live.
        apu_write(c*4,0x80);apu_write(c*4+3,8);render();cur_event=0;assert(wave_sounds(c));
        for(unsigned frame=0;frame<7;frame++){render();cur_event=0;}
        assert(!wave_sounds(c));
        // Retriggering the length register restarts the decayed envelope.
        apu_write(c*4+3,8);render();cur_event=0;assert(wave_sounds(c));
    }
    InfoNES_pAPUInit();ApuCtrl=4;
    apu_write(8,3);apu_write(10,0x40);apu_write(11,8);
    ApuRenderingWave3();cur_event=0;assert(wave_sounds(2));
    for(unsigned frame=0;frame<3;frame++)ApuRenderingWave3();
    assert(!wave_sounds(2)); // Triangle linear counter expires without a reload.
    memset(wave_buffers[4],123,735);ApuCtrl=0;ApuRenderingWave5();assert(!wave_sounds(4));
    puts("PASS: pulse mute/resume, constant-volume zero, envelope expiry/retrigger, triangle release and stale DMC clearing.");
}
