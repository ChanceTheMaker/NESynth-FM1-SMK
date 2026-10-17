#include <assert.h>
#include <stdio.h>
#include "../firmware/smk37_brightness.h"
int main(void){
    smk_brightness_t s={0};unsigned level=150;
    for(unsigned input=0;input<24;input++)if(input!=18){
        assert(smk_brightness_sample(&s,level,input,0)==150);
        assert(smk_brightness_sample(&s,level,input,1023)==150);
        assert(!s.ready);
    }
    assert(smk_brightness_sample(&s,level,18,512)==150&&s.ready&&!s.active);
    assert(smk_brightness_sample(&s,level,18,519)==150&&!s.active);
    level=smk_brightness_sample(&s,level,18,520);assert(s.active&&level<150);
    assert(smk_brightness_sample(&s,level,18,521)==level);
    level=smk_brightness_sample(&s,level,18,0);assert(level==0);
    for(unsigned raw=1;raw<=1023;raw++){
        unsigned next=smk_brightness_sample(&s,level,18,raw);
        assert(next>=level&&next<=255);level=next;
    }
    assert(level==255);
    for(unsigned raw=1023;raw;){
        unsigned next=smk_brightness_sample(&s,level,18,--raw);
        assert(next<=level);level=next;
    }
    assert(!level);
    puts("PASS: Fader 2 isolation, startup 150 until movement, noise deadband, monotonic range and off/full endpoints");
}
